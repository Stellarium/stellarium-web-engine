/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#define EARTH_RADIUS 6378.0         // Earth_radius in km
#define MAX_ALTITUDE 120.0          // Max meteor altitude in km
#define MIN_ALTITUDE 80.0           // Min meteor altitude in km

typedef struct meteor meteor_t;

/*
 * Type: meteor_t
 * Represents a single meteor
 */
struct meteor {
    meteor_t    *next, *prev;   // List within the meteors module.
    double      pvo[2][4];
    double      duration; // Duration (sec).
    double      time; // From 0 to duration.
};

typedef struct shower {
    obj_t obj;
    char iau_code[4];
    char designation[256];
    double pos[3];
    double zhr;

    // Times are in MJD, modulo one year.
    double start;
    double finish;
    double peak;

    json_value *data; // Original json data.
} shower_t;

/*
 * Type: meteors_t
 * Meteors module object
 */
typedef struct {
    obj_t   obj;
    double  zhr;
    meteor_t *meteors;
    char *showers_url;
    bool showers_loaded;
} meteors_t;

static double frand(double from, double to)
{
    return from + (rand() / (double)RAND_MAX) * (to - from);
}

static meteor_t *meteor_create(void)
{
    double z, mat[3][3];
    meteor_t *m = calloc(1, sizeof(*m));

    // Give the meteor a random position and speed.
    z = (EARTH_RADIUS + MAX_ALTITUDE) * 1000 * DM2AU;
    mat3_set_identity(mat);
    mat3_rz(frand(0, 360 * DD2R), mat, mat);
    mat3_ry(frand(-90 * DD2R, +90 * DD2R), mat, mat);
    mat3_mul_vec3(mat, VEC(1, 0, 0), m->pvo[0]);
    vec3_mul(z, m->pvo[0], m->pvo[0]);
    m->pvo[0][3] = 1.0;

    vec4_set(m->pvo[1], frand(-1, 1), frand(-1, 1), frand(-1, 1), 1);
    vec3_mul(0.00001, m->pvo[1], m->pvo[1]);

    m->duration = 4.0;
    return m;
}

static int meteor_update(meteor_t *m, double dt)
{
    vec3_addk(m->pvo[0], m->pvo[1], dt, m->pvo[0]);
    m->time += dt;
    return 0;
}

/*
 * Project UV coordinates into a tail shape.
 * For the moment this projects into a triangle.
 */
static void tail_project(const uv_map_t *map, const double v[2], double out[4])
{
    double r, p[4] = {1, 0, 0, 0};
    double m[3][3];
    r = v[1];
    r *= 1 - v[0]; // Triangle shape by scaling along U.
    p[2] = (r - 0.5);
    mat3_set_identity(m);
    mat3_rz(v[0] * 10 * DD2R, m, m);
    mat3_mul_vec3(m, p, p);
    mat3_mul_vec3(map->mat, p, p);

    p[3] = 1;
    vec4_copy(p, out);
}

static void render_tail(const painter_t *painter,
                        const double p1[4],
                        const double p2[4])
{
    double mat[3][3];
    uv_map_t map;
    /*
     * Compute the rotation/scale matrix that transforms X into p1 and
     * Y into p1 rotated 90° in the direction of p2.
     */
    vec3_copy(p1, mat[0]);
    vec3_copy(p2, mat[1]);
    vec3_cross(mat[0], mat[1], mat[2]);
    vec3_normalize(mat[0], mat[0]);
    vec3_normalize(mat[2], mat[2]);
    vec3_cross(mat[2], mat[0], mat[1]);

    // Scale along Z to specify the tail width.
    mat3_iscale(mat, 1, 1, 0.001);

    map = (uv_map_t) {
        .map = tail_project,
    };
    mat3_copy(mat, map.mat); // XXX: remove that I guess.
    paint_quad(painter, FRAME_ICRF, &map, 8);
}

static int meteor_render(const meteor_t *m, const painter_t *painter_)
{
    double p1[4]; // Head position (CIRS)
    double p2[4]; // End of tail position (CIRS)
    painter_t painter = *painter_;

    // Very basic fade out.
    painter.color[3] *= max(0.0, 1.0 - m->time / m->duration);

    vec4_copy(m->pvo[0], p1);
    vec3_addk(p1, m->pvo[1], -2, p2);

    render_tail(&painter, p1, p2);
    return 0;
}

