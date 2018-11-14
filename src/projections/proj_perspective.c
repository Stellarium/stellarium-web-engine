/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "projection.h"
#include "utils/vec.h"

/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

static void proj_perspective_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double v4[4] = {v[0], v[1], v[2], 1};
    mat4_mul_vec4((void*)proj->mat, v4, out);
}

static void proj_perspective_backward(const projection_t *proj, int flags,
        const double *v, double *out)
{
    double p[3], r;
    vec3_copy(v, p);

    p[0] *= proj->scaling[0];
    p[1] *= proj->scaling[1];

    r = sqrt(1.0 + p[0] * p[0] + p[1] * p[1]);
    p[0] /= r;
    p[1] /= r;
    p[2] = -1.0 / r;

    vec3_copy(p, out);
}

void proj_perspective_init(projection_t *p, double fov, double aspect)
{
    double fovy;
    fovy = atan(tan(fov / 2) / aspect) * 2;
    mat4_perspective(p->mat, fovy / DD2R, aspect, 0, 256);
    p->name = "perspective";
    p->type = PROJ_PERSPECTIVE;
    p->max_fov = 120. * DD2R;
    p->project = proj_perspective_project;
    p->backward = proj_perspective_backward;
    p->scaling[0] = tan(fov / 2);
    p->scaling[1] = p->scaling[0] / aspect;
}

