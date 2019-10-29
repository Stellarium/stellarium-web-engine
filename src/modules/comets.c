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
#include <regex.h>

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
    char        name[64]; // e.g 'C/1995 O1 (Hale-Bopp)'
    bool        on_screen;  // Set once the object has been visible.

    // Cached values.
    double      vmag;
    double      pvo[2][4];
} comet_t;

/*
 * Type: comet_t
 * Comets module object
 */
typedef struct {
    obj_t   obj;
    char    *source_url;
    bool    parsed; // Set to true once the data has been parsed.
    int     update_pos; // Index of the position for iterative update.
    regex_t search_reg;
    bool    visible;
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

static void load_data(comets_t *comets, const char *data, int size)
{
    comet_t *comet;
    int num, nb_err = 0, len, line_idx = 0, r, nb;
    double peri_time, peri_dist, e, peri, node, i, epoch, h, g;
    double last_epoch = 0;
    const char *line = NULL;
    char orbit_type;
    char desgn[64];
    char buf[128];
    obj_t *tmp;

    while (iter_lines(data, size, &line, &len)) {
        line_idx++;
        r = mpc_parse_comet_line(
                line, len, &num, &orbit_type, &peri_time, &peri_dist, &e,
                &peri, &node, &i, &epoch, &h, &g, desgn);
        if (r) {
            nb_err++;
            continue;
        }

        comet = (void*)module_add_new(&comets->obj, "mpc_comet", NULL, NULL);
        comet->num = num;
        comet->amag = h;
        comet->slope_param = g;
        comet->orbit.d = peri_time;
        comet->orbit.i = i * DD2R;
        comet->orbit.o = node * DD2R;
        comet->orbit.w = peri * DD2R;
        comet->orbit.q = peri_dist;
        comet->orbit.e = e;
        strncpy(comet->obj.type, orbit_type_to_otype(orbit_type), 4);
        strcpy(comet->name, desgn);
        comet->obj.oid = oid_create("Com", line_idx);
        comet->pvo[0][0] = NAN;
        last_epoch = max(epoch, last_epoch);
    }

    if (nb_err) {
        LOG_W("Comet planet data got %d error lines.", nb_err);
    }
    DL_COUNT(comets->obj.children, tmp, nb);
    LOG_I("Parsed %d comets (latest epoch: %s)", nb,
          format_time(buf, last_epoch, 0, "YYYY-MM-DD"));
}

static int comet_update(comet_t *comet, const observer_t *obs)
{
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

    mat3_mul_vec3(obs->re2i, ph[0], ph[0]);

    vec3_set(ph[1], 0, 0, 0);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, ph, pv);
    vec3_copy(pv[0], comet->pvo[0]);
    comet->pvo[0][3] = 1;
    vec3_copy(pv[1], comet->pvo[1]);
    comet->pvo[1][3] = 0;

    // Compute vmag.
    // We use the g,k model: m = g + 5*log10(D) + 2.5*k*log10(r)
    // (http://www.clearskyinstitute.com/xephem/help/xephem.html)
    sr = vec3_norm(ph[0]);
    or = vec3_norm(comet->pvo[0]);
    comet->vmag = comet->amag + 5 * log10(or) +
                      2.5 * comet->slope_param * log10(sr);
    return 0;
}

static int comet_get_info(const obj_t *obj, const observer_t *obs, int info,
                          void *out)
{
    comet_t *comet = (comet_t*)obj;
    comet_update(comet, obs);
    switch (info) {
    case INFO_PVO:
        memcpy(out, comet->pvo, sizeof(comet->pvo));
        return 0;
    case INFO_VMAG:
        *(double*)out = comet->vmag;
        return 0;
    }
    return 1;
}

void comet_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    comet_t *comet = (void*)obj;
    f(obj, user, "NAME", comet->name);
}


