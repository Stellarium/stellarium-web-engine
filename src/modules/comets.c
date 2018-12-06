/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <zlib.h> // For crc32.
#include <regex.h>

// Data source url.
static const char *URL = "https://data.stellarium.org/mpc/CometEls.txt";

typedef struct orbit_t {
    double d;    // date (julian day).
    double i;    // inclination (rad).
    double o;    // Longitude of the Ascending Node (rad).
    double w;    // Argument of Perihelion (rad).
    double n;    // Daily motion (rad/day).
    double e;    // Eccentricity.
    double q;    // Perihelion distance (AU).
} orbit_t;

/*
 * Type: comet_t
 * Object that represents a single comet
 */
typedef struct {
    obj_t       obj;
    int         num;
    double      amag;
    double      slope_param;
    orbit_t     orbit;
    char        name[64];
    int         on_screen_timer;
} comet_t;

/*
 * Type: comet_t
 * Comets module object
 */
typedef struct {
    obj_t   obj;
    bool    parsed; // Set to true once the data has been parsed.
    int     update_range;
    regex_t search_reg;
} comets_t;


static const char *orbit_type_to_otype(char o)
{
    switch (o) {
    case 'P': return "PCo";
    case 'C': return "CCo";
    case 'X': return "XCo";
    case 'D': return "DCo";
    case 'A': return "ACo";
    case 'I': return "ISt";
    default: return "Com";
    }
}

static void load_data(comets_t *comets, const char *data)
{
    comet_t *comet;
    int line, num, year, month;
    double day, q, e, w, o, i, djm0, amag, slope;
    char orbit_type;
    char desgn[16] = {};
    char name[64] = {};
    const catalog_t CAT[] = {
        { 1,    4,  "I4",   "Periodic comet number", '?'},
        { 5,    5,  "A1",   "Orbit type (generally `C', `P' or `D')"},
        { 6,   12,  "A7",   "Provisional designation (in packed form)"},
        {15,   18,  "I4",   "Year of perihelion passage"},
        {20,   21,  "I2",   "Month of perihelion passage"},
        {23,   29,  "F7.4", "Day of perihelion passage (TT)"},
        {31,   39,  "F9.6", "Perihelion distance (AU)"},
        {42,   49,  "F8.6", "Orbital eccentricity"},
        {52,   59,  "F8.4", "Argument of perihelion, J2000.0 (degrees)"},
        {62,   69,  "F8.4", "Longitude of the ascending node, J2000.0 (deg)"},
        {72,   79,  "F8.4", "Inclination in degrees, J2000.0 (degrees)"},
        {92,   95,  "F4.1", "Absolute magnitude"},
        {97,  100,  "F4.0", "Slope parameter"},
        {103, 158,  "A56",  "Designation and Name"},
        {0}
    };
    CATALOG_ITER(CAT, data, line, &num, &orbit_type, desgn,
                 &year, &month, &day, &q, &e, &w, &o, &i, &amag, &slope,
                 name) {
        comet = (void*)obj_create("mpc_comet", NULL, &comets->obj, NULL);
        comet->num = num;
        comet->amag = amag;
        comet->slope_param = slope;
        eraCal2jd(year, month, day, &djm0, &comet->orbit.d);
        comet->orbit.d += fmod(day, 1.0);
        comet->orbit.i = i * DD2R;
        comet->orbit.o = o * DD2R;
        comet->orbit.w = w * DD2R;
        comet->orbit.q = q;
        comet->orbit.e = e;
        memcpy(comet->obj.type, orbit_type_to_otype(orbit_type), 4);
        strcpy(comet->name, name);
        str_rstrip(comet->name);
        comet->obj.oid = oid_create("Com ", line);
        identifiers_add("NAME", comet->name, comet->obj.oid, 0, "Com ",
                        amag, NULL, NULL);
    }
}

static int comet_update(obj_t *obj, const observer_t *obs, double dt)
{
    comet_t *comet = (comet_t*)obj;
    double a, p, n, ph[2][3], pv[2][3], or, sr, b, v, w, r, o, u, i;
    const double K = 0.01720209895; // AU, day

    // Position algo for elliptical comets.
    if (comet->orbit.e < 0.98) {
        // Mean distance.
        a = comet->orbit.q / (1.0 - comet->orbit.e);
        // Orbital period.
        p = 2 * M_PI * sqrt(a * a * a) / K;
        // Daily motion.
        n = 2 * M_PI / p;

        orbit_compute_pv(0.005 * DD2R,
                         obs->tt, ph[0], NULL, comet->orbit.d, comet->orbit.i,
                         comet->orbit.o, comet->orbit.w, a, n, comet->orbit.e,
                         0, 0, 0);
    } else {
        // Algo for non elliptical orbits, taken from
        // http://stjarnhimlen.se/comp/tutorial.html
        // TODO: move into orbit_compute_pv directly somehow?
        a = 1.5 * (obs->tt - comet->orbit.d) * K /
                sqrt(2 * comet->orbit.q * comet->orbit.q * comet->orbit.q);
        b = sqrt(1 + a * a);
        w = pow(b + a, 1. / 3) - pow(b - a, 1. / 3);
        v = 2 * atan(w);
        r = comet->orbit.q * (1 + w * w);
        // Compute position into the plane of the ecliptic.
        o = comet->orbit.o;
        u = v + comet->orbit.w;
        i = comet->orbit.i;
        ph[0][0] = r * (cos(o) * cos(u) - sin(o) * sin(u) * cos(i));
        ph[0][1] = r * (sin(o) * cos(u) + cos(o) * sin(u) * cos(i));
        ph[0][2] = r * (sin(u) * sin(i));
    }

    mat4_mul_vec3(obs->re2i, ph[0], ph[0]);
    sr = vec3_norm(ph[0]);

    vec3_set(ph[1], 0, 0, 0);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, ph, pv);
    vec3_copy(pv[0], obj->pvo[0]);
    obj->pvo[0][3] = 1;
    vec3_copy(pv[1], obj->pvo[1]);
    obj->pvo[1][3] = 0;

    // Compute vmag.
    // We use the g,k model: m = g + 5*log10(D) + 2.5*k*log10(r)
    // (http://www.clearskyinstitute.com/xephem/help/xephem.html)
    sr = vec3_norm(ph[0]);
    or = vec3_norm(obj->pvo[0]);
    comet->obj.vmag = comet->amag + 5 * log10(or) +
                      2.5 * comet->slope_param * log10(sr);
    return 0;
}

