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

/*
 * Type: mplanet_t
 * Object that represents a single minor planet.
 */
typedef struct {
    obj_t       obj;
    orbit_t     orbit;
    double      h;      // Absolute magnitude.
    double      g;      // Slope parameter.
    char        name[32];
    int         mpl_number; // Minor planet number if one has been assigned.
} mplanet_t;

/*
 * Type: mplanets_t
 * Minor planets module object
 */
typedef struct mplanets {
    obj_t   obj;
} mplanets_t;

static int unpack_char(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return 10 + c - 'A';
    else if (c >= 'a' && c < 'z')
        return 36 + c - 'a';
    else {
        LOG_E("Cannot unpack char '%c'", c);
        return -1;
    }
}

// Unpack epoch field in MJD.
static double unpack_epoch(const char epoch[5])
{
    int year, month, day, r;
    double d1, d2;
    year = ((epoch[0] - 'I') + 18) * 100;
    year += (epoch[1] - '0') * 10;
    year += (epoch[2] - '0');
    month = unpack_char(epoch[3]);
    day = unpack_char(epoch[4]);
    r = eraDtf2d("", year, month, day, 0, 0, 0, &d1, &d2);
    if (r != 0) {
        LOG_E("Cannot parse epoch %.5s", epoch);
        return NAN;
    }
    return d1 - DJM0 + d2;
}

/*
 * Parse a 7 char packed designation
 *
 * The algo is described there:
 * https://www.minorplanetcenter.net/iau/info/PackedDes.html
 */
