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
 *
 * Most of this code comes as directly from Stellarium.
 */

typedef struct skybrightness
{

    float airMassMoon;
    float airMassSun;
    float magMoon;
    float RA;
    float K;
    float C3;
    float C4;
    float SN;
    float bNightTerm;
    float bMoonTerm1;
    float bTwilightTerm;

} skybrightness_t;

void skybrightness_prepare(
        skybrightness_t *sb,
        int year, int month, float moonPhase, float moonMag,
        float latitude, float altitude,
        float temperature, float relativeHumidity,
        float cosDistMoonZenith, float cosDistSunZenith);

float skybrightness_get_luminance(const skybrightness_t *sb,
        float cosDistMoon, float cosDistSun, float cosDistZenith);

#endif // SKYBRIGHTNESS_H
