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

#define DD2Rf ((float)M_PI / 180.f)
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

    const float p_saemundson = 1.02f * pressure / 1010.f * 283.f /
                         (273.f + temperature) / 60.f;

    assert(vec3_is_normalized(v));
    out[0] = v[0];
    out[1] = v[1];

    float geom_alt_deg = asinf(v[2]) / DD2Rf;

    if (geom_alt_deg > MIN_GEO_ALTITUDE_DEG)
    {
        // refraction from Saemundsson, S&T1986 p70 / in Meeus, Astr.Alg.
        float r = p_saemundson / tanf((geom_alt_deg + 10.3f /
                     (geom_alt_deg + 5.11f)) * DD2Rf) + 0.0019279f;
        geom_alt_deg += r;
        if (geom_alt_deg > 90.)
            geom_alt_deg = 90.;
        out[2] = sinf(geom_alt_deg * DD2Rf);
    }
    else if (geom_alt_deg > MIN_GEO_ALTITUDE_DEG - TRANSITION_WIDTH_GEO_DEG)
    {
        // Avoids the jump below -5 by interpolating linearly between
        // MIN_GEO_ALTITUDE_DEG and bottom of transition zone
        float r_m5 = p_saemundson / tanf((MIN_GEO_ALTITUDE_DEG + 10.3f /
                     (MIN_GEO_ALTITUDE_DEG + 5.11f)) * DD2Rf) + 0.0019279f;
        geom_alt_deg += r_m5 * (geom_alt_deg - (MIN_GEO_ALTITUDE_DEG -
                        TRANSITION_WIDTH_GEO_DEG)) / TRANSITION_WIDTH_GEO_DEG;
        out[2] = sinf(geom_alt_deg * DD2Rf);
    }
    vec3_normalize(out, out);
}

void refraction_inv(const double v[3], double pressure, double temperature,
                double out[3])
{
    double delta[3];
    double a[3], b[3];
    int i;
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
}
