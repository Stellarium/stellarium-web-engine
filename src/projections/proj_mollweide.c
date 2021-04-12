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
#define DR2D (57.29577951308232087679815)
#define DAU (149597870.7e3)
#define DM2AU  (1. / DAU)

static bool proj_mollweide_project(
        const projection_t *proj, const double v[3], double out[3])
{
    double phi, lambda, theta, d, k, length;
    int i;
    const int MAX_ITER = 10;
    const double PRECISION = 1e-7;

    length = vec3_norm(v);
    // Computation using algo from wikipedia:
    // https://en.wikipedia.org/wiki/Mollweide_projection
    lambda = atan2(v[0], -v[2]);
    phi = atan2(v[1], sqrt(v[0] * v[0] + v[2] * v[2]));

    // We could optimize the iteration by computing 2 * theta instead.
    k = M_PI * sin(phi);
    theta = phi;
    for (i = 0; i < MAX_ITER; i++) {
        d = 2 + 2 * cos(2 * theta);
        if (fabs(d) < PRECISION) break;
        d = (2 * theta + sin(2 * theta) - k) / d;
        theta -= d;
        if (fabs(d) < PRECISION) break;
    }

    out[0] = 2 * sqrt(2) / M_PI * lambda * cos(theta);
    out[1] = sqrt(2) * sin(theta);
    out[2] = -1;
    vec3_mul(length, out, out);
    return true;
}

static double clamp(double x, double a, double b)
{
    return x < a ? a : x > b ? b : x;
}

static bool proj_mollweide_backward(const projection_t *proj,
            const double v[3], double out[3])
{
    double x, y, theta, phi, lambda, cp;
    bool ret = true;
    x = v[0];
    y = v[1];

    if (fabs(y) > sqrt(2)) {
        ret = false;
        y = clamp(y, -sqrt(2), sqrt(2));
    }

    theta = asin(y / sqrt(2));

    phi = asin((2 * theta + sin(2 * theta)) / M_PI);
    lambda = M_PI * x / (2 * sqrt(2) * cos(theta));

    if (fabs(lambda) > M_PI) {
        ret = false;
        lambda = clamp(lambda, -M_PI, M_PI);
    }

    cp = cos(phi);
    out[0] = cp * sin(lambda);
    out[1] = sin(phi);
    out[2] = -cp * cos(lambda);
    return ret;
}

static void proj_mollweide_compute_fov(int id, double fov, double aspect,
                                       double *fovx, double *fovy)
{
    *fovx = fov;
    *fovy = fov / aspect;
}

void proj_mollweide_init(projection_t *p, double fovy, double aspect)
{
    p->flags = PROJ_HAS_DISCONTINUITY;
    double fovy2 = 2 * atan(fovy / M_PI * sqrt(2));
    const double clip_near = 5 * DM2AU;
    mat4_inf_perspective(p->mat, fovy2 * DR2D, aspect, clip_near);
}

static const projection_klass_t proj_mollweide_klass = {
    .name                   = "mollweide",
    .id                     = PROJ_MOLLWEIDE,
    .max_fov                = 360 * DD2R,
    .max_ui_fov             = 360 * DD2R,
    .init                   = proj_mollweide_init,
    .project                = proj_mollweide_project,
    .backward               = proj_mollweide_backward,
    .compute_fovs           = proj_mollweide_compute_fov,
};
PROJECTION_REGISTER(proj_mollweide_klass);