static int comet_render(const obj_t *obj, const painter_t *painter)
{
    double pos[4], win_pos[4], vmag, size, luminance;
    comet_t *comet = (comet_t*)obj;
    point_t point;
    double label_color[4] = RGBA(255, 124, 124, 255);
    vmag = comet->obj.vmag;

    if (vmag > painter->mag_max) return 0;
    if (isnan(obj->pvo[0][0])) return 0; // For the moment!
    convert_frame(painter->obs, FRAME_ICRF, FRAME_OBSERVED, false,
                        obj->pvo[0], pos);

    if ((painter->flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return 0;
    convert_frame(painter->obs, FRAME_OBSERVED, FRAME_VIEW, false,
                      pos, pos);
    if (!project(painter->proj, PROJ_TO_WINDOW_SPACE, 2, pos, win_pos))
        return 0;

    comet->on_screen_timer = 100; // Keep the comet 'alive' for 100 frames.
    core_get_point_for_mag(vmag, &size, &luminance);

    point = (point_t) {
        .pos = {win_pos[0], win_pos[1], 0, 0},
        .size = size,
        .color = {1, 1, 1, luminance},
        .oid = obj->oid,
    };
    paint_points(painter, 1, &point, FRAME_WINDOW);

    // Render name if needed.
    if (*comet->name && vmag < painter->label_mag_max) {
        labels_add(comet->name, win_pos, size, 13, label_color,
                   0, ANCHOR_AROUND, 0);
    }
    return 0;
}

static int comets_init(obj_t *obj, json_value *args)
{
    comets_t *comets = (comets_t*)obj;
    regcomp(&comets->search_reg,
            "(([PCXDAI])/([0-9]+) [A-Z].+)|([0-9]+[PCXDAI]/.+)",
            REG_EXTENDED);
    return 0;
}

static bool range_contains(int range_start, int range_size, int nb, int i)
{
    if (i < range_start) i += nb;
    return i > range_start && i < range_start + range_size;
}

static int comets_update(obj_t *obj, const observer_t *obs, double dt)
{
    int size, code, i, nb;
    const char *data;
    obj_t *tmp;
    comet_t *child;
    comets_t *comets = (void*)obj;
    const int update_nb = 32;

    if (!comets->parsed) {
        data = asset_get_data(URL, &size, &code);
        if (!code) return 0; // Still loading.
        comets->parsed = true;
        if (!data) {
            LOG_E("Cannot load comets data: %s", URL);
            return 0;
        }
        load_data(comets, data);
        // Make sure the search work.
        assert(strcmp(obj_get(NULL, "C/1995 O1", 0)->klass->id,
                      "mpc_comet") == 0);
        assert(strcmp(obj_get(NULL, "1P/Halley", 0)->klass->id,
                      "mpc_comet") == 0);
    }

    /* To prevent spending too much time computing position of comets that
     * are not visible, we only update a small number of them at each
     * frame, using a moving range.  The comets who have been flagged as
     * on screen get updated no matter what.  */
    DL_COUNT(obj->children, tmp, nb);
    i = 0;
    OBJ_ITER(obj, child, "mpc_comet") {
        if (child->on_screen_timer) child->on_screen_timer--;
        if (child->on_screen_timer ||
                range_contains(comets->update_range, update_nb, nb, i))
        {
            obj_update((obj_t*)child, obs, dt);
        }
        i++;
    }
    comets->update_range = (comets->update_range + update_nb) % nb;
    return 0;
}

static int comets_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, "mpc_comet")
        obj_render(child, painter);
    return 0;
}

static obj_t *comets_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (!oid_is_catalog(oid, "Com ")) return NULL;
    OBJ_ITER(obj, child, NULL) {
        if (child->oid == oid) {
            child->ref++;
            return child;
        }
    }
    return NULL;
}

static obj_t *comets_get(const obj_t *obj, const char *id, int flags)
{
    comets_t *comets = (comets_t*)obj;
    comet_t *child;
    regmatch_t matches[3];
    int r;
    r = regexec(&comets->search_reg, id, 3, matches, 0);
    if (r) return NULL;
    OBJ_ITER(obj, child, "mpc_comet") {
        if (str_startswith(child->name, id)) {
            child->obj.ref++;
            return &child->obj;
        }
    }
    return NULL;
}

/*
 * Meta class declarations.
 */

static obj_klass_t comet_klass = {
    .id         = "mpc_comet",
    .size       = sizeof(comet_t),
    .update     = comet_update,
    .render     = comet_render,
    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        PROPERTY("distance"),
        PROPERTY("radec"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {},
    },
};
OBJ_REGISTER(comet_klass)

static obj_klass_t comets_klass = {
    .id             = "comets",
    .size           = sizeof(comets_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = comets_init,
    .update         = comets_update,
    .render         = comets_render,
    .get            = comets_get,
    .get_by_oid     = comets_get_by_oid,
    .render_order   = 20,
};
OBJ_REGISTER(comets_klass)
