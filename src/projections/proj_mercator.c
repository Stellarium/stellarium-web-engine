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
        const projection_t *proj, int flags, const double v[4], double out[4])
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

    p[0] /= proj->scaling[0];
    p[1] /= proj->scaling[1];
    vec3_copy(p, out);
    out[3] = 1.0;
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

void proj_mercator_init(projection_t *p, double fov, double aspect)
{
    p->name                      = "mercator";
    p->type                      = PROJ_MERCATOR;
    p->max_fov                   = 360 * DD2R;
    p->max_ui_fov                = 175.0 * aspect * DD2R;
    p->project                   = proj_mercator_project;
    p->backward                  = proj_mercator_backward;
    p->scaling[0]                = fov / 2;
    p->scaling[1]                = p->scaling[0] / aspect;
    p->flags                     = PROJ_HAS_DISCONTINUITY;
}