static int meteors_init(obj_t *obj, json_value *args)
{
    meteors_t *ms = (meteors_t*)obj;
    ms->zhr = 10; // Normal rate.
    return 0;
}

static int parse_date(const char *str, double *out)
{
    int m, d, r;
    double djm0, djm;
    if (sscanf(str, "%d-%d", &m, &d) != 2) {
        LOG_E("Malformed meteor shower date: %s", str);
        return -1;
    }
    r = eraCal2jd(2000, m, d, &djm0, &djm);
    if (r != 0) return -1;
    assert(djm0 == ERFA_DJM0);
    *out = fmod(djm, ERFA_DJY);
    return 0;
}

static shower_t *create_shower(const json_value *doc)
{
    const char *iau_code, *designation, *start, *finish, *peak;
    double ra, dec, speed, zhr;
    int r, iau_number;
    shower_t *s = NULL;

    r = jcon_parse(doc, "{",
        "iau_code", JCON_STR(iau_code),
        "iau_number", JCON_INT(iau_number, 0),
        "designation", JCON_STR(designation),
        "start", JCON_STR(start),
        "finish", JCON_STR(finish),
        "peak", JCON_STR(peak),
        "ra", JCON_DOUBLE(ra, 0),
        "dec", JCON_DOUBLE(dec, 0),
        "speed", JCON_DOUBLE(speed, 0),
        "zhr", JCON_DOUBLE(zhr, 0),
    "}");
    if (r != 0) goto error;

    s = (void*)obj_create("meteor-shower", NULL);
    eraS2c(ra * DD2R, dec * DD2R, s->pos);
    snprintf(s->iau_code, sizeof(s->iau_code), "%s", iau_code);
    snprintf(s->designation, sizeof(s->designation), "%s", designation);
    strncpy(s->obj.type, "MSh", 4);
    if (parse_date(start, &s->start)) goto error;
    if (parse_date(finish, &s->finish)) goto error;
    if (parse_date(peak, &s->peak)) goto error;
    s->data = json_copy(doc);
    return s;

error:
    LOG_E("Error parsing meteor shower json.");
    obj_release((obj_t*)s);
    return NULL;
}

static void load_showers(meteors_t *ms)
{
    int i, size, code, nb = 0;
    const char *data;
    json_value *doc = NULL, *showers;
    shower_t *shower;

    if (ms->showers_loaded || !ms->showers_url) return;

    data = asset_get_data2(ms->showers_url, ASSET_USED_ONCE, &size, &code);
    if (!code) return; // Sill loading.
    ms->showers_loaded = true;
    if (!data) goto error;

    doc = json_parse(data, strlen(data));
    if (!doc) goto error;
    showers = json_get_attr(doc, "showers", json_array);
    if (!showers) goto error;

    for (i = 0; i < showers->u.array.length; i++) {
        shower = create_shower(showers->u.array.values[i]);
        if (!shower) continue;
        module_add(&ms->obj, &shower->obj);
        nb++;
    }
    LOG_I("Added %d meteor showers", nb);
    json_value_free(doc);
    return;

error:
    LOG_E("Cannot parse meteor shower data");
    json_value_free(doc);
    return;
}

static void shower_get_designations(
        const obj_t *obj, void *user,
        int (*f)(const obj_t *obj, void *user,
                 const char *cat, const char *str))
{
    shower_t *s = (void*)obj;
    f(obj, user, "NAME", s->designation);
}

static int shower_get_pvo(const shower_t *s, const observer_t *obs,
                          double pvo[2][4])
{
    convert_frame(obs, FRAME_ASTROM, FRAME_ICRF, true, s->pos, pvo[0]);
    pvo[0][3] = 0.0;
    pvo[1][0] = pvo[1][1] = pvo[1][2] = pvo[1][3] = 0.0;
    return 0;
}

static double shower_get_next_peak(const shower_t *s, const observer_t *obs)
{
    double ret;
    ret = obs->utc - fmod(obs->utc, ERFA_DJY) + fmod(s->peak, ERFA_DJY);
    if (ret < obs->utc) ret += ERFA_DJY;
    return ret;
}

