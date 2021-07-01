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

// J2000 ecliptic to ICRF rotation matrix.
// Computed with 'eraEcm06(ERFA_DJ00, 0)'
static const double ECLIPTIC_ROT[3][3] = {
    { 1.000000000000, -0.000000070784, 0.000000080562},
    { 0.000000032897,  0.917482129915, 0.397776999444},
    {-0.000000102070, -0.397776999444, 0.917482129915},
};

enum {
    TAIL_GAS,
    TAIL_DUST,
};

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
typedef struct comet comet_t;
struct comet {
    obj_t       obj;
    int         num;
    double      h;  // Absolute magnitude.
    double      g;  // Slope parameter.
    orbit_t     orbit;
    char        name[64]; // e.g 'C/1995 O1 (Hale-Bopp)'

    // Optional historical data.
    struct {
        double      time; // TT MJD.
        double      duration; // days.
        double      h;
        double      g;
        double      peak_vmag;  // If set, brightest observed historical mag.
    } history;

    // Cached values.
    double      vmag;
    double      pvo[2][4];

    // Linked list of currently visible.
    comet_t     *visible_next, *visible_prev;
};

/*
 * Type: comet_t
 * Comets module object
 */
typedef struct {
    obj_t   obj;
    char    *source_url;
    bool    parsed; // Set to true once the data has been parsed.
    regex_t search_reg;
    bool    visible;
    // Hints/labels magnitude offset
    double hints_mag_offset;
    bool   hints_visible;

    comet_t *render_current;
    comet_t *visibles; // Linked list of currently visible comets.
} comets_t;

// Static instance.
static comets_t *g_comets = NULL;

static double date2mjd(int year, int month, int day)
{
    double djm0, djm;
    eraCal2jd(year, month, day, &djm0, &djm);
    return djm0 - DJM0 + djm;
}

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

        comet = (void*)module_add_new(&comets->obj, "mpc_comet", NULL);
        comet->num = num;
        comet->h = h;
        comet->g = g;
        comet->orbit.d = peri_time;
        comet->orbit.i = i * DD2R;
        comet->orbit.o = node * DD2R;
        comet->orbit.w = peri * DD2R;
        comet->orbit.q = peri_dist;
        comet->orbit.e = e;
        strncpy(comet->obj.type, orbit_type_to_otype(orbit_type), 4);
        snprintf(comet->name, sizeof(comet->name), "%s", desgn);
        comet->pvo[0][0] = NAN;
        last_epoch = max(epoch, last_epoch);

        // Check for historical comets, where we change the h and g values
        // around a peak date.  Only support Neowise for the moment.
        if (strcmp(desgn, "C/2020 F3 (NEOWISE)") == 0) {
            comet->history = (typeof(comet->history)) {
                .time = date2mjd(2020, 7, 3),
                .duration = 30,
                .peak_vmag = 1,
                .h = 7.5,
                .g = 5.2,
            };
        }
    }

    if (nb_err) {
        LOG_W("Comet planet data got %d error lines.", nb_err);
    }
    DL_COUNT(comets->obj.children, tmp, nb);
    LOG_I("Parsed %d comets (latest epoch: %s)", nb,
          format_time(buf, last_epoch, 0, "YYYY-MM-DD"));
}

static void comet_get_h_g(const comet_t *comet, double tt, double *h, double *g)
{
    double dt, k;
    if (!comet->history.time) {
        *h = comet->h;
        *g = comet->g;
        return;
    }
    dt = fabs(tt - comet->history.time);
    k = smoothstep(comet->history.duration * 1.5, comet->history.duration, dt);
    *h = mix(comet->h, comet->history.h, k);
    *g = mix(comet->g, comet->history.g, k);
}

static int comet_update(comet_t *comet, const observer_t *obs)
{
    double a, p, n, ph[2][3], pv[2][3], or, sr, b, v, w, r, o, u, i, h, g;
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

    mat3_mul_vec3(ECLIPTIC_ROT, ph[0], ph[0]);

    vec3_set(ph[1], 0, 0, 0);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, ph, pv);
    vec3_copy(pv[0], comet->pvo[0]);
    comet->pvo[0][3] = 1;
    vec3_copy(pv[1], comet->pvo[1]);
    comet->pvo[1][3] = 0;

    // Compute vmag.
    // We use the g,k model: m = g + 5*log10(D) + 2.5*k*log10(r)
    // (http://www.clearskyinstitute.com/xephem/help/xephem.html)
    // XXX: probably better to switch to the same model as for asteroids.
    sr = vec3_norm(ph[0]);
    or = vec3_norm(comet->pvo[0]);
    comet_get_h_g(comet, obs->tt, &h, &g);
    comet->vmag = h + 5 * log10(or) + 2.5 * g * log10(sr);
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
    case INFO_SEARCH_VMAG:
        *(double*)out = min(comet->vmag, comet->history.peak_vmag ?: DBL_MAX);
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

