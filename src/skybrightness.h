/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef SKYBRIGHTNESS_H
#define SKYBRIGHTNESS_H

/*
 * Atmosphere brightness computation, based on the 1998 sky brightness model by
 * Bradley Schaefer:
 * B. Schaefer: To the Visual Limits. Sky&Telescope 5/1998 57-60.
 */

typedef struct skybrightness
{
    float Y, M, AM, LA, AL, TE, RH, ZM, ZS;
    float max_BM;

    // Precomputed values.
    float K, XM, XS;

} skybrightness_t;

void skybrightness_prepare(
        skybrightness_t *sb,
        int year, int month, float moon_phase,
        float latitude, float altitude,
        float temperature, float relative_humidity,
        float dist_moon_zenith, float dist_sun_zenith,
        float max_moon_brightness);

float skybrightness_get_luminance(
        const skybrightness_t *sb,
        float moon_dist, float sun_dist, float zenith_dist);

#endif // SKYBRIGHTNESS_H
