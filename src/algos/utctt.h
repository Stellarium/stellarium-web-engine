/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef UTCTT_H
#define UTCTT_H

/*
 * Function: tt2utc
 * Convert time from TT to UTC
 *
 * Parameters:
 *   tt     - TT time (MJD)
 *   dut1   - If not NULL, output of DUT1 (sec)
 *
 * Returns:
 *   UTC time (MJD)
 */
double tt2utc(double tt, double *dut1);

/*
 * Function: utc2tt
 * Convert time from UTC to TT
 *
 * Parameters:
 *   utc    - UTC time (MJD)
 *
 * Returns:
 *   TT time (MJD)
 */
double utc2tt(double utc);

#endif // UTCTT_H
