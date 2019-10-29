/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "mpc.h"
#include <zlib.h> // For crc32.

// Minor planets module

typedef struct orbit_t {
    float d;    // date (julian day).
    float i;    // inclination (rad).
    float o;    // Longitude of the Ascending Node (rad).
    float w;    // Argument of Perihelion (rad).
    float a;    // Mean distance (Semi major axis).
    float n;    // Daily motion (rad/day).
    float e;    // Eccentricity.
    float m;    // Mean Anomaly (rad).
} orbit_t;

/*
 * Type: mplanet_t
 * Object that represents a single minor planet.
 */
typedef struct {
    obj_t       obj;
    orbit_t     orbit;
    float       h;      // Absolute magnitude.
    float       g;      // Slope parameter.
    char        name[24];
    char        desig[24];  // Principal designation.
    int         mpl_number; // Minor planet number if one has been assigned.
    bool        on_screen;  // Set once the object has been visible.

    // Cached values.
    float       vmag;
    double      pvo[2][4];
} mplanet_t;

/*
 * Type: mplanets_t
 * Minor planets module object
 */
typedef struct mplanets {
    obj_t   obj;
    int     update_pos; // Index of the position for iterative update.
    bool    visible;
} mplanets_t;


static uint64_t compute_oid(int number, const char desig[static 22])
{
    if (number) return oid_create("MPl", number);
    // No number, default to the crc32 of the designation.
    return oid_create("MPl*", crc32(0L, (const Bytef*)desig, 22));
}


/*
 * Compute an asteroid observed magnitude from its H, G and positions.
 * http://www.britastro.org/asteroids/dymock4.pdf
 */
static double compute_magnitude(double h, double g,
                                const double ph[3],
                                const double po[3])
{
    double alpha;   // Phase angle (sun/asteroid/earth)
    double r;       // distance asteroid / sun (AU)
    double delta;   // distance asteroid / earth (AU)
    double phi1, phi2, ha;
    r = vec3_norm(ph);
    delta = vec3_norm(po);
    alpha = eraSepp(ph, po);
    phi1 = exp(-3.33 * pow(tan(0.5 * alpha), 0.63));
    phi2 = exp(-1.87 * pow(tan(0.5 * alpha), 1.22));
    ha = h - 2.5 * log10((1 - g) * phi1 + g * phi2);
    return ha + 5 * log10(r * delta);
}

// Match minor planet center orbit type number to otype.
static const char *ORBIT_TYPES[] = {
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


static void load_data(mplanets_t *mplanets, const char *data, int size)
{
    const char *line = NULL;
    int r, len, line_idx = 0, flags, orbit_type, number, nb_err, nb;
    char desig[24], name[24];
    double h, g, m, w, o, i, e, n, a, epoch;
    mplanet_t *mplanet;
    obj_t *tmp;

    line_idx = 0;
    nb_err = 0;
    while (iter_lines(data, size, &line, &len)) {
        line_idx++;
        if (len < 160) continue;
        r = mpc_parse_line(line, len, &number, name, desig,
                           &h, &g, &epoch, &m, &w, &o, &i, &e,
                           &n, &a, &flags);
        if (r) {
            nb_err++;
            continue;
        }
        mplanet = (void*)module_add_new(&mplanets->obj, "asteroid", NULL, NULL);
        mplanet->orbit.d = epoch;
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
        strncpy(mplanet->obj.type, ORBIT_TYPES[orbit_type], 4);
        mplanet->mpl_number = number;
        mplanet->obj.oid = compute_oid(number, desig);
        if (name[0]) {
            _Static_assert(sizeof(name) == sizeof(mplanet->name), "");
            memcpy(mplanet->name, name, sizeof(name));
        }
        if (desig[0]) {
            _Static_assert(sizeof(desig) == sizeof(mplanet->desig), "");
            memcpy(mplanet->desig, desig, sizeof(desig));
        }
    }
    if (nb_err) {
        LOG_W("Minor planet data got %d errors lines.", nb_err);
    }
    DL_COUNT(mplanets->obj.children, tmp, nb);
    LOG_I("Parsed %d asteroids", nb);
}

static int mplanets_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    const char *data;
    int size, code;
    mplanets_t *mplanets = (void*)obj;
    if (strcmp(type, "mpc_asteroids") != 0) return 1;
    data = asset_get_data(url, &size, &code);
    if (!data) {
        LOG_W("Cannot read asteroids data (%s)", url);
        return 0;
    }
    load_data(mplanets, data, size);
    asset_release(url);
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
            mp->obj.oid = oid_create("MPl", num);
            mp->mpl_number = num;
        }
    }
    // XXX: use proper type.
    strncpy(mp->obj.type, "MBA", 4);
    return 0;
}

