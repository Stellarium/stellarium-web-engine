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
#include "designation.h"
#include <zlib.h> // For crc32.

// J2000 ecliptic to ICRF rotation matrix.
// Computed with 'eraEcm06(ERFA_DJ00, 0)'
static const double ECLIPTIC_ROT[3][3] = {
    { 1.000000000000, -0.000000070784, 0.000000080562},
    { 0.000000032897,  0.917482129915, 0.397776999444},
    {-0.000000102070, -0.397776999444, 0.917482129915},
};

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
typedef struct mplanet mplanet_t;
struct mplanet {
    obj_t       obj;
    orbit_t     orbit;
    float       h;      // Absolute magnitude.
    float       g;      // Slope parameter.
    char        name[24];
    char        desig[24];  // Principal designation.
    int         mpl_number; // Minor planet number if one has been assigned.
    char        model[64];  // Model name. e.g: '1_Ceres'
    bool        no_model;

    // Cached values.
    float       vmag;
    double      pvo[2][4];

    // Linked list of currently visible.
    mplanet_t   *visible_next, *visible_prev;
};

/*
 * Type: mplanets_t
 * Minor planets module object
 */
typedef struct mplanets {
    obj_t   obj;
    char    *source_url;
    bool    parsed; // Set to true once the data has been parsed.
    bool    visible;
    double hints_mag_offset; // Hints/labels magnitude offset
    bool   hints_visible;

    mplanet_t *render_current;
    mplanet_t *visibles; // Linked list of currently visible minor planets.
} mplanets_t;

// Static instance.
static mplanets_t *g_mplanets = NULL;

static double mean3(double x, double y, double z)
{
    return (x + y + z) / 3;
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
        mplanet = (void*)module_add_new(&mplanets->obj, "asteroid", NULL);
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
        if (name[0]) {
            _Static_assert(sizeof(name) == sizeof(mplanet->name), "");
            memcpy(mplanet->name, name, sizeof(name));
            snprintf(mplanet->model, sizeof(mplanet->model), "%d_%s",
                     mplanet->mpl_number, mplanet->name);
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
        obj_t *obj, const char *url, const char *key)
{
    mplanets_t *mplanets = (void*)obj;
    if (strcmp(key, "mpc_asteroids") != 0) return 1;
    mplanets->source_url = strdup(url);
    return 0;
}

static int mplanet_init(obj_t *obj, json_value *args)
{
    // Support creating a minor planet using noctuasky model data json values.
    mplanet_t *mp = (mplanet_t*)obj;
    orbit_t *orbit = &mp->orbit;
    json_value *model, *names;
    int num = -1;
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
        num = json_get_attr_i(model, "Number", -1);
    }
    if (num >= 0) {
        mp->mpl_number = num;
    }
    names = json_get_attr(args, "names", json_array);
    if (names && names->u.array.length > 0) {
        designation_cleanup(names->u.array.values[0]->u.string.ptr, mp->name,
                            sizeof(mp->name), 0);
    }
    // XXX: use proper type.
    strncpy(mp->obj.type, "MBA", 4);
    return 0;
}

static int mplanet_update(mplanet_t *mp, const observer_t *obs)
{
    double pvh[2][3], pvo[2][3];

    orbit_compute_pv(0, obs->tt, pvh[0], pvh[1],
            mp->orbit.d, mp->orbit.i, mp->orbit.o, mp->orbit.w,
            mp->orbit.a, mp->orbit.n, mp->orbit.e, mp->orbit.m,
            0, 0);
    mat3_mul_vec3(ECLIPTIC_ROT, pvh[0], pvh[0]);
    mat3_mul_vec3(ECLIPTIC_ROT, pvh[1], pvh[1]);
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
    double bounds[2][3], radius;

    mplanet_update(mp, obs);
    switch (info) {
    case INFO_PVO:
        memcpy(out, mp->pvo, sizeof(mp->pvo));
        return 0;
    case INFO_VMAG:
        *(double*)out = mp->vmag;
        return 0;
    case INFO_RADIUS:
        mp->no_model = mp->no_model ||
            painter_get_3d_model_bounds(NULL, mp->model, bounds);
        if (!mp->no_model) {
            radius = mean3(bounds[1][0] - bounds[0][0],
                           bounds[1][1] - bounds[0][1],
                           bounds[1][2] - bounds[0][2]) * 1000 / 2 * DM2AU;
            *(double*)out = radius / vec3_norm(mp->pvo[0]);
            return 0;
        }
        return 1;
    }
    return 1;
}

