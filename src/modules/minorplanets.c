/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <regex.h>
#include <zlib.h> // For crc32.

// Minor planets module

typedef struct orbit_t {
    double d;    // date (julian day).
    double i;    // inclination (rad).
    double o;    // Longitude of the Ascending Node (rad).
    double w;    // Argument of Perihelion (rad).
    double a;    // Mean distance (Semi major axis).
    double n;    // Daily motion (rad/day).
    double e;    // Eccentricity.
    double m;    // Mean Anomaly (rad).
    double od;   // variation of o in time.
    double wd;   // variation of w in time.
} orbit_t;

// A single minor planet.
typedef struct {
    obj_t       obj;
    orbit_t     orbit;
    double      h;      // Absolute magnitude.
    double      g;      // Slope parameter.
    char        name[32];
} mplanet_t;

static int mplanet_init(obj_t *obj, json_value *args);
static int mplanet_update(obj_t *obj, const observer_t *obs, double dt);
static int mplanet_render(const obj_t *obj, const painter_t *painter);

static obj_klass_t mplanet_klass = {
    .id         = "minor_planet",
    .size       = sizeof(mplanet_t),
    .init       = mplanet_init,
    .update     = mplanet_update,
    .render     = mplanet_render,
    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        PROPERTY("ra"),
        PROPERTY("dec"),
        PROPERTY("distance"),
        PROPERTY("alt"),
        PROPERTY("az"),
        PROPERTY("radec"),
        PROPERTY("azalt"),
        PROPERTY("rise"),
        PROPERTY("set"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {},
    },
};
OBJ_REGISTER(mplanet_klass)


// Planets module.

static int mplanets_init(obj_t *obj, json_value *args);
static int mplanets_update(obj_t *obj, const observer_t *obs, double dt);
static int mplanets_render(const obj_t *obj, const painter_t *painter);

typedef struct mplanets mplanets_t;
struct mplanets {
    obj_t   obj;
};


