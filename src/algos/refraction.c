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

/* Refraction computation
 *
 * Using A*tan(z)+B*tan^3(z) model, with Newton-Raphson correction.
 *
 * inputs:
 *   v: cartesian horizontal coordinates (Z up).
 *   refa: refraction A argument.
 *   refb: refraction B argument.
 *
 * outputs:
 *   out: corrected cartesian horizontal coordinates.
 */
void refraction(const double v[3], double refa, double refb,
                double out[3])
{
    const double CELMIN = 1e-6;
    const double SELMIN = 0.05;
    double xaet, yaet, zaet, r, z, tz, w, del, cosdel, f;

    if (refa == 0.0 && refb == 0.0) {
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
        return;
    }

    xaet = v[0];
    yaet = v[1];
    zaet = v[2];

    // Cosine and sine of altitude, with precautions.
    r = sqrt(xaet * xaet + yaet * yaet);
    r = r > CELMIN ? r : CELMIN;
    z = zaet > SELMIN ? zaet : SELMIN;

    // A*tan(z)+B*tan^3(z) model, with Newton-Raphson correction.
    tz = r / z;
    w = refb * tz * tz;
    del = (refa + w) * tz / (1.0 + (refa + 3.0 * w) / (z * z));

    // Apply the change, giving observed vector.
    cosdel = 1.0 - del * del / 2.0;
    f = cosdel - del * z / r;

    out[0] = xaet * f;
    out[1] = yaet * f;
    out[2] = cosdel * zaet + del * r;
}

