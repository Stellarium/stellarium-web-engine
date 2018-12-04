/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * REFERENCES :
 * Thanks to all the authors of the following papers I used for providing
 * their work freely online.
 *
 * [1] "Tone Reproduction for Realistic Images", Tumblin and Rushmeier,
 * IEEE Computer Graphics & Application, November 1993
 */

#ifndef TONEMAPPER_H
#define TONEMAPPER_H

#include <stdbool.h>

typedef struct tonemapper tonemapper_t;

/*
 * Function: tonemapper_create
 * Create a new tonemapper.
 *
 * Parameters:
 *   ldmax      - max display luminance in cd/m² (usually around 100).
 *   cmax       - cmax value (usually 35).
 *   ignore_add_term - set to true to ignore the addition term in the
 *                     tumbin algorithm.  This is just here so that we
 *                     properly simulate original stellarium tonemapping.
 */
tonemapper_t *tonemapper_create(float ldmax, float cmax, bool ignore_add_term);

/*
 * Function: tonemapper_set_adaptation_luminance
 * Update the tonemapper for a new adapatation luminance value.
 */
void tonemapper_set_adaptation_luminance(tonemapper_t *t, float lwa);

/*
 * Function: tonemapper_map
 * Compute the display value for a given world luminance
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
float tonemapper_map(const tonemapper_t *t, float lw);

/*
 * Function: tonemapper_map_log10
 * Same as tonemapper_map but take the log10 of the luminance as input.
 */
float tonemapper_map_log10(const tonemapper_t *t, float log_lw);

/*
 * Function: tonemapper_get_shader_args
 * Return tonmapper function values for shader usage.
 *
 * Return A, B and C, such as the tonemapping function is:
 * Ld = A * Lw^B + C
 */
void tonemapper_get_shader_args(const tonemapper_t *t,
        float *a, float *b, float *c);


#endif // TONEMAPPER_H
