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

static void proj_stereographic_project(
        const projection_t *proj, int flags, const double v[4], double out[4])
{
    double one_over_h;
    vec3_copy(v, out);
    if (!(flags & PROJ_ALREADY_NORMALIZED)) vec3_normalize(out, out);
    // Discountinuity case.
    if (out[2] == 1.0) {
        memset(out, 0, 4 * sizeof(double));
        return;
    }
    one_over_h = 1.0 / (0.5 * (1.0 - out[2]));
    out[0] *= one_over_h / proj->scaling[0];
    out[1] *= one_over_h / proj->scaling[1];
    out[2] = 0.0; // Z = 0 => Center in the clipping space.
    out[3] = 1.0; // w value.
}

static bool proj_stereographic_backward(const projection_t *proj, int flags,
                                        const double v[2], double out[4])
{
    double lqq;
    double p[3] = {0};
    vec2_copy(v, p);
    p[0] *= proj->scaling[0];
    p[1] *= proj->scaling[1];
    lqq = 0.25 * (p[0] * p[0] + p[1] * p[1]);
    p[2] = lqq - 1.0;
    vec3_mul(1.0 / (lqq + 1.0), p, out);
    out[3] = 0.0;
    return true;
}

void proj_stereographic_compute_fov(double fov, double aspect,
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

void proj_stereographic_init(projection_t *p, double fovx, double aspect)
{
    p->name          = "stereographic";
    p->type          = PROJ_STEREOGRAPHIC;
    p->max_fov       = 360. * DD2R;
    p->max_ui_fov    = 185. * DD2R;
    p->project       = proj_stereographic_project;
    p->backward      = proj_stereographic_backward;
    p->scaling[0]    = 2 * tan(fovx / 4);
    p->scaling[1]    = p->scaling[0] / aspect;
}
