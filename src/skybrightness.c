/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>
#include "skybrightness.h"
#include "utils/utils.h"

#define exp10(x) exp((x) * log(10.0))
#define exp10f(x) expf((x) * logf(10.f))

static inline float fast_expf(float x) {
    x = 1.0f + x / 1024;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x;
    return x;
}

static inline float fast_exp10f(float x) {
    return fast_expf(x * logf(10.f));
}

// The taylor serie is not accurate around x=1 and x=-1
static inline float fast_acosf(float x)
{
    return (float)(M_PI_2) - (x + x * x * x *
        (1.f / 6.f + x * x *(3.f / 40.f + 5.f / 112.f * x * x)));
}

// Radiant to degree.
static const float D2R = M_PI / 180.0f;

// Nanolambert to cd/m²
static const float NLAMBERT_TO_CDM2 = 3.183e-6f;

void skybrightness_prepare(skybrightness_t *sb,
        int year, int month, float moon_mag,
        float latitude, float altitude,
        float temperature, float relative_humidity,
        float dist_moon_zenith, float dist_sun_zenith)
{
    // month : 1=Jan, 12=Dec
    float RA = (month - 3.f) * 0.52359878f;

    // Term for dark sky brightness computation
    sb->b_night_term = 1.0e-13 + 0.3e-13 * cosf(0.57118f * (year - 1992.f));

    float sign_latitude = (latitude>=0.f) * 2.f - 1.f;

    // extinction Coefficient for V band
    float KR = 0.1066f * expf(-altitude / 8200.f);
    float KA = 0.1f * expf(-altitude / 1500.f) *
               powf(1.f - 0.32f / logf(relative_humidity / 100.f), 1.33f) *
               (1.f + 0.33f * sign_latitude * sinf(RA));
    float KO = 0.031f * expf(-altitude / 8200.f) * (3.f + 0.4f *
               (latitude * cosf(RA) - cosf(3.f * latitude))) / 3.f;
    float KW = 0.031f * 0.94f * (relative_humidity / 100.f) *
               expf(temperature / 15.f) * expf(-altitude / 8200.f);
    sb->K = KR + KA + KO + KW;

    // Air mass for Moon
    const float cos_dist_moon_zenith = cosf(dist_moon_zenith);
    if (cos_dist_moon_zenith < 0)
        sb->airmass_moon = 40.f;
    else
        sb->airmass_moon = 1.f / (cos_dist_moon_zenith + 0.025f *
                                  expf(-11.f * cos_dist_moon_zenith));

    // Air mass for Sun
    const float cos_dist_sun_zenith = cosf(dist_sun_zenith);
    if (cos_dist_sun_zenith < 0)
        sb->airmass_sun = 40.f;
    else
        sb->airmass_sun = 1.f / (cos_dist_sun_zenith + 0.025f *
                                 expf(-11.f * cos_dist_sun_zenith));

    double mt = exp10(-0.4 * (moon_mag + 54.32));

    // Graduate the moon impact on atmosphere from 0 to 100% when its altitude
    // is ranging from 0 to 10 deg to avoid discontinuity.
    // This hack can probably be reduced when extinction is taken into account.
    if (dist_moon_zenith > 90.f * D2R)  {
        mt = 0.;
    } else if (dist_moon_zenith > 75.f * D2R)  {
        mt *= (90. * D2R - dist_moon_zenith) / (15. * D2R);
    }
    // Scale by 1e6 to avoid reaching float precision limit
    sb->b_moon_term = mt * 1000000;

    // Term for moon brightness computation
    sb->C3 = exp10f(-0.4f * sb->K * sb->airmass_moon);

    sb->b_twilight_term = -6.724f + 22.918312f * (M_PI_2 - dist_sun_zenith);

    // Term for sky brightness computation
    sb->C4 = exp10f(-0.4f * sb->K * sb->airmass_sun);
}

float skybrightness_get_luminance(
        const skybrightness_t *sb,
        float cos_moon_dist, float cos_sun_dist, float cos_zenith_dist)
{
    // This avoid issues in the algo
    cos_moon_dist = min(cos_moon_dist, cosf(1.f * D2R));
    cos_sun_dist  = min(cos_sun_dist, cosf(1.f * D2R));

    const float moon_dist = acosf(cos_moon_dist);
    const float sun_dist = acosf(cos_sun_dist);

    // Air mass
    const float bKX = fast_exp10f(-0.4f * sb->K *
        1.f / (cos_zenith_dist + 0.025f * fast_expf(-11.f * cos_zenith_dist)));

    // Daylight brightness
    const float FS = 18886.28f / (sun_dist * sun_dist) +
                   fast_exp10f(6.15f - (sun_dist + 0.001f) * 1.43239f) +
                   229086.77f * ( 1.06f + cos_sun_dist * cos_sun_dist);
    float b_daylight = 9.289663e-12f * (1.f - bKX) *
        (FS * sb->C4 + 440000.f * (1.f - sb->C4));

    // Twilight brightness
    float b_twilight_k = sb->b_twilight_term + 0.063661977f *
        fast_acosf(cos_zenith_dist) / (sb->K > 0.05f ? sb->K : 0.05f);
    float b_twilight = 0;
    if (b_twilight_k > -32) { // Prevent underflow.
        b_twilight = fast_exp10f(b_twilight_k) *
            (1.7453293f / sun_dist) * (1.f - bKX);
    }

    // Total sky brightness
    float b_total = ((b_twilight < b_daylight) ? b_twilight : b_daylight);

    // Moonlight brightness
    const float FM = 18886.28f / (moon_dist * moon_dist)
        + fast_exp10f(6.15f - moon_dist * 1.43239f)
        + 229086.77f * (1.06f + cos_moon_dist * cos_moon_dist);
    float b_moon = sb->b_moon_term * (1.f - bKX) *
            (FM * sb->C3 + 440000.f * (1.f - sb->C3)) / 1000000.f;

    b_total += b_moon;

    // Dark night sky brightness, don't compute if less than 1% daylight
    if (b_total && (sb->b_night_term * bKX) / b_total > 0.01f) {
        b_total += (0.4f + 0.6f / sqrtf(0.04f + 0.96f *
                   cos_zenith_dist * cos_zenith_dist)) * sb->b_night_term * bKX;

        // Ad-hoc addition to make the sky slightly more blueish
        b_total += 0.0000000000012f;
    }

    if (b_total < 0.f)
        return 0.f;

    // Convert to nano lambert then cd/m2
    return b_total / 1.11E-15f * NLAMBERT_TO_CDM2;
}