static int render_3d_model(const mplanet_t *mplanet, const painter_t *painter_)
{
    painter_t painter = *painter_;
    double model_mat[4][4] = MAT4_IDENTITY;

    painter.flags |= PAINTER_ENABLE_DEPTH;
    mat4_itranslate(model_mat, VEC3_SPLIT(mplanet->pvo[0]));

    mat4_iscale(model_mat, 1000 * DM2AU, 1000 * DM2AU, 1000 * DM2AU);
    paint_3d_model(&painter, mplanet->model, model_mat, NULL);
    return 0;
}

// Note: return 1 if the planet is actually visible on screen.
static int mplanet_render(const obj_t *obj, const painter_t *painter)
{
    double pvo[2][4], win_pos[2], vmag, size, luminance;
    double label_color[4] = RGBA(223, 223, 255, 255);
    mplanet_t *mplanet = (mplanet_t*)obj;
    point_t point;
    const bool selected = core->selection && obj == core->selection;
    double hints_mag_offset = g_mplanets->hints_mag_offset;
    double radius_m, model_r, model_size, bounds[2][3], model_alpha = 0;
    double max_radius, radius, cap[4];

    mplanet_update(mplanet, painter->obs);
    vmag = mplanet->vmag;

    if (!selected && vmag > painter->stars_limit_mag + 1.4 + hints_mag_offset)
        return 0;

    // First clip test using a fixed small radius.
    obj_get_pvo(obj, painter->obs, pvo);
    vec3_normalize(pvo[0], cap);
    cap[3] = cos(1. / 60 * DD2R);
    if (painter_is_cap_clipped(painter, FRAME_ICRF, cap))
        return 0;

    painter_project(painter, FRAME_ICRF, pvo[0], false, false, win_pos);
    core_get_point_for_mag(vmag, &size, &luminance);

    // Max possible model radius (using Ceres radius).
    max_radius = core_get_point_for_apparent_angle(painter->proj,
            500000 * DM2AU / vec3_norm(pvo[0]));

    // Render 3d model if possible.
    if ((max_radius > size) &&
        painter_get_3d_model_bounds(painter, mplanet->model, bounds) == 0)
    {
        radius_m = mean3(bounds[1][0] - bounds[0][0],
                         bounds[1][1] - bounds[0][1],
                         bounds[1][2] - bounds[0][2]) / 2 * 1000;
        model_r = radius_m * DM2AU / vec3_norm(mplanet->pvo[0]);
        model_size = core_get_point_for_apparent_angle(
                painter->proj, model_r);
        model_alpha = smoothstep(0.5, 1.0, size ? model_size / size : 1);
        if (model_alpha > 0)
            render_3d_model(mplanet, painter);
    }


    point = (point_t) {
        .pos = {win_pos[0], win_pos[1]},
        .size = size,
        .color = {255, 255, 255, luminance * 255 * (1.0 - model_alpha)},
        .obj = obj,
    };
    paint_2d_points(painter, 1, &point);

    // Render name if needed.
    if (*mplanet->name && (selected || (g_mplanets->hints_visible &&
                                        vmag <= painter->hints_limit_mag +
                                        1.4 + hints_mag_offset))) {
        // Use actual pixel radius on screen.
        if (mplanet_get_info(obj, painter->obs, INFO_RADIUS, &radius) == 0) {
            radius = core_get_point_for_apparent_angle(painter->proj, radius);
            size = max(size, radius);
        }

        if (selected)
            vec4_set(label_color, 1, 1, 1, 1);
        labels_add_3d(mplanet->name, FRAME_ICRF, pvo[0], false, size + 4,
              FONT_SIZE_BASE - 1, label_color, 0, 0,
              TEXT_SEMI_SPACED | TEXT_BOLD | (selected ? 0 : TEXT_FLOAT),
              0, obj);
    }
    return 1;
}