static int mplanet_update(mplanet_t *mp, const observer_t *obs)
{
    double pvh[2][3], pvo[2][3];

    orbit_compute_pv(0, obs->ut1, pvh[0], pvh[1],
            mp->orbit.d, mp->orbit.i, mp->orbit.o, mp->orbit.w,
            mp->orbit.a, mp->orbit.n, mp->orbit.e, mp->orbit.m,
            0, 0);

    mat3_mul_vec3(obs->re2i, pvh[0], pvh[0]);
    mat3_mul_vec3(obs->re2i, pvh[1], pvh[1]);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, pvh, pvo);
    vec3_copy(pvo[0], mp->pvo[0]);
    vec3_copy(pvo[1], mp->pvo[1]);
    mp->pvo[0][3] = 1.0; // AU unit.
    mp->pvo[1][3] = 1.0;

    // Compute vmag using algo from
    // http://www.britastro.org/asteroids/dymock4.pdf
    mp->vmag = compute_magnitude(mp->h, mp->g, pvh[0], pvo[0]);
    return 0;
}

static int mplanet_get_info(const obj_t *obj, const observer_t *obs, int info,
                            void *out)
{
    mplanet_t *mp = (mplanet_t*)obj;
    mplanet_update(mp, obs);
    switch (info) {
    case INFO_PVO:
        memcpy(out, mp->pvo, sizeof(mp->pvo));
        return 0;
    case INFO_VMAG:
        *(double*)out = mp->vmag;
        return 0;
    }
    return 1;
}

static int mplanet_render(const obj_t *obj, const painter_t *painter)
{
    double pvo[2][4], win_pos[2], vmag, size, luminance;
    double label_color[4] = RGBA(255, 124, 124, 255);
    mplanet_t *mplanet = (mplanet_t*)obj;
    point_t point;
    const bool selected = core->selection && obj->oid == core->selection->oid;

    mplanet_update(mplanet, painter->obs);
    vmag = mplanet->vmag;
    if (vmag > painter->stars_limit_mag) return 0;
    obj_get_pvo(obj, painter->obs, pvo);
    if (!painter_project(painter, FRAME_ICRF, pvo[0], false, true, win_pos))
        return 0;

    mplanet->on_screen = true;
    core_get_point_for_mag(vmag, &size, &luminance);

    point = (point_t) {
        .pos = {win_pos[0], win_pos[1]},
        .size = size,
        .color = {255, 255, 255, luminance * 255},
        .oid = obj->oid,
    };
    paint_2d_points(painter, 1, &point);

    // Render name if needed.
    if (*mplanet->name && (selected || vmag <= painter->hints_limit_mag)) {
        if (selected)
            vec4_set(label_color, 1, 1, 1, 1);
        labels_add_3d(mplanet->name, FRAME_ICRF, pvo[0], false, size,
                      FONT_SIZE_BASE, label_color, 0, 0,
                      selected ? TEXT_BOLD : TEXT_FLOAT,
                      0, obj->oid);
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
        snprintf(buf, sizeof(buf), "%d", mplanet->mpl_number);
        f(obj, user, "", buf);

        snprintf(buf, sizeof(buf), "(%d)", mplanet->mpl_number);
        f(obj, user, "MPC", buf);
    }
    if (*mplanet->name)  f(obj, user, "NAME", mplanet->name);
    if (*mplanet->desig) f(obj, user, "NAME", mplanet->desig);
}

static int mplanets_init(obj_t *obj, json_value *args)
{
    mplanets_t *mps = (void*)obj;
    mps->visible = true;
    return 0;
}

static bool range_contains(int range_start, int range_size, int nb, int i)
{
    if (i < range_start) i += nb;
    return i > range_start && i < range_start + range_size;
}

static int mplanets_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(mplanets_render, 0);

    mplanets_t *mps = (void*)obj;
    int i, nb;
    const int update_nb = 32;
    mplanet_t *child;
    obj_t *tmp;
    uint64_t selection_oid = core->selection ? core->selection->oid : 0;

    if (!mps->visible) return 0;
    DL_COUNT(obj->children, tmp, nb);

    /* To prevent spending too much time computing position of asteroids that
     * are not visible, we only update a small number of them at each
     * frame, using a moving range.  The asteroids who have been flagged as
     * on screen get updated no matter what.  */
    i = 0;
    MODULE_ITER(obj, child, NULL) {
        if (    !child->on_screen &&
                child->obj.oid != selection_oid &&
                !range_contains(mps->update_pos, update_nb, nb, i))
            continue;
        obj_render((obj_t*)child, painter);
    }
    mps->update_pos = nb ? (mps->update_pos + update_nb) % nb : 0;
    return 0;
}

static obj_t *mplanets_get_by_oid(
        const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (    !oid_is_catalog(oid, "MPl") &&
            !oid_is_catalog(oid, "MPl*")) return NULL;
    MODULE_ITER(obj, child, NULL) {
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
    .get_info   = mplanet_get_info,
    .render     = mplanet_render,
    .get_designations = mplanet_get_designations,
};
OBJ_REGISTER(mplanet_klass)

static obj_klass_t mplanets_klass = {
    .id             = "minor_planets",
    .size           = sizeof(mplanets_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE | OBJ_LISTABLE,
    .init           = mplanets_init,
    .add_data_source    = mplanets_add_data_source,
    .render         = mplanets_render,
    .get_by_oid     = mplanets_get_by_oid,
    .render_order   = 20,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(mplanets_t, visible)),
        {},
    },
};
OBJ_REGISTER(mplanets_klass)
