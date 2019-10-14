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

static void proj_mollweide_project(
        const projection_t *proj, int flags, const double v[4], double out[4])
{
    double phi, lambda, d, k;
    int i;
    const int MAX_ITER = 10;
    const double PRECISION = 1e-7;

    // Computation using algo from wikipedia:
    // https://en.wikipedia.org/wiki/Mollweide_projection
    lambda = atan2(v[0], -v[2]);
    phi = atan2(v[1], sqrt(v[0] * v[0] + v[2] * v[2]));

    // We could optimize the iteration by computing 2 * theta instead.
    k = M_PI * sin(phi);
    for (i = 0; i < MAX_ITER; i++) {
        d = 2 + 2 * cos(2 * phi);
        if (fabs(d) < PRECISION) break;
        d = (2 * phi + sin(2 * phi) - k) / d;
        phi -= d;
        if (fabs(d) < PRECISION) break;
    }

    out[0] = lambda * cos(phi) / proj->scaling[0];
    out[1] = sqrt(2) * sin(phi) / proj->scaling[1];
    out[2] = 0;
    out[3] = 1;
}

static bool proj_mollweide_backward(const projection_t *proj, int flags,
            const double v[2], double out[4])
{
    double x, y, theta, phi, lambda, cp;
    bool ret = true;
    x = v[0] * proj->scaling[0];
    y = v[1] * proj->scaling[1];

    if (fabs(y) > sqrt(2)) {
        ret = false;
        y = 0;
    }

    theta = asin(y / sqrt(2));
    phi = asin((2 * theta + sin(2 * theta)) / M_PI);
    lambda = x / cos(theta);

    cp = cos(lambda);
    out[1] = cp * sin(phi);
    out[0] = sin(lambda);
    out[2] = -cp * cos(phi);
    out[3] = 0;
    return ret;
}

void proj_mollweide_init(projection_t *p, double fov, double aspect)
{
    p->name                      = "mollweide";
    p->type                      = PROJ_MOLLWEIDE;
    p->max_fov                   = 360 * DD2R;
    p->project                   = proj_mollweide_project;
    p->backward                  = proj_mollweide_backward;
    p->scaling[0]                = aspect < 1 ? fov / 2 : fov / aspect / 2;
    p->scaling[1]                = p->scaling[0] / aspect;
    p->flags                     = PROJ_HAS_DISCONTINUITY;
}

