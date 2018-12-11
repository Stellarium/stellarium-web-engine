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

#define exp10(x) exp((x) * log(10.f))
#define exp10f(x) expf((x) * logf(10.f))

static inline float pow2(float x) { return x * x; }
static inline float pow4(float x) { return x * x * x * x; }

static inline float fast_expf(float x) {
    x = 1.0 + x / 1024;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x;
    return x;
}

static inline float fast_exp10f(float x) {
    return fast_expf(x * logf(10.f));
}

// Radiant to degree.
static const float DR = 180.0 / 3.14159;

// Nanolambert to cd/m²
static const float NLAMBERT_TO_CDM2 = 3.183e-6;


static const float WA = 0.55;
static const float MO = -11.05;
static const float OZ = 0.031;
static const float WT = 0.031;
static const float BO = 1.0E-13;
static const float CM = 0.00;
static const float MS = -26.74;

void skybrightness_prepare(
        skybrightness_t *sb,
        int year, int month, float moon_phase,
        float latitude, float altitude,
        float temperature, float relative_humidity,
        float dist_moon_zenith, float dist_sun_zenith,
        float max_moon_brightness)
{
    sb->Y = year;
    sb->M = month;
    sb->AM = moon_phase * DR;
    sb->LA = latitude * DR;
    sb->AL = altitude;
    sb->TE = temperature;
    sb->RH = relative_humidity;
    sb->ZM = dist_moon_zenith * DR;
    sb->ZS = dist_sun_zenith * DR;
    sb->max_BM = max_moon_brightness * (1.11e-15 / NLAMBERT_TO_CDM2);

    // Precompute as much as possible.
    float K, KR, KA, KO, KW, LT, RA, SL, XM, XS;
    const float M = sb->M; // Month (1=Jan, 12=Dec)
    const float RD = 3.14159 / 180.0;
    const float LA = sb->LA; // Latitude (deg.)
    const float AL = sb->AL; // Altitude above sea level (m)
    const float RH = sb->RH; // relative humidity (%)
    const float TE = sb->TE; // Air temperature (deg. C)
    const float ZM = sb->ZM; // Zenith distance of Moon (deg.)
    const float ZS = sb->ZS; // Zenith distance of Sun (deg.)

    LT = LA * RD;
    RA = (M - 3) * 30.0 * RD;
    SL = LA / fabs(LA);
    // 1080 Airmass for each component
    // 1130 UBVRI extinction for each component
    KR = .1066 * expf(-1 * AL / 8200) * powf((WA / .55), -4);
    KA = .1 * powf((WA / .55), -1.3) * expf(-1 * AL / 1500);
    KA = KA * powf((1 - .32 / logf(RH / 100.0)), 1.33) *
             (1 + 0.33 * SL * sinf(RA));
    KO = OZ * (3.0 + .4 * (LT * cosf(RA) - cosf(3 * LT))) / 3.0;
    KW = WT * .94 * (RH / 100.0) * expf(TE / 15) * expf(-1 * AL / 8200);
    K = KR + KA + KO + KW;
    sb->K = K;

    // air mass Moon
    XM = 1 / (cosf(ZM * RD) + .025 * expf(-11 * cosf(ZM * RD)));
    if (ZM > 90.0) XM = 40.0;
    sb->XM = XM;

    // air mass Sun
    XS = 1 / (cosf(ZS * RD) + .025 * expf(-11 * cosf(ZS * RD)));
    if (ZS > 90.0) XS = 40.0;
    sb->XS = XS;
}


float skybrightness_get_luminance(
        const skybrightness_t *sb,
        float moon_dist, float sun_dist, float zenith_dist)
{
    float BL, B, ZZ,
           K, X, XS, XM, BN, MM, C3, FM, BM, HS, BT, C4, FS,
           BD;

    const float RD = 3.14159 / 180.0;
    // 80 Input for Moon and Sun
    const float AM = sb->AM; // Moon phase (deg.; 0=FM, 90=FQ/LQ, 180=NM)
    const float ZM = sb->ZM; // Zenith distance of Moon (deg.)
    const float RM = moon_dist * DR; // Angular distance to Moon (deg.)
    const float ZS = sb->ZS; // Zenith distance of Sun (deg.)
    const float RS = sun_dist * DR; // Angular distance to Sun (deg.)
    // 140 Input for the Site, Date, Observer
    const float Y = sb->Y; // Year
    const float Z = zenith_dist * DR; // Zenith distance (deg.)

    // 1000 Extinction Subroutine
    // 1080 Airmass for each component
    ZZ = Z * RD;
    K = sb->K;

    // 2000 SKY Subroutine
    X = 1 / (cosf(ZZ) + .025 * fast_expf(-11 * cosf(ZZ))); // air mass
    XM = sb->XM;
    XS = sb->XS;

    // 2130 Dark night sky brightness
    BN = BO * (1 + .3 * cosf(6.283 * (Y - 1992) / 11));
    BN = BN * (.4 + .6 / sqrtf(1.0 - .96 * powf((sinf(ZZ)), 2)));
    BN = BN * (fast_exp10f(-.4 * K * X));

    // 2170 Moonlight brightness
    MM = -12.73 + .026 * fabs(AM) + 4E-09 * pow4(AM); // moon mag in V
    MM = MM + CM; // Moon mag
    C3 = fast_exp10f(-.4 * K * XM);
    FM = 6.2E+07 / pow2(RM) + (exp10f(6.15 - RM / 40));
    FM = FM + exp10f(5.36) * (1.06 + pow2(cosf(RM * RD)));
    BM = exp10f(-.4 * (MM - MO + 43.27));
    BM = BM * (1 - exp10f(-.4 * K * X));
    BM = BM * (FM * C3 + 440000.0 * (1 - C3));

    // Added from the original code, a clamping value to prevent the
    // moon brightness to get too hight.
    if (sb->max_BM >= 0 && BM > sb->max_BM) BM = sb->max_BM;

    // 2260 Twilight brightness
    HS = 90.0 - ZS; // Height of Sun
    BT = exp10f(-.4 * (MS - MO + 32.5 - HS - (Z / (360 * K))));
    BT = BT * (100 / RS) * (1.0 - exp10f(-.4 * K * X));

    // 2300 Daylight brightness
    C4 = fast_exp10f(-.4 * K * XS);
    FS = 6.2E+07 / pow2(RS) + (fast_exp10f(6.15 - RS / 40));
    FS = FS + fast_exp10f(5.36) * (1.06 + pow2(cosf(RS * RD)));
    BD = exp10f(-.4 * (MS - MO + 43.27));
    BD = BD * (1 - exp10f(-.4 * K * X));
    BD = BD * (FS * C4 + 440000.0 * (1 - C4));

    // 2370 Total sky brightness
    if (BD > BT)
        B = BN + BT;
    else
        B = BN + BD;
    if (ZM < 90.0) B = B + BM;
    // End sky subroutine.


    // 250 Visual limiting magnitude
    BL = B / 1.11E-15; // in nanolamberts
    // 330 PRINT : REM  Write results and stop program
    return BL * NLAMBERT_TO_CDM2;
}
