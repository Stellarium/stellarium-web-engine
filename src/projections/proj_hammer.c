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

static bool proj_hammer_project(
        const projection_t *proj, const double v[3], double out[3])
{
    double r = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    double alpha = atan2(v[0], -v[2]);
    double cos_delta = sqrt(1 - v[1] * v[1] / (r * r));
    double z = sqrt(1 + cos_delta * cos(alpha / 2));
    out[0] = 2 * sqrt(2) * cos_delta * sin(alpha / 2) / z;
    out[1] = sqrt(2) * v[1] / r / z;
    out[2] = -1;
    vec3_mul(r, out, out);
    return true;
}

static bool proj_hammer_backward(const projection_t *proj,
            const double v[3], double out[3])
{
    double p[3] = {0}, zsq, z, alpha, delta, cd;
    bool ret;
    vec2_copy(v, p);
    zsq = 1 - 0.25 * 0.25 * p[0] * p[0] - 0.5 * 0.5 * p[1] * p[1];
    ret = 0.25*p[0]*p[0]+p[1]*p[1] < 2.0;
    z = zsq < 0 ? 0 : sqrt(zsq);
    alpha = 2 * atan2(z * p[0], (2 * (2 * zsq - 1)));
    delta = asin(p[1] * z);
    cd = cos(delta);
    out[0] = cd * sin(alpha);
    out[1] = p[1] * z;
    out[2] = -cd * cos(alpha);
    return ret;
}

void proj_hammer_init(projection_t *p, double fov, double aspect)
{
    p->flags = PROJ_HAS_DISCONTINUITY;
    // XXX: set the projection matrix!
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
