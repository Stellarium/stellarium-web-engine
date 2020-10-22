/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>
#include "algos.h"
#include "erfa.h"

double tt2utc(double tt, double *dut1)
{
    double dt, utc1, utc2, ut11, ut12, tai1, tai2, utc, ut1, dut1_;
    int r;

    if (!dut1) dut1 = &dut1_; // Support dut1 = NULL.

    dt = deltat(tt);
    eraTtut1(ERFA_DJM0, tt, dt, &ut11, &ut12);
    eraTttai(ERFA_DJM0, tt, &tai1, &tai2);
    r = eraTaiutc(tai1, tai2, &utc1, &utc2);

    // If we don't know the leap seconds, assume UTC = UT1.
    if (r != 0) {
        *dut1 = 0;
        return ut11 - ERFA_DJM0 + ut12;
    }

    utc = utc1 - ERFA_DJM0 + utc2;
    ut1 = ut11 - ERFA_DJM0 + ut12;
    *dut1 = (ut1 - utc) * ERFA_DAYSEC;

    if (fabs(*dut1) > 1) {
        LOG_W_ONCE("DUT1 = %fs", *dut1);
    }

    return utc;
}

double utc2tt(double utc)
{
    double tai1, tai2, tt1, tt2, dt, tt;
    int r;

    r = eraUtctai(ERFA_DJM0, utc, &tai1, &tai2);

    // If we don't know the leap seconds, assume UTC = UT1 and use ΔT to
    // compute TT.
    if (r != 0) {
        dt = deltat(utc);
        eraUt1tt(ERFA_DJM0, utc, dt, &tt1, &tt2);
        tt = tt1 - ERFA_DJM0 + tt2;
        // Adjust dt with the value at the final TT time.
        tt += (deltat(tt) - dt) / ERFA_DAYSEC;
        return tt;
    }

    eraTaitt(tai1, tai2, &tt1, &tt2);
    return tt1 - ERFA_DJM0 + tt2;
}