static int parse_designation(
        const char str[7], char type[4], bool *permanent)
{
    int ret;
    // 5 chars => permanent designations.
    if (strlen(str) == 5) {
        *permanent = true;
        // Minor planet.
        if (str[4] >= '0' && str[4] <= '9') {
            memcpy(type, "MPl ", 4);
            ret = atoi(str + 1) + 10000 * unpack_char(str[0]);
        }
        // Comet.
        else if (str[4] == 'P' || str[4] == 'D') {
            memcpy(type, "Com ", 4);
            ret = atoi(str);
        }
        // Natural satellites (not supported yet).
        else {
            LOG_W("MPC natural satellites not supported");
            memcpy(type, "    ", 4);
            ret = 0;
        }
    }
    // 7 char -> Provisional designation.
    // Not supported yet.
    else {
        *permanent = false;
        memcpy(type, "    ", 4);
        ret = 0;
    }
    return ret;
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

static uint64_t compute_oid(const char desgn[7])
{
    bool permanent;
    char type[4];
    int n;
    n = parse_designation(desgn, type, &permanent);
    if (permanent) return oid_create(type, n);
    return oid_create("MPl ", crc32(0L, (const Bytef*)desgn, 7));
}

static void load_data(mplanets_t *mplanets, const char *data) {
    int line, r, flags, orbit_type, number;
    char desgn[16] = {}, readable[32] = {}, epoch[5], type[4];
    double h = 0, g = 0;
    double m, w, o, i, e, n, a;
    bool permanent;
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

        mplanet = (void*)obj_create("asteroid", NULL,
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
        number = parse_designation(desgn, type, &permanent);
        if (permanent && strncmp(type, "MPl ", 4) == 0)
            mplanet->mpl_number = number;
        mplanet->obj.oid = compute_oid(desgn);

        r = regexec(&name_reg, readable, 3, matches, 0);
        if (!r) {
            strncpy(mplanet->name, readable + matches[2].rm_so,
                    matches[2].rm_eo - matches[2].rm_so);
            mplanet->name[matches[2].rm_eo - matches[2].rm_so] = '\0';
            identifiers_add("NAME", mplanet->name, mplanet->obj.oid,
                            0, "MPl ", 0, NULL, NULL);
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
    json_value *model;
    const char *name;
    int num;
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
    name = json_get_attr_s(args, "short_name");
    if (name) {
        strncpy(mp->name, name, sizeof(mp->name));
        if (sscanf(name, "(%d)", &num) == 1) {
            mp->obj.oid = oid_create("MPl ", num);
            mp->mpl_number = num;
        }
    }
    // XXX: use proper type.
    strncpy(mp->obj.type, "MBA", 4);
    return 0;
}

static int mplanet_update(obj_t *obj, const observer_t *obs, double dt)
{
    double ph[2][3], po[2][3];
    mplanet_t *mp = (mplanet_t*)obj;

    orbit_compute_pv(0, obs->ut1, ph[0], ph[1],
            mp->orbit.d, mp->orbit.i, mp->orbit.o, mp->orbit.w,
            mp->orbit.a, mp->orbit.n, mp->orbit.e, mp->orbit.m,
            mp->orbit.od, mp->orbit.wd);

    mat3_mul_vec3(obs->re2i, ph[0], ph[0]);
    mat3_mul_vec3(obs->re2i, ph[1], ph[1]);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, ph, po);
    vec3_copy(po[0], obj->pvo[0]);
    vec3_copy(po[1], obj->pvo[1]);
    obj->pvo[0][3] = 1.0; // AU unit.
    obj->pvo[1][3] = 1.0;
    // Compute vmag.
    // XXX: move this into algo.
    // http://www.britastro.org/asteroids/dymock4.pdf
    double alpha;   // Phase angle (sun/asteroid/earth)
    double r;       // distance asteroid / sun (AU)
    double delta;   // distance asteroid / earth (AU)
    double phi1, phi2, ha, h, g;

    h = mp->h;
    g = mp->g;
    r = vec3_norm(ph[0]);
    delta = vec3_norm(po[0]);
    alpha = eraSepp(ph[0], po[0]);
    phi1 = exp(-3.33 * pow(tan(0.5 * alpha), 0.63));
    phi2 = exp(-1.87 * pow(tan(0.5 * alpha), 1.22));
    ha = h - 2.5 * log10((1 - g) * phi1 + g * phi2);
    mp->obj.vmag = ha + 5 * log10(r * delta);

    return 0;
}

static int mplanet_render(const obj_t *obj, const painter_t *painter)
{
    double pos[4], vmag, size, luminance;
    double label_color[4] = RGBA(255, 124, 124, 255);
    mplanet_t *mplanet = (mplanet_t*)obj;
    point_t point;

    vmag = mplanet->obj.vmag;
    if (vmag > painter->mag_max) return 0;
    obj_get_pos_observed(obj, painter->obs, pos);
    if ((painter->flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return 0;
    vec3_normalize(pos, pos);
    core_get_point_for_mag(vmag, &size, &luminance);

    point = (point_t) {
        .pos = {pos[0], pos[1], pos[2], 0},
        .size = size,
        .color = {1, 1, 1, luminance},
        .oid = obj->oid,
    };
    paint_points(painter, 1, &point, FRAME_OBSERVED);

    // Render name if needed.
    if (*mplanet->name && vmag <= painter->label_mag_max) {
        mat3_mul_vec3(core->observer->ro2v, pos, pos);
        if (project(painter->proj,
                    PROJ_ALREADY_NORMALIZED | PROJ_TO_WINDOW_SPACE,
                    2, pos, pos)) {
            labels_add(mplanet->name, pos, size, 13, label_color, 0,
                       ANCHOR_AROUND, 0);
        }
    }
    return 0;
}

void mplanet_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    mplanet_t *mplanet = (mplanet_t*)obj;
    char buf[32];
    if (mplanet->mpl_number) {
        // Workaround so that we don't break stellarium web, that expect
        // to the an id that is simply the MPC object number!
        // Remove when we can.
        sprintf(buf, "%d", mplanet->mpl_number);
        f(obj, user, "", buf);

        sprintf(buf, "(%d)", mplanet->mpl_number);
        f(obj, user, "MPC", buf);
    }
    if (*mplanet->name) f(obj, user, "NAME", mplanet->name);
}

static int mplanets_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *child;
    OBJ_ITER(obj, child, "asteroid")
        obj_update(child, obs, dt);
    return 0;
}

static int mplanets_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, "asteroid")
        obj_render(child, painter);
    return 0;
}

static obj_t *mplanets_get_by_oid(
        const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (    !oid_is_catalog(oid, "MPl ") &&
            !oid_is_catalog(oid, "Com ")) return NULL;
    OBJ_ITER(obj, child, NULL) {
        if (child->oid == oid) {
            child->ref++;
            return child;
        }
    }
    return NULL;
}

/*
 * Meta class declarations.
 */

static obj_klass_t mplanet_klass = {
    .id         = "asteroid",
    .model      = "mpc_asteroid",
    .size       = sizeof(mplanet_t),
    .init       = mplanet_init,
    .update     = mplanet_update,
    .render     = mplanet_render,
    .get_designations = mplanet_get_designations,
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
        PROPERTY("vmag"),
        PROPERTY("type"),
        {},
    },
};
OBJ_REGISTER(mplanet_klass)

static obj_klass_t mplanets_klass = {
    .id             = "minor_planets",
    .size           = sizeof(mplanets_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = mplanets_init,
    .update         = mplanets_update,
    .render         = mplanets_render,
    .get_by_oid     = mplanets_get_by_oid,
    .render_order   = 20,
};
OBJ_REGISTER(mplanets_klass)