// https://www.projectpluto.com/update7b.htm#comet_tail_formula
static void compute_tail_size(double H, double K, double r,
                              double *out_l, double *out_d)
{
    double mhelio, Lo, L, Do, D;
    mhelio = H + K * log10(r);
    Lo = pow(10, -0.0075 * mhelio * mhelio - 0.19 * mhelio + 2.10);
    L = Lo * (1 - pow(10, -4 * r)) * (1 - pow(10, -2 * r));
    Do = pow(10, -0.0033 * mhelio * mhelio - 0.07 * mhelio + 3.25);
    D = Do * (1 - pow(10, -2 * r)) * (1 - pow(10, -r));
    // Convert all in AU.
    *out_l = L * 1000000 * 1000 * DM2AU;
    *out_d = D * 1000 * 1000 * DM2AU;
}

// Rotate a matrix to make the Y axis point toward a given position.
static void mat_rotate_y_toward(double mat[4][4], double dir[3])
{
    double rot[4][4] = MAT4_IDENTITY;
    vec3_set(rot[0], 1, 0, 0);
    vec3_normalize(dir, rot[1]);
    vec3_cross(rot[0], rot[1], rot[2]);
    vec3_normalize(rot[2], rot[2]);
    vec3_cross(rot[1], rot[2], rot[0]);
    mat4_mul(mat, rot, mat);
}

static void render_tail(comet_t *comet, const painter_t *painter, int tail)
{
    double model_mat[4][4] = MAT4_IDENTITY;
    double ph[3], rh, l, d, angle, point, dir[3], curvature = 0, h, g;
    double color[4], lum_apparent, ld;
    json_value *args, *uniforms;

    vec3_sub(comet->pvo[0], painter->obs->sun_pvo[0], ph);
    rh = vec3_norm(ph);
    comet_get_h_g(comet, painter->obs->tt, &h, &g);
    compute_tail_size(h, g, rh, &l, &d);
    mat4_itranslate(model_mat, VEC3_SPLIT(comet->pvo[0]));

    switch (tail) {
    case TAIL_GAS:
        vec4_set(color, 0.15, 0.35, 0.6, 0.25);
        mat_rotate_y_toward(model_mat, ph);
        // Rotate along axis so that both tails don't look exactly the same.
        mat4_ry(M_PI / 2, model_mat, model_mat);
        break;
    case TAIL_DUST:
        // Empirical size adjustement to the dust tail size.
        d *= 1.5;
        l *= 0.6;
        curvature = -M_PI;
        vec4_set(color, 0.7, 0.7, 0.4, 1.0);
        vec3_addk(ph, comet->pvo[1], -5, dir);
        mat_rotate_y_toward(model_mat, dir);
        break;
    }

    // Compute alpha.
    // XXX: this is ad-hoc, I have to manually make the tail brigher than
    // it should.  Also since we don't report the luminance to the
    // tonemapper I manually dim out the tail as we zoom in!
    angle = d / vec3_norm(comet->pvo[0]);
    lum_apparent = core_mag_to_lum_apparent(
            comet->vmag - 4, M_PI * angle * angle);
    ld = tonemapper_map(&core->tonemapper, lum_apparent);
    color[3] *= clamp(ld, 0.0, 1.0);

    point = core_get_point_for_apparent_angle(painter->proj, angle);
    color[3] *= smoothstep(1000, 100, point);
    if (color[3] <= 0.0) return;

    // Translate to put the orgin in the middle of the coma.
    mat4_itranslate(model_mat, 0, -0.0001, 0);
    mat4_iscale(model_mat, d / 2, l, d / 2);

    args = json_object_new(0);
    json_object_push(args, "shader", json_string_new("comet"));
    json_object_push(args, "blend_mode", json_string_new("ADD"));
    uniforms = json_object_push(args, "uniforms", json_object_new(0));
    json_object_push(uniforms, "u_length", json_double_new(l));
    json_object_push(uniforms, "u_curvature", json_double_new(curvature));
    json_object_push(uniforms, "u_color", json_vector_new(4, color));

    paint_3d_model(painter, "comet", model_mat, args);
    json_builder_free(args);
}


