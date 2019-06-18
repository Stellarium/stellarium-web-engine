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

static void proj_mercator_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double s;
    double p[3];
    vec3_copy(v, p);

    if (!(flags & PROJ_ALREADY_NORMALIZED)) vec3_normalize(p, p);
    s = p[1];
    p[0] = atan2(p[0], -p[2]);
    if (fabs(s) != 1)
        p[1] = 0.5 * log((1 + s) / (1 - s));
    else
        p[1] = 1024;  // Just use an arbitrary large value.

    if (proj->shift == +1 && p[0] < 0) p[0] += 2 * M_PI;
    if (proj->shift == -1 && p[0] > 0) p[0] -= 2 * M_PI;

    p[0] /= proj->scaling[0];
    p[1] /= proj->scaling[1];
    vec3_copy(p, out);
}

static bool proj_mercator_backward(const projection_t *proj, int flags,
            const double *v, double *out)
{
    double e, h, h1, sin_delta, cos_delta;
    double p[3];
    bool ret;
    vec3_copy(v, p);

    p[0] *= proj->scaling[0];
    p[1] *= proj->scaling[1];

    ret = p[1] < M_PI_2 && p[1] > -M_PI_2 && p[0] > -M_PI && p[0] < M_PI;

    e = exp(p[1]);
    h = e * e;
    h1 = 1.0 / (1.0 + h);
    sin_delta = (h - 1.0) * h1;
    cos_delta = 2.0 * e * h1;
    p[2] = - cos_delta * cos(p[0]);
    p[0] = cos_delta * sin(p[0]);
    p[1] = sin_delta;
    vec3_copy(p, out);
    return ret;
}

static int proj_mercator_intersect_discontinuity(const projection_t *p,
        const double a[3], const double b[3])
{
    double x0, x1;
    if (a[0] * b[0] > 0)
        return 0;
    if (a[2] < 0 && b[2] < 0)
        return PROJ_CANNOT_SPLIT;
    if (a[2] > 0 && b[2] > 0)
        return PROJ_INTERSECT_DISCONTINUITY;
    x0 = atan2(a[0], -a[2]);
    x1 = atan2(b[0], -b[2]);
    if (fabs(x0) + fabs(x1) >= M_PI) {
        return PROJ_INTERSECT_DISCONTINUITY;
    } else {
        return PROJ_CANNOT_SPLIT;
    }
}

static void proj_mercator_split(const projection_t *p, projection_t *out)
{
    int i;
    for (i = 0; i < 2; i++) {
        out[i] = *p;
        out[i].shift = i ? +1 : -1;
        out[i].intersect_discontinuity = NULL;
        out[i].split = NULL;
    }
}

void proj_mercator_compute_fov(double fov, double aspect,
                               double *fovx, double *fovy)
{
    // Is that correct?
    if (aspect < 1) {
        *fovx = fov;
        *fovy = fov / aspect;
    } else {
        *fovy = fov;
        *fovx = fov * aspect;
    }
}

void proj_mercator_init(projection_t *p, double fov, double aspect)
{
    p->name                      = "mercator";
    p->type                      = PROJ_MERCATOR;
    p->max_fov                   = 175.0 * aspect * DD2R;
    p->project                   = proj_mercator_project;
    p->backward                  = proj_mercator_backward;
    p->intersect_discontinuity   = proj_mercator_intersect_discontinuity;
    p->split                     = proj_mercator_split;
    p->scaling[0]                = fov / 2;
    p->scaling[1]                = p->scaling[0] / aspect;
}

