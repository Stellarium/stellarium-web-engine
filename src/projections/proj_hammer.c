/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
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

static void proj_hammer_project(
        const projection_t *proj, int flags, const double v[4], double out[4])
{
    double r = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    double alpha = atan2(v[0], -v[2]);
    double cos_delta = sqrt(1 - v[1] * v[1] / (r * r));
    double z = sqrt(1 + cos_delta * cos(alpha / 2));
    out[0] = 2 * sqrt(2) * cos_delta * sin(alpha / 2) / z / proj->scaling[0];
    out[1] = sqrt(2) * v[1] / r / z / proj->scaling[1];
    out[2] = 0;
    out[3] = 1;
}

static bool proj_hammer_backward(const projection_t *proj, int flags,
            const double v[2], double out[4])
{
    double p[3] = {0}, zsq, z, alpha, delta, cd;
    bool ret;
    vec2_copy(v, p);
    p[0] *= proj->scaling[0];
    p[1] *= proj->scaling[1];
    zsq = 1 - 0.25 * 0.25 * p[0] * p[0] - 0.5 * 0.5 * p[1] * p[1];
    ret = 0.25*p[0]*p[0]+p[1]*p[1] < 2.0;
    z = zsq < 0 ? 0 : sqrt(zsq);
    alpha = 2 * atan2(z * p[0], (2 * (2 * zsq - 1)));
    delta = asin(p[1] * z);
    cd = cos(delta);
    out[0] = cd * sin(alpha);
    out[1] = p[1] * z;
    out[2] = -cd * cos(alpha);
    out[3] = 0;
    return ret;
}

void proj_hammer_init(projection_t *p, double fov, double aspect)
{
    p->scaling[0] = aspect < 1 ? fov / 2 : fov / aspect / 2;
    p->scaling[1] = p->scaling[0] / aspect;
    p->flags = PROJ_HAS_DISCONTINUITY;
}

static const projection_klass_t proj_hammer_klass = {
    .name                   = "hammer",
    .id                     = PROJ_HAMMER,
    .max_fov                = 360 * DD2R,
    .max_ui_fov             = 360 * DD2R,
    .init                   = proj_hammer_init,
    .project                = proj_hammer_project,
    .backward               = proj_hammer_backward,
};
PROJECTION_REGISTER(proj_hammer_klass);