// Note: return 1 if the comet is actually visible on screen.
static int comet_render(const obj_t *obj, const painter_t *painter)
{
    double win_pos[2], vmag, size, luminance;
    comet_t *comet = (comet_t*)obj;
    point_t point;
    double label_color[4] = RGBA(223, 223, 255, 255);
    const bool selected = core->selection && obj == core->selection;
    double hints_mag_offset = g_comets->hints_mag_offset;
    double cap[4];

    comet_update(comet, painter->obs);
    vmag = comet->vmag;

    if (!selected && vmag > painter->stars_limit_mag + 2.0 + hints_mag_offset)
        return 0;
    if (isnan(comet->pvo[0][0])) return 0; // For the moment!

    // Clip test using a small radius for the tail size.
    vec3_normalize(comet->pvo[0], cap);
    cap[3] = cos(5 * DD2R);
    if (painter_is_cap_clipped(painter, FRAME_ICRF, cap))
        return 0;

    painter_project(painter, FRAME_ICRF, comet->pvo[0], false, false, win_pos);
    core_get_point_for_mag(vmag, &size, &luminance);

    point = (point_t) {
        .pos = {win_pos[0], win_pos[1]},
        .size = size,
        .color = {255, 255, 255, luminance * 255},
        .obj = obj,
    };
    paint_2d_points(painter, 1, &point);

    // Render name if needed.
    if (*comet->name && (selected || (g_comets->hints_visible &&
                                      vmag < painter->hints_limit_mag + 2.0 +
                                      hints_mag_offset))) {
        if (selected)
            vec4_set(label_color, 1, 1, 1, 1);
        labels_add_3d(comet->name, FRAME_ICRF, comet->pvo[0], false, size + 4,
            FONT_SIZE_BASE - 2, label_color, 0, 0,
            TEXT_SEMI_SPACED | TEXT_BOLD | (selected ? 0 : TEXT_FLOAT),
            0, obj);
    }

    if (size > 1) {
        render_tail(comet, painter, TAIL_GAS);
        render_tail(comet, painter, TAIL_DUST);
    }
    return 1;
}

static int comets_init(obj_t *obj, json_value *args)
{
    comets_t *comets = (comets_t*)obj;
    assert(!g_comets);
    g_comets = comets;
    comets->visible = true;
    comets->hints_visible = true;
    regcomp(&comets->search_reg,
            "(([PCXDAI])/([0-9]+) [A-Z].+)|([0-9]+[PCXDAI]/.+)",
            REG_EXTENDED);
    return 0;
}

static int comets_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    comets_t *comets = (void*)obj;
    if (strcmp(key, "mpc_comets") != 0) return -1;
    comets->source_url = strdup(url);
    return 0;
}

static int comets_update(obj_t *obj, double dt)
{
    int size, code;
    const char *data;
    comets_t *comets = (void*)obj;

    if (comets->parsed || !comets->source_url)
        return 0;

    data = asset_get_data2(comets->source_url, ASSET_USED_ONCE, &size, &code);
    if (!code) return 0; // Still loading.
    comets->parsed = true;
    if (!data) {
        LOG_E("Cannot load comets data: %s (%d)", comets->source_url, code);
        return 0;
    }
    load_data(comets, data, size);

    // Make sure the search work.
    obj = core_search("NAME C/1995 O1 (Hale-Bopp)");
    assert(obj && strcmp(obj->klass->id, "mpc_comet") == 0);
    obj = core_search("NAME 1P/Halley");
    assert(obj && strcmp(obj->klass->id, "mpc_comet") == 0);
    return 0;
}

static void add_to_visible(comets_t *comets, comet_t *comet)
{
    if (comet->visible_prev) return;
    DL_APPEND2(comets->visibles, comet, visible_prev, visible_next);
}

static int comets_render(const obj_t *obj, const painter_t *painter)
{
    comets_t *comets = (void*)obj;
    int i, r;
    const int update_nb = 32;
    comet_t *child, *tmp;

    if (!comets->visible) return 0;

    // If the current selection is a comet, make sure it is flagged
    // as visible.
    if (core->selection && core->selection->parent == obj) {
        add_to_visible(comets, (void*)core->selection);
    }

    // Render all the flagged visible minor planets, remove those that are
    // no longer visible.
    DL_FOREACH_SAFE2(comets->visibles, child, tmp, visible_next) {
        r = comet_render(&child->obj, painter);
        if (r == 0 && &child->obj != core->selection) {
            DL_DELETE2(comets->visibles, child, visible_prev, visible_next);
            child->visible_prev = NULL;
        }
    }

    // Then iter part of the full list as well.
    child = comets->render_current ?: (void*)comets->obj.children;
    for (i = 0; child && i < update_nb; i++, child = (void*)child->obj.next) {
        if (child->visible_prev) continue; // Was already rendered.
        r = comet_render(&child->obj, painter);
        if (r == 1) add_to_visible(comets, child);
    }
    comets->render_current = child;

    return 0;
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
    .render_order   = 20,
    .attributes     = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(comets_t, visible)),
        PROPERTY(hints_mag_offset, TYPE_FLOAT,
                 MEMBER(comets_t, hints_mag_offset)),
        PROPERTY(hints_visible, TYPE_BOOL, MEMBER(comets_t, hints_visible)),
        {},
    },
};
OBJ_REGISTER(comets_klass)