static obj_klass_t mplanets_klass = {
    .id             = "minor_planets",
    .size           = sizeof(mplanets_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = mplanets_init,
    .update         = mplanets_update,
    .render         = mplanets_render,
    .render_order   = 20,
};

OBJ_REGISTER(mplanets_klass)

static int unpack_number(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else
        return 10 + c - 'A';
}

// Unpack epoch field in MJD.
static double unpack_epoch(const char epoch[5])
{
    int year, month, day, r;
    double d1, d2;
    year = ((epoch[0] - 'I') + 18) * 100;
    year += (epoch[1] - '0') * 10;
    year += (epoch[2] - '0');
    month = unpack_number(epoch[3]);
    day = unpack_number(epoch[4]);
    r = eraDtf2d("", year, month, day, 0, 0, 0, &d1, &d2);
    if (r != 0) {
        LOG_E("Cannot parse epoch %.5s", epoch);
        return NAN;
    }
    return d1 - DJM0 + d2;
}

// Compute nsid following the same algo as noctuasky.
static uint64_t compute_nsid(const char *readable)
{
    uint64_t crc;
    // Remove spaces at the beginning.
    while (*readable == ' ') readable++;
    crc = crc32(0L, (const Bytef*)readable, strlen(readable));
    return (1ULL << 63) | (201326592ULL << 35) | (crc & 0xffffffff);
}

static void load_data(mplanets_t *mplanets, const char *data) {
    int line, r, flags, orbit_type;
    char desgn[16] = {}, readable[32] = {}, epoch[5];
    double h = 0, g = 0;
    double m, w, o, i, e, n, a;
    regex_t name_reg;
    mplanet_t *mplanet;
    regmatch_t matches[3];

    // Match minor planet center orbit type number to otype.
    const char *ORBIT_TYPES[] = {
       [ 0] = "MPl",
       [ 1] = "Ati",
       [ 2] = "Ate",
       [ 3] = "Apo",
       [ 4] = "Amo",
       [ 5] = "MPl", // ? Object with q < 1.665 AU
       [ 6] = "Hun",
       [ 7] = "Pho",
       [ 8] = "Hil",
       [ 9] = "JTA",
       [10] = "DOA",
    };

    const catalog_t CAT[] = {
        { 1,  7, "A7", "Desgn"},
        { 9, 13, "F5.2", "H"},
        {15, 19, "F5.2", "G"},
        {21, 25, "A5", "Epoch"},
        {27, 35, "F9.5", "M"},
        {38, 46, "F9.5", "Peri."},
        {49, 57, "F9.5", "Node"},
        {60, 68, "F9.5", "Incl."},
        {71, 79, "F9.7", "e"},
        {81, 91, "F11.8", "n"},
        {93, 103, "F11.7", "a"},
        {106, 106, "I1", "U"},
        {108, 116, "A9", "Reference"},
        {118, 122, "I5", "#Obs"},
        {124, 126, "I3", "#Opp"},
        {128, 136, "A8", "Arc"},
        {138, 141, "F4.2", "rms"},
        {143, 145, "A3", "Perts"},
        {147, 149, "A3", "Perts"},
        {151, 160, "A10", "Computer"},
        {162, 165, "Z4", "flags"},
        {167, 194, "A", "Readable"},
        {0}
    };

    regcomp(&name_reg, " *(\\(.+\\))? *(.+)", REG_EXTENDED | REG_ICASE);
    CATALOG_ITER(CAT, data, line, desgn, &h, &g, epoch, &m, &w,
                 &o, &i, &e, &n, &a, NULL, NULL, NULL, NULL,
                 NULL, NULL, NULL, NULL, NULL, &flags, readable) {
        assert(m >= 0 && m <= 360);
        assert(w >= 0 && w <= 360);
        assert(o >= 0 && o <= 360);
        assert(i >= 0 && i <= 360);
        assert(e >= 0 && e <= 1);
        str_rstrip(desgn);
        str_rstrip(readable);
        str_to_upper(desgn, desgn);

        mplanet = (void*)obj_create("minor_planet", desgn,
                                    &mplanets->obj, NULL);
        mplanet->orbit.d = unpack_epoch(epoch);
        mplanet->orbit.m = m * DD2R;
        mplanet->orbit.w = w * DD2R;
        mplanet->orbit.o = o * DD2R;
        mplanet->orbit.i = i * DD2R;
        mplanet->orbit.e = e;
        mplanet->orbit.n = n * DD2R;
        mplanet->orbit.a = a;
        mplanet->h = h;
        mplanet->g = g;

        orbit_type = flags & 0x3f;
        strcpy(mplanet->obj.type, ORBIT_TYPES[orbit_type]);
        mplanet->obj.nsid = compute_nsid(readable);

        r = regexec(&name_reg, readable, 3, matches, 0);
        if (!r) {
            strncpy(mplanet->name, readable + matches[2].rm_so,
                    matches[2].rm_eo - matches[2].rm_so);
            mplanet->name[matches[2].rm_eo - matches[2].rm_so] = '\0';
            identifiers_add(desgn, "NAME", mplanet->name, NULL, NULL);
        }
    }
    regfree(&name_reg);
}

static int mplanets_init(obj_t *obj, json_value *args)
{
    int size;
    const char *data;
    mplanets_t *mplanets = (void*)obj;
    data = asset_get_data("asset://mpcorb.dat", &size, NULL);
    load_data(mplanets, data);
    return 0;
}

static int mplanet_init(obj_t *obj, json_value *args)
{
    // Support creating a minor planet using noctuasky model data json values.
    mplanet_t *mp = (mplanet_t*)obj;
    orbit_t *orbit = &mp->orbit;
    json_value *model, *names;
    model = json_get_attr(args, "model_data", json_object);
    if (model) {
        mp->h = json_get_attr_f(model, "H", 0);
        mp->g = json_get_attr_f(model, "G", 0);
        orbit->d = json_get_attr_f(model, "Epoch", DJM0) - DJM0;
        orbit->i = json_get_attr_f(model, "i", 0) * DD2R;
        orbit->o = json_get_attr_f(model, "Node", 0) * DD2R;
        orbit->w = json_get_attr_f(model, "Peri", 0) * DD2R;
        orbit->a = json_get_attr_f(model, "a", 0);
        orbit->n = json_get_attr_f(model, "n", 0) * DD2R;
        orbit->e = json_get_attr_f(model, "e", 0);
        orbit->m = json_get_attr_f(model, "M", 0) * DD2R;
    }
    // XXX: should use a function to extract string items from json
    // array instead of this.
    if (        ((names = json_get_attr(args, "names", json_array)) &&
                    names->u.array.length >= 1)) {
        strncpy(mp->name, names->u.array.values[0]->u.string.ptr,
                sizeof(mp->name));
    }
    // XXX: use proper type.
    strncpy(mp->obj.type, "MBA", 4);
    return 0;
}

static int mplanet_update(obj_t *obj, const observer_t *obs, double dt)
{
    double ph[3], pg[3];
    mplanet_t *mp = (mplanet_t*)obj;

    orbit_compute_pv(obs->ut1, ph, NULL,
            mp->orbit.d, mp->orbit.i, mp->orbit.o, mp->orbit.w,
            mp->orbit.a, mp->orbit.n, mp->orbit.e, mp->orbit.m,
            mp->orbit.od, mp->orbit.wd);

    mat4_mul_vec3(obs->re2i, ph, ph);
    vec3_sub(ph, obs->earth_pvh[0], pg);
    vec3_copy(pg, obj->pos.pvg[0]);
    obj->pos.pvg[0][3] = 1.0; // AU unit.
    compute_coordinates(obs, obj->pos.pvg[0],
                        &obj->pos.ra, &obj->pos.dec,
                        &obj->pos.az, &obj->pos.alt);

    // Compute vmag.
    // XXX: move this into algo.
    // http://www.britastro.org/asteroids/dymock4.pdf
    double alpha;   // Phase angle (sun/asteroid/earth)
    double r;       // distance asteroid / sun (AU)
    double delta;   // distance asteroid / earth (AU)
    double phi1, phi2, ha, h, g;

    h = mp->h;
    g = mp->g;
    r = vec3_norm(ph);
    delta = vec3_norm(pg);
    alpha = eraSepp(ph, pg);
    phi1 = exp(-3.33 * pow(tan(0.5 * alpha), 0.63));
    phi2 = exp(-1.87 * pow(tan(0.5 * alpha), 1.22));
    ha = h - 2.5 * log10((1 - g) * phi1 + g * phi2);
    mp->obj.vmag = ha + 5 * log10(r * delta);

    return 0;
}

static int mplanet_render(const obj_t *obj, const painter_t *painter)
{
    double pos[4], mag, size, luminance;
    double label_color[4] = RGBA(255, 124, 124, 255);
    mplanet_t *mplanet = (mplanet_t*)obj;
    point_t point;

    if (mplanet->obj.vmag > painter->mag_max) return 0;
    obj_get_pos_observed(obj, painter->obs, pos);
    if ((painter->flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return 0;
    vec3_normalize(pos, pos);
    mag = core_get_observed_mag(mplanet->obj.vmag);
    core_get_point_for_mag(mag, &size, &luminance);

    point = (point_t) {
        .pos = {pos[0], pos[1], pos[2], 0},
        .size = size,
        .color = {1, 1, 1, luminance},
    };
    strcpy(point.id, obj->id);
    paint_points(painter, 1, &point, FRAME_OBSERVED);

    // Render name if needed.
    if (*mplanet->name && mplanet->obj.vmag <= painter->label_mag_max) {
        mat4_mul_vec3(core->observer->ro2v, pos, pos);
        if (project(painter->proj,
                    PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE,
                    2, pos, pos)) {
            labels_add(mplanet->name, pos, size, 13, label_color,
                ANCHOR_AROUND, 0);
        }
    }
    return 0;
}

static int mplanets_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *child;
    OBJ_ITER(obj, child, &mplanet_klass)
        obj_update(child, obs, dt);
    return 0;
}

static int mplanets_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, &mplanet_klass)
        obj_render(child, painter);
    return 0;
}
