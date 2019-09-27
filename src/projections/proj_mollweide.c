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
        const projection_t *proj, int flags, const double *v, double *out)
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
            const double *v, double *out)
{
    // XXX: this is the code from the hammer projection!
    // We need to use the proper function.
    double p[3], zsq, z, alpha, delta, cd;
    bool ret;
    vec3_copy(v, p);
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
    return ret;
}

void proj_mollweide_compute_fov(double fov, double aspect,
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

static bool proj_mollweide_intersect_discontinuity(const projection_t *p,
        const double a[3], const double b[3])
{
    // XXX: probably over complicated!
    double x0, x1;
    if (a[0] * b[0] > 0)
        return false;
    if (a[2] < 0 && b[2] < 0)
        return false;
    if (a[2] > 0 && b[2] > 0)
        return true;
    x0 = atan2(a[0], -a[2]);
    x1 = atan2(b[0], -b[2]);
    if (fabs(x0) + fabs(x1) >= M_PI) {
        return true;
    } else {
        return false;
    }
}

void proj_mollweide_init(projection_t *p, double fov, double aspect)
{
    p->name                      = "mollweide";
    p->type                      = PROJ_MOLLWEIDE;
    p->max_fov                   = 360 * DD2R;
    p->project                   = proj_mollweide_project;
    p->backward                  = proj_mollweide_backward;
    p->intersect_discontinuity   = proj_mollweide_intersect_discontinuity;
    p->scaling[0]                = aspect < 1 ? fov / 2 : fov / aspect / 2;
    p->scaling[1]                = p->scaling[0] / aspect;
}

