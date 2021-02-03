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

static void proj_mollweide_project(
        const projection_t *proj, int flags, const double v[4], double out[4])
{
    double phi, lambda, theta, d, k;
    int i;
    const int MAX_ITER = 10;
    const double PRECISION = 1e-7;

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

    out[0] /= proj->scaling[0];
    out[1] /= proj->scaling[1];

    out[2] = 0;
    out[3] = 1;
}

static double clamp(double x, double a, double b)
{
    return x < a ? a : x > b ? b : x;
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
    out[3] = 0;
    return ret;
}

void proj_mollweide_compute_fov(double fov, double aspect,
                                double *fovx, double *fovy)
{
    *fovx = fov;
    *fovy = fov / aspect;
}

static inline double smoothstep(double edge0, double edge1, double x)
{
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

static inline double mix(double x, double y, double t)
{
    return x * (1.0 - t) + y * t;
}

void proj_mollweide_init(projection_t *p, double fovx, double aspect)
{
    p->name                      = "mollweide";
    p->type                      = PROJ_MOLLWEIDE;
    p->max_fov                   = 360 * DD2R;
    p->max_ui_fov                = 360 * DD2R;
    p->project                   = proj_mollweide_project;
    p->backward                  = proj_mollweide_backward;
    p->scaling[0]                = fovx / M_PI * sqrt(2);
    p->scaling[1]                = p->scaling[0] / aspect;
    p->flags                     = PROJ_HAS_DISCONTINUITY;
}

void proj_mollweide_adaptive_init(projection_t *p, double fovx, double aspect)
{
    // μ ellipse ratio such that the scale at the equator equals one.
    // http://master.grad.hr/hdgg/kog_stranica/kog15/2Lapaine-KoG15.pdf
    double mu = M_PI * M_PI / 4;
    double scale = smoothstep(180, 360, fovx * DR2D);
    proj_mollweide_init(p, fovx, aspect);
    p->scaling[1] *= mix(mu / 2, 1, scale);
}
