/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#define _GNU_SOURCE
#include <math.h>

#include "skybrightness.h"

void skybrightness_prepare(
        skybrightness_t *sb,
        int year, int month, float moonPhase, float moonMag,
        float latitude, float altitude,
        float temperature, float relativeHumidity,
        float cosDistMoonZenith, float cosDistSunZenith)
{
    sb->magMoon = moonMag;
    // GZ: Bah, a very crude estimate for the solar position...
    sb->RA = (month - 3) * 0.52359878f;
    // Term for dark sky brightness computation.
    // GZ: This works for a few 11-year solar cycles around 1992...
    // ... cos((y-1992)/11 * 2pi)
    sb->bNightTerm = 1.0e-13 + 0.3e-13 * cosf(0.57118f * (year - 1992));

    float sign_latitude = (latitude >= 0.f) * 2.f - 1.f;
    // extinction Coefficient for V band
    // GZ TODO: re-create UBVRI for colored extinction, and get RGB extinction
    // factors from SkyBright!
    float KR = 0.1066f * expf(-altitude / 8200.f); // Rayleigh
    float KA = 0.1f * expf(-altitude / 1500.f) *
        powf(1.f - 0.32f / logf(relativeHumidity / 100.f), 1.33f) *
        (1.f + 0.33f * sign_latitude * sinf(sb->RA)); // Aerosol
    float KO = 0.031f * expf(-altitude / 8200.f) *
        (3.f + 0.4f * (latitude * cosf(sb->RA) -
                       cosf(3.f*latitude))) / 3.f; // Ozone
    float KW = 0.031f * 0.94f * (relativeHumidity / 100.f)
        * expf(temperature / 15.f) * expf(-altitude / 8200.f); // Water
    sb->K = KR + KA + KO + KW; // Total extinction coefficient

    // Air mass for Moon
    if (cosDistMoonZenith < 0) sb->airMassMoon = 40.f;
    else sb->airMassMoon = 1.f / (cosDistMoonZenith + 0.025f *
            expf(-11.f * cosDistMoonZenith));

    // Air mass for Sun
    if (cosDistSunZenith < 0) sb->airMassSun = 40.f;
    else sb->airMassSun = 1.f / (cosDistSunZenith + 0.025f *
            expf(-11.f * cosDistSunZenith));

    sb->bMoonTerm1 = exp10(-0.4f * (sb->magMoon + 54.32f));

    // Moon should have no impact if below the horizon
    // .05 is ad hoc fadeout range - Rob
    if( cosDistMoonZenith < 0.f )
        sb->bMoonTerm1 *= 1.f + cosDistMoonZenith/0.05f;
    if(cosDistMoonZenith < -0.05f)
        sb->bMoonTerm1 = 0.f;

    // Term for moon brightness computation
    sb->C3 = exp10(-0.4f * sb->K * sb->airMassMoon);

    sb->bTwilightTerm = -6.724f + 22.918312f *
        (M_PI_2 - acosf(cosDistSunZenith));

    // Term for sky brightness computation
    sb->C4 = exp10(-0.4f * sb->K * sb->airMassSun);
}

float skybrightness_get_luminance(const skybrightness_t *sb,
        float cosDistMoon, float cosDistSun, float cosDistZenith)
{
    // Air mass
    const float bKX = exp10(-0.4f * sb->K *
            (1.f / (cosDistZenith + 0.025f * exp(-11.f * cosDistZenith))));

    // Daylight brightness
    const float distSun = acos(cosDistSun);
    const float FSv = 18886.28f / (distSun * distSun + 0.0007f)
                    + exp10(6.15f - (distSun + 0.001f)* 1.43239f)
                    + 229086.77f * ( 1.06f + cosDistSun * cosDistSun);
    const float b_daylight = 9.289663e-12f * (1.f - bKX) *
                (FSv * sb->C4 + 440000.f * (1.f - sb->C4));

    //Twilight brightness
    const float b_twilight = exp10(sb->bTwilightTerm + 0.063661977f *
            acos(cosDistZenith) / (sb->K > 0.05f ? sb->K : 0.05f)) *
            (1.7453293f / distSun) * (1.f - bKX);

    // Total sky brightness
    float b_total = ((b_twilight < b_daylight) ? b_twilight : b_daylight);

    // Moonlight brightness, don't compute if less than 1% daylight
    if ((sb->bMoonTerm1 * (1.f - bKX) *
                (28860205.1341274269f * sb->C3 + 440000.f *
                 (1.f - sb->C3)))/b_total>0.01f) {
        float dist_moon;
        if (cosDistMoon >= 1.f) {cosDistMoon = 1.f; dist_moon = 0.f;}
        else {
            // Because the accuracy of our power serie is bad around 1, call
            // the real acos if it's the case
            dist_moon = cosDistMoon > 0.99f ? acosf(cosDistMoon) :
                                              acos(cosDistMoon);
        }
        // The last 0.0005 should be 0, but it causes too fast brightness
        // change
        const float FM = 18886.28f / (dist_moon * dist_moon + 0.0005f)
            + exp10(6.15f - dist_moon * 1.43239f)
            + 229086.77f * ( 1.06f + cosDistMoon * cosDistMoon);
        b_total += sb->bMoonTerm1 * (1.f - bKX) *
            (FM * sb->C3 + 440000.f * (1.f - sb->C3));
    }

    // Dark night sky brightness, don't compute if less than 1% daylight
    if ((sb->bNightTerm * bKX) / b_total > 0.01f) {
        b_total += (0.4f + 0.6f / sqrtf(0.04f + 0.96f *
                    cosDistZenith * cosDistZenith)) * sb->bNightTerm * bKX;
    }

    // In cd/m^2 : the 32393895 is empirical term because the
    // lambert -> cd/m^2 formula seems to be wrong.
    return (b_total < 0.f) ? 0.f :
        b_total * (900900.9f * M_PI * 1e-4f * 3239389.f * 2.f * 1.5f);
}
