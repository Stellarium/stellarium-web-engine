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

/*
 * Type: meteor_t
 * Object that represents a single meteor
 */
typedef struct {
    obj_t       obj;
} meteor_t;

/*
 * Type: meteors_t
 * Meteors module object
 */
typedef struct {
    obj_t   obj;
    double  zhr;
} meteors_t;

static double frand(double from, double to)
{
    return from + (rand() / (double)RAND_MAX) * (to - from);
}

static int meteor_init(obj_t *obj, json_value *args)
{
    double z;
    double mat[3][3];

    // Give the meteor a random position and speed.
    // XXX: just testing values for the moment.
    z = (EARTH_RADIUS + MAX_ALTITUDE) * 1000 / DAU;
    vec4_copy(core->observer->pointer.icrs, obj->pos.pvg[0]);
    obj->pos.pvg[0][0] += frand(-0.2, +0.2);
    obj->pos.pvg[0][1] += frand(-0.2, +0.2);
    obj->pos.pvg[0][2] += frand(-0.2, +0.2);
    vec3_normalize(obj->pos.pvg[0], obj->pos.pvg[0]);
    vec3_mul(z, obj->pos.pvg[0], obj->pos.pvg[0]);
    obj->pos.pvg[0][3] = 1.0;
    mat3_set_identity(mat);
    mat3_mul_vec3(mat, obj->pos.pvg[0], obj->pos.pvg[0]);
    vec4_set(obj->pos.pvg[1], frand(-1, 1), frand(-1, 1), frand(-1, 1), 1);
    vec3_mul(0.00001, obj->pos.pvg[1], obj->pos.pvg[1]);
    return 0;
}

static int meteor_update(obj_t *obj, const observer_t *obs, double dt)
{
    vec3_addk(obj->pos.pvg[0], obj->pos.pvg[1], dt, obj->pos.pvg[0]);
    return 0;
}

/*
 * Project UV coordinates into a tail shape.
 * For the moment this projects into a triangle.
 */
static void tail_project(const projection_t *proj, int flags,
                         const double *v, double *out)
{
    double r, p[4] = {1, 0, 0, 0};
    double m[3][3];
    r = v[1];
    r *= 1 - v[0]; // Triangle shape by scaling along U.
    p[2] = (r - 0.5);
    mat3_set_identity(m);
    mat3_rz(v[0] * 10 * DD2R, m, m);
    mat3_mul_vec3(m, p, p);
    mat3_mul_vec3(proj->mat3, p, p);

    out[3] = 0;
    vec4_copy(p, out);
}

static void render_tail(const painter_t *painter,
                        const double p1[4],
                        const double p2[4])
{
    double mat[3][3];
    projection_t proj;
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

    proj = (projection_t) {
        .backward   = tail_project,
    };
    mat3_copy(mat, proj.mat3); // XXX: remove that I guess.
    paint_quad(painter, FRAME_CIRS, NULL, NULL, NULL, &proj, 8);
}

static int meteor_render(const obj_t *obj, const painter_t *painter)
{
    point_t point;
    double p1[4]; // Head position (CIRS)
    double p2[4]; // End of tail position (CIRS)
    double size, luminance;

    vec4_copy(obj->pos.pvg[0], p1);
    vec3_addk(p1, obj->pos.pvg[1], -2, p2);

    core_get_point_for_mag(4, &size, &luminance);
    point = (point_t) {
        .pos = {p1[0], p1[1], p1[2]},
        .size = size,
        .color = {1, 1, 1, luminance},
    };
    if (0)
    paint_points(painter, 1, &point, FRAME_CIRS);

    render_tail(painter, p1, p2);
    return 0;
}

static int meteors_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *child;
    int nb;

    obj_create("meteor", NULL, obj, NULL);

    OBJ_ITER(obj, child, "meteor")
        obj_update(child, obs, dt);

    DL_COUNT(obj->children, child, nb);
    if (nb > 100) {
        obj_remove(obj, obj->children);
    }
    return 0;
}

static int meteors_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, "meteor")
        obj_render(child, painter);
    return 0;
}

/*
 * Meta class declarations.
 */
static obj_klass_t meteor_klass = {
    .id             = "meteor",
    .size           = sizeof(meteor_t),
    .init           = meteor_init,
    .update         = meteor_update,
    .render         = meteor_render,
};
OBJ_REGISTER(meteor_klass)

static obj_klass_t meteors_klass = {
    .id             = "meteors",
    .size           = sizeof(meteors_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render_order   = 20,
    .update         = meteors_update,
    .render         = meteors_render,
    .attributes = (attribute_t[]) {
        PROPERTY("zhr", "f", MEMBER(meteors_t, zhr)),
        {}
    },
};
OBJ_REGISTER(meteors_klass)