static int comet_render(const obj_t *obj, const painter_t *painter)
{
    double win_pos[2], vmag, size, luminance;
    comet_t *comet = (comet_t*)obj;
    point_t point;
    double label_color[4] = RGBA(255, 124, 124, 255);
    const bool selected = core->selection && obj->oid == core->selection->oid;
    vmag = comet->vmag;

    if (vmag > painter->stars_limit_mag) return 0;
    if (isnan(comet->pvo[0][0])) return 0; // For the moment!
    if (!painter_project(painter, FRAME_ICRF, comet->pvo[0], false, true,
                         win_pos))
        return 0;

    comet->on_screen = true;
    core_get_point_for_mag(vmag, &size, &luminance);

    point = (point_t) {
        .pos = {win_pos[0], win_pos[1]},
        .size = size,
        .color = {255, 255, 255, luminance * 255},
        .oid = obj->oid,
    };
    paint_2d_points(painter, 1, &point);

    // Render name if needed.
    if (*comet->name && (selected || vmag < painter->hints_limit_mag)) {
        if (selected)
            vec4_set(label_color, 1, 1, 1, 1);
        labels_add_3d(comet->name, FRAME_ICRF, comet->pvo[0], false, size,
            FONT_SIZE_BASE, label_color, 0, 0,
            selected ? TEXT_BOLD : TEXT_FLOAT,
            0, obj->oid);
    }
    return 0;
}

static int comets_init(obj_t *obj, json_value *args)
{
    comets_t *comets = (comets_t*)obj;
    comets->visible = true;
    regcomp(&comets->search_reg,
            "(([PCXDAI])/([0-9]+) [A-Z].+)|([0-9]+[PCXDAI]/.+)",
            REG_EXTENDED);
    return 0;
}

static int comets_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    comets_t *comets = (void*)obj;
    if (strcmp(type, "mpc_comets") != 0) return 1;
    comets->source_url = strdup(url);
    return 0;
}

static bool range_contains(int range_start, int range_size, int nb, int i)
{
    if (i < range_start) i += nb;
    return i > range_start && i < range_start + range_size;
}

static int comets_update(obj_t *obj, double dt)
{
    PROFILE(comets_update, 0);
    int size, code;
    const char *data;
    comets_t *comets = (void*)obj;

    if (!comets->parsed && comets->source_url) {
        data = asset_get_data(comets->source_url, &size, &code);
        if (!code) return 0; // Still loading.
        comets->parsed = true;
        if (!data) {
            LOG_E("Cannot load comets data: %s (%d)",
                  comets->source_url, code);
            return 0;
        }
        load_data(comets, data, size);
        asset_release(comets->source_url);
        // Make sure the search work.
        assert(strcmp(obj_get(NULL, "C/1995 O1", 0)->klass->id,
                      "mpc_comet") == 0);
        assert(strcmp(obj_get(NULL, "1P/Halley", 0)->klass->id,
                      "mpc_comet") == 0);
    }

    return 0;
}

static int comets_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(comets_render, 0);
    comets_t *comets = (void*)obj;
    comet_t *child;
    obj_t *tmp;
    const int update_nb = 32;
    int nb, i;

    if (!comets->visible) return 0;
    /* To prevent spending too much time computing position of comets that
     * are not visible, we only render a small number of them at each
     * frame, using a moving range.  The comets who have been flagged as
     * on screen get rendered no matter what.  */
    DL_COUNT(obj->children, tmp, nb);
    i = 0;
    MODULE_ITER(obj, child, "mpc_comet") {
        if (child->on_screen ||
                range_contains(comets->update_pos, update_nb, nb, i)) {
            obj_render(&child->obj, painter);
        }
        i++;
    }
    comets->update_pos = nb ? (comets->update_pos + update_nb) % nb : 0;

    return 0;
}

static obj_t *comets_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (!oid_is_catalog(oid, "Com")) return NULL;
    MODULE_ITER(obj, child, NULL) {
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
    MODULE_ITER(obj, child, "mpc_comet") {
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
    .get_info   = comet_get_info,
    .render     = comet_render,
    .get_designations = comet_get_designations,
};
OBJ_REGISTER(comet_klass)

static obj_klass_t comets_klass = {
    .id             = "comets",
    .size           = sizeof(comets_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE | OBJ_LISTABLE,
    .init           = comets_init,
    .add_data_source = comets_add_data_source,
    .update         = comets_update,
    .render         = comets_render,
    .get            = comets_get,
    .get_by_oid     = comets_get_by_oid,
    .render_order   = 20,
    .attributes     = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(comets_t, visible)),
        {},
    },
};
OBJ_REGISTER(comets_klass)
