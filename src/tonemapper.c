/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "tonemapper.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#ifndef exp10
#define exp10(x) exp((x) * log(10.0))
#endif

void tonemapper_update(tonemapper_t *t,
                       float p, float q, float exposure, float lwmax)
{
    if (lwmax != -1) t->lwmax = lwmax;
    if (exposure != -1) t->exposure = exposure;
    if (p != -1) t->p = p;
    if (q != -1) t->q = q;
    assert(t->q == 1.0);
}

double tonemapper_map(const tonemapper_t *t, double lw)
{
    // Assume q = 1, so that we can skip the pow call.
    return log(1.0 + t->p * lw) / log(1.0 + t->p * t->lwmax) * t->exposure;
}

double tonemapper_map_log10(const tonemapper_t *t, double log_lw)
{
    return tonemapper_map(t, exp10(log_lw));
}
