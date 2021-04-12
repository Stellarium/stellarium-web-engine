/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <float.h>

/*
 * Stereographic projection.
 *
 *                    -z ^              x' = 2 * tan(θ / 2)
 *                       |                 = 2 * x / (1 - z)
 *    -------------------+------------+------
 *                   ,---|---.       /
 *                ,-'    |    `-.   /
 *              ,'       |       `./ x,z
 *             /         |     .' /\
 *            /          | θ .'  /  \
 *           ;           | .'   /    :
 *           |           X ----/-----|-----> x
 *           :                /      ;
 *            \              /      /
 *             \            /      /
 *              `.         /     ,'
 *                '-.     /   ,-'
 *                   `---+---'
 *                      z = 1
 *
 *
 * Note: the algorithm will for the moment return x = y = x = w = 0 for the
 * point (0, 0, 1), since this is a discontinuity.  We should maybe implement
 * the intersect_discontinuity method to handle this case properly, I am not
 * totally sure about the best way to do that.
 */

static bool proj_stereographic_project(
        const projection_t *proj, const double v[3], double out[3])
{
    double d, one_over_h;

    d = vec3_norm(v);
    vec3_mul(1. / d, v, out);
    // Discountinuity case.
    if (out[2] == 1.0) {
        memset(out, 0, 4 * sizeof(double));
        return false;
    }
    one_over_h = 1.0 / (0.5 * (1.0 - out[2]));
    out[0] *= one_over_h;
    out[1] *= one_over_h;
    out[2] = -1;
    vec3_mul(d, out, out);
    return true;
}

static bool proj_stereographic_backward(const projection_t *proj,
                                        const double v[3], double out[3])
{
    double lqq;
    double p[3] = {0};
    vec2_copy(v, p);
    lqq = 0.25 * (p[0] * p[0] + p[1] * p[1]);
    p[2] = lqq - 1.0;
    vec3_mul(1.0 / (lqq + 1.0), p, out);
    return true;
}

static void proj_stereographic_compute_fov(int id, double fov, double aspect,
                                           double *fovx, double *fovy)
{
    if (aspect < 1) {
        *fovx = fov;
        *fovy = 4 * atan(tan(fov / 4) / aspect);
    } else {
        *fovy = fov;
        *fovx = 4 * atan(tan(fov / 4) * aspect);
    }
}

static void proj_stereographic_init(projection_t *p, double fovy, double aspect)
{
    double fovy2 = 2 * atan(2 * tan(fovy / 4));
    const double clip_near = 5 * DM2AU;
    mat4_inf_perspective(p->mat, fovy2 * DR2D, aspect, clip_near);
}

static const projection_klass_t proj_stereographic_klass = {
    .name           = "stereographic",
    .id             = PROJ_STEREOGRAPHIC,
    .max_fov        = 360. * DD2R,
    .max_ui_fov     = 185. * DD2R,
    .init           = proj_stereographic_init,
    .project        = proj_stereographic_project,
    .backward       = proj_stereographic_backward,
    .compute_fovs   = proj_stereographic_compute_fov,
};
PROJECTION_REGISTER(proj_stereographic_klass);
