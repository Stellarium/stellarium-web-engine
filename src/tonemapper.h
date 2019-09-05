/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Implementation of the Logarthmic Mapping discussed in:
 * "Quantization Techniques for Visualization of High Dynamic Range Pictures"
 * by Schlick 1994.
 */

#ifndef TONEMAPPER_H
#define TONEMAPPER_H

#include <stdbool.h>

typedef struct tonemapper
{
    float lwmax;
    float p;
    float q;
    float exposure;

    // precomputed terms.
    float s;
} tonemapper_t;

/*
 * Function: tonemapper_update
 * Update the tonemapper.
 *
 * Any parameters can be -1 to keep the current value.
 *
 * Parameters:
 *   p          - [0, inf]
 *   q          - [1, 3]
 *   exposure   - Exposure factor.
 *   lwmax      - Max luminance (cd/m²).
 */
void tonemapper_update(tonemapper_t *t,
                       float p, float q, float exposure, float lwmax);

/*
 * Function: tonemapper_map
 * Compute the display value for a given world luminance
 *
 * The function used in the logarithmic mapping discussed at the beginning
 * of Schlick 1994.
 *  Fp,q = pow( log(1 + p * lw) / log(1 + p * lwmax) , 1/q)
 *
 * The pow(.., 1/q) term is in fact the gamma correction, which we ignore
 * here because we apply it later, so we assume q == 1.
 *
 * Parameters:
 *   t      - A tonemapper.
 *   lw     - A luminance (cd/m²).
 *
 * Return:
 *   A screen value with 0 for black and 1 for full illuminance.
 *   No gamma correction is applied.  The value can be larger than one
 *   for saturated luminance.
 */
double tonemapper_map(const tonemapper_t *t, double lw);

/*
 * Function: tonemapper_map_log10
 * Same as tonemapper_map but take the log10 of the luminance as input.
 */
double tonemapper_map_log10(const tonemapper_t *t, double log_lw);


#endif // TONEMAPPER_H
