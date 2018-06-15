/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


#include <stdio.h>
#include "erfa.h"

// ut in MJD.
double deltat(double ut)
{
    double y, t;
    y = eraEpj(ERFA_DJM0, ut);
    if (y > 2005 && y < 2050) {
        t = y - 2000;
        return 62.92 + 0.32217 * t + 0.005589 * t * t;
    }
    return 0;
}