static int shower_get_info(const obj_t *obj, const observer_t *obs, int info,
                           void *out)
{
    shower_t *s = (void*)obj;
    switch (info) {
    case INFO_PVO:
        shower_get_pvo(s, obs, out);
        return 0;
    case INFO_NEXT_PEAK:
        *(double*)out = shower_get_next_peak(s, obs);
        return 0;
    default:
        return 1;
    }
}

static int shower_render(const obj_t *obj, const painter_t *painter)
{
    const double color[4] = {1, 1, 1, 1};
    const double size[2] = {30, 30};
    double win_pos[2];
    shower_t *s = (void*)obj;

    // Only render if selected.
    if (core->selection != obj) return 0;
    if (!painter_project(painter, FRAME_ASTROM, s->pos, true, true, win_pos))
        return 0;
    symbols_paint(painter, SYMBOL_METEOR_SHOWER, win_pos, size, color, 0);
    labels_add_3d(sys_translate("sky", s->designation),
                  FRAME_ASTROM, s->pos, true, size[0] / 2,
                  FONT_SIZE_BASE, color, 0, 0, TEXT_BOLD, 0, obj);
    return 0;
}

static int shower_render_pointer(const obj_t *obj, const painter_t *painter)
{
    // Don't render the pointer.
    return 0;
}

static json_value *shower_get_json_data(const obj_t *obj)
{
    const shower_t *s = (void*)obj;
    json_value *ret = json_object_new(0);
    json_object_push(ret, "model_data", json_copy(s->data));
    return ret;
}

static int meteors_update(obj_t *obj, double dt)
{
    meteors_t *ms = (meteors_t*)obj;
    meteor_t *m, *tmp;
    int nb, max_nb = 100;
    double proba;

    load_showers(ms);

    DL_COUNT(ms->meteors, m, nb);
    // Probabiliy of having a new shooting star at this frame.
    proba = ms->zhr * dt / 3600;

    if (nb < max_nb && frand(0, 1) < proba) {
        m = meteor_create();
        DL_APPEND(ms->meteors, m);
    }

    DL_FOREACH_SAFE(ms->meteors, m, tmp) {
        meteor_update(m, dt);
        if (m->time > m->duration) {
            DL_DELETE(ms->meteors, m);
            free(m);
        }
    }

    return 0;
}

static int meteors_render(const obj_t *obj, const painter_t *painter)
{
    const meteors_t *meteors = (void*)obj;
    obj_t *child;
    meteor_t *m;

    DL_FOREACH(meteors->meteors, m) {
        meteor_render(m, painter);
    }

    DL_FOREACH(meteors->obj.children, child) {
        obj_render(child, painter);
    }
    return 0;
}

static int meteors_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    meteors_t *ms = (void*)obj;
    if (strcmp(key, "json/meteor-showers") != 0) return -1;
    ms->showers_url = strdup(url);
    return 0;
}

static int meteors_list(const obj_t *obj,
                        double max_mag, uint64_t hint,
                        const char *sources, void *user,
                        int (*f)(void *user, obj_t *obj))
{
    obj_t *child;
    DL_FOREACH(obj->children, child) {
        if (f(user, child)) break;
    }
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t meteor_shower_klass = {
    .id             = "meteor-shower",
    .size           = sizeof(shower_t),
    .render_order   = 20,
    .get_designations = shower_get_designations,
    .get_info       = shower_get_info,
    .render         = shower_render,
    .render_pointer = shower_render_pointer,
    .get_json_data  = shower_get_json_data,
    .attributes = (attribute_t[]) {
        {}
    },
};
OBJ_REGISTER(meteor_shower_klass)

static obj_klass_t meteors_klass = {
    .id             = "meteors",
    .size           = sizeof(meteors_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render_order   = 20,
    .init           = meteors_init,
    .update         = meteors_update,
    .render         = meteors_render,
    .add_data_source = meteors_add_data_source,
    .list           = meteors_list,
    .attributes = (attribute_t[]) {
        PROPERTY(zhr, TYPE_FLOAT, MEMBER(meteors_t, zhr)),
        {}
    },
};
OBJ_REGISTER(meteors_klass)
