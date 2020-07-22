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

/*
 * Type: meteors_t
 * Meteors module object
 */
typedef struct {
    obj_t   obj;
    double  zhr;
    meteor_t *meteors;
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
    z = (EARTH_RADIUS + MAX_ALTITUDE) * 1000 / DAU;
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

static int meteors_update(obj_t *obj, double dt)
{
    PROFILE(meterors_update, 0);
    meteors_t *ms = (meteors_t*)obj;
    meteor_t *m, *tmp;
    int nb, max_nb = 100;
    double proba;

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
    PROFILE(meterors_render, 0);
    const meteors_t *meteors = (void*)obj;
    meteor_t *m;

    DL_FOREACH(meteors->meteors, m) {
        meteor_render(m, painter);
    }
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t meteors_klass = {
    .id             = "meteors",
    .size           = sizeof(meteors_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render_order   = 20,
    .init           = meteors_init,
    .update         = meteors_update,
    .render         = meteors_render,
    .attributes = (attribute_t[]) {
        PROPERTY(zhr, TYPE_FLOAT, MEMBER(meteors_t, zhr)),
        {}
    },
};
OBJ_REGISTER(meteors_klass)
