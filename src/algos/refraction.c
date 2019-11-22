/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdbool.h>
#include <math.h>
#include "utils/vec.h"

// Use more flexible refraction model coming from Stellarium instead of the
// one from ERFA lib. It has a much better behaviour for low altitudes ~< 5 deg
// at the expense of speed.
// If performances becomes a problem, it may be possible to use the fast
// ERFA model for higher altitudes, and revert to this one for lower ones.

#define DD2R (1.745329251994329576923691e-2)

/*
 * Function: refraction_prepare
 * Compute the constants A and B used in the refraction computation.
 *
 * Parameters:
 *   phpa   - Pressure at the observer (millibar).
 *   tc     - Temperature at the observer (deg C).
 *   rh     - Relative humidity at the observer (range 0-1).
 *   refa   - Output of the A coefficient.
 *   refb   - Output of the B coefficient.
 */
void refraction_prepare(double phpa, double tc, double rh,
                        double *refa, double *refb)
{
    // Directly pass the pressure and temperature to the refraction function.
    *refa = phpa;
    *refb = tc;
}

void refraction(const double v[3], double pressure, double temperature,
                double out[3])
{
    // The following 2 variables are set according to Georg Zotti comment in
    // original Stellarium code, so that nothing happens below -5 degrees.

    // This must be -5 or higher.
    static const float MIN_GEO_ALTITUDE_DEG = -3.54f;
    // This must be positive. Transition zone goes that far below the values
    // just specified.
    static const float TRANSITION_WIDTH_GEO_DEG = 1.46f;
    // Hopefully, the compiler pre-compute this
    const float min_sinalt = sinf(MIN_GEO_ALTITUDE_DEG * DD2R -
                                         TRANSITION_WIDTH_GEO_DEG * DD2R);

    assert(vec3_is_normalized(v));
    vec3_copy(v, out);
    if (v[2] < min_sinalt)
        return;

    double geom_alt_deg = asin(v[2]) / DD2R;

    const double p_saemundson = 1.02 * pressure / 1010. * 283. /
                         (273. + temperature) / 60.;
    if (geom_alt_deg > MIN_GEO_ALTITUDE_DEG)
    {
        // refraction from Saemundsson, S&T1986 p70 / in Meeus, Astr.Alg.
        double r = p_saemundson / tan((geom_alt_deg + 10.3 /
                     (geom_alt_deg + 5.11)) * DD2R) + 0.0019279;
        geom_alt_deg += r;
        if (geom_alt_deg > 90.)
            geom_alt_deg = 90.;
        out[2] = sin(geom_alt_deg * DD2R);
    }
    else if (geom_alt_deg > MIN_GEO_ALTITUDE_DEG - TRANSITION_WIDTH_GEO_DEG)
    {
        // Avoids the jump below -5 by interpolating linearly between
        // MIN_GEO_ALTITUDE_DEG and bottom of transition zone
        double r_m5 = p_saemundson / tan((MIN_GEO_ALTITUDE_DEG + 10.3 /
                     (MIN_GEO_ALTITUDE_DEG + 5.11)) * DD2R) + 0.0019279;
        geom_alt_deg += r_m5 * (geom_alt_deg - (MIN_GEO_ALTITUDE_DEG -
                        TRANSITION_WIDTH_GEO_DEG)) / TRANSITION_WIDTH_GEO_DEG;
        out[2] = sin(geom_alt_deg * DD2R);
    }
    vec3_normalize(out, out);
}

void refraction_inv(const double v[3], double pressure, double temperature,
                double out[3])
{
    double delta[3];
    double a[3], b[3];
    int i;

    assert(vec3_is_normalized(v));
    vec3_copy(v, a);
    for (i = 0; i < 10 ; ++i) {
        refraction(a, pressure, temperature, b);
        vec3_normalize(b, b);
        vec3_sub(b, v, delta);
        vec3_sub(a, delta, a);
        vec3_normalize(a, a);
        if (vec3_dot(b, v) > cos(0.001 / 3600 * M_PI / 180))
            break;
    }
    vec3_copy(a, out);
    assert(vec3_is_normalized(out));
}
