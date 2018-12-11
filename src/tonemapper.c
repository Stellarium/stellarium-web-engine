/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "tonemapper.h"

#include <math.h>
#include <stdlib.h>

#define exp10f(x) expf((x) * logf(10.f))

struct tonemapper
{
    float lwa;      // Adaptation luminance.
    float ldmax;
    float cmax;
    bool ignore_add_term;

    // Computed values.
    float a, b, c;
};

static float sqr(float x) {return x * x;}

tonemapper_t *tonemapper_create(float ldmax, float cmax, bool ignore_add_term)
{
    tonemapper_t *t = calloc(1, sizeof(*t));

    t->ldmax = ldmax;
    t->cmax = cmax;
    t->ignore_add_term = ignore_add_term;
    return t;
}

void tonemapper_set_adaptation_luminance(tonemapper_t *t, float lwa)
{
    float a_rw, b_rw, lwd, ldmax, cmax, a_d, b_d;

    t->lwa = lwa;

    ldmax = t->ldmax;
    cmax = t->cmax;
    a_rw = 0.4f * log10f(lwa) + 2.92f;
    b_rw = -0.4f * sqr(log10f(lwa)) - 2.584f * log10f(lwa) + 2.0208f;
    lwd = ldmax / sqrtf(cmax);
    a_d = 0.4f * log10f(lwd) + 2.92f;
    b_d = -0.4f * sqr(log10f(lwd)) - 2.584f * log10f(lwd) + 2.0208f;

    // Precomputed values to speed up algo.
    t->a = exp10f((b_rw - b_d) / a_d) / ldmax;
    t->b = a_rw / a_d;
    t->c = t->ignore_add_term ? 0 : -1.f / cmax;
}

float tonemapper_map(const tonemapper_t *t, float lw)
{
    // Implementation of the Tumblin tonemapping algorithm.
    return t->a * powf(lw, t->b) + t->c;
}

float tonemapper_map_log10(const tonemapper_t *t, float log_lw)
{
    return t->a * exp10f(log_lw * t->b) + t->c;
}

void tonemapper_get_shader_args(const tonemapper_t *t,
        float *a, float *b, float *c)
{
    *a = t->a;
    *b = t->b;
    *c = t->c;
}
