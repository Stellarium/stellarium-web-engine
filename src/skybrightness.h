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
    // Term for dark sky brightness computation
    float b_night_term;
    float K;

    // Air mass for Moon
    float airmass_moon;
    // Air mass for Sun
    float airmass_sun;
    float b_moon_term;
    // Term for moon brightness computation
    float C3;
    float b_twilight_term;
    // Term for sky brightness computation
    float C4;
} skybrightness_t;


void skybrightness_prepare(skybrightness_t *sb,
        int year, int month, float moon_phase,
        float latitude, float altitude,
        float temperature, float relative_humidity,
        float dist_moon_zenith, float dist_sun_zenith);

float skybrightness_get_luminance(
        const skybrightness_t *sb,
        float cos_moon_dist, float cos_sun_dist, float cos_zenith_dist);

#endif // SKYBRIGHTNESS_H
