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

void tonemapper_update(tonemapper_t *t,
                       float p, float q, float exposure, float lwmax)
{
    if (lwmax != -1) t->lwmax = lwmax;
    if (exposure != -1) t->exposure = exposure;
    if (p != -1) t->p = p;
    if (q != -1) t->q = q;
}

float tonemapper_map(const tonemapper_t *t, float lw)
{
    return powf(logf(1.f + t->p * lw) / logf(1.f + t->p * t->lwmax),
                1.f / t->q);
}

float tonemapper_map_log10(const tonemapper_t *t, float log_lw)
{
    return tonemapper_map(t, exp10f(log_lw));
}