void mplanet_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    mplanet_t *mp = (mplanet_t*)obj;
    char buf[128];
    // Make sure we sort so that the first designation is the most useful.
    // First name, e.g. 'Ceres'
    if (*mp->name) f(obj, user, "NAME", mp->name);
    // Then MPC number with name e.g. '(1) Ceres'
    if (*mp->name && mp->mpl_number) {
        snprintf(buf, sizeof(buf), "(%d) %s", mp->mpl_number, mp->name);
        f(obj, user, "MPC", buf);
    }
    // Then designation, e.g. '2001 FO32'
    if (*mp->desig) f(obj, user, NULL, mp->desig);
    // Finally only number, e.g. '(231937)'
    if (!*mp->name && mp->mpl_number) {
        snprintf(buf, sizeof(buf), "(%d) %s", mp->mpl_number, mp->name);
        f(obj, user, "MPC", buf);
    }
}

static int mplanets_init(obj_t *obj, json_value *args)
{
    mplanets_t *mps = (void*)obj;
    assert(!g_mplanets);
    g_mplanets = mps;
    mps->visible = true;
    mps->hints_visible = true;
    return 0;
}

static int mplanets_update(obj_t *obj, double dt)
{
    int size, code;
    const char *data;
    mplanets_t *mps = (void*)obj;

    if (!mps->parsed && mps->source_url) {
        data = asset_get_data(mps->source_url, &size, &code);
        if (!code) return 0; // Still loading.
        mps->parsed = true;
        if (!data) {
            LOG_W("Cannot read asteroids data: %s (%d)", mps->source_url, code);
            return 0;
        }
        load_data(mps, data, size);
        asset_release(mps->source_url);
    }
    return 0;
}

static void add_to_visible(mplanets_t *mps, mplanet_t *mplanet)
{
    if (mplanet->visible_prev) return;
    DL_APPEND2(mps->visibles, mplanet, visible_prev, visible_next);
}

static int mplanets_render(const obj_t *obj, const painter_t *painter)
{
    mplanets_t *mps = (void*)obj;
    int i, r;
    const int update_nb = 32;
    mplanet_t *child, *tmp;

    if (!mps->visible) return 0;

    // If the current selection is a minor planet, make sure it is flagged
    // as visible.
    if (core->selection && core->selection->parent == obj) {
        add_to_visible(mps, (void*)core->selection);
    }

    // Render all the flagged visible minor planets, remove those that are
    // no longer visible.
    DL_FOREACH_SAFE2(mps->visibles, child, tmp, visible_next) {
        r = mplanet_render(&child->obj, painter);
        if (r == 0 && &child->obj != core->selection) {
            DL_DELETE2(mps->visibles, child, visible_prev, visible_next);
            child->visible_prev = NULL;
        }
    }

    // Then iter part of the full list as well.
    for (   i = 0, child = mps->render_current ?: (void*)mps->obj.children;
            child && i < update_nb;
            i++, child = (void*)child->obj.next) {
        if (child->visible_prev) continue; // Was already rendered.
        r = mplanet_render(&child->obj, painter);
        if (r == 1) add_to_visible(mps, child);
    }
    mps->render_current = child;

    return 0;
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
    .update         = mplanets_update,
    .render         = mplanets_render,
    .render_order   = 20,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(mplanets_t, visible)),
        PROPERTY(hints_mag_offset, TYPE_FLOAT,
                 MEMBER(mplanets_t, hints_mag_offset)),
        PROPERTY(hints_visible, TYPE_BOOL, MEMBER(mplanets_t, hints_visible)),
        {},
    },
};
OBJ_REGISTER(mplanets_klass)
