/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <time.h>
#include "erfa.h"

#define DJM0 ERFA_DJM0

double time_set_dtf(double utc, double utcoffset,
                    int year, int month, int day,
                    int h, int m, int s)
{
    double t, d1, d2;
    int r, iy, im, id, hmsf[4];
    t = utc + utcoffset;
    r = eraD2dtf("UTC", 0, DJM0, t, &iy, &im, &id, hmsf);
    if (r != 0) return NAN;
    if (year > 0) iy = year; // XXX: might be a problem!
    if (month > 0) im = month;
    if (day > 0) id = day;
    if (h >= 0) hmsf[0] = h;
    if (m >= 0) hmsf[1] = m;
    if (s >= 0) hmsf[2] = s;
    r = eraDtf2d("UTC", iy, im, id, h, m, s, &d1, &d2);
    if (r != 0) return NAN;
    t = d1 - DJM0 + d2;
    t -= utcoffset;
    return t;
}

double time_add_dtf(double utc, double utcoffset, int year,
                    int month, int day,
                    int h, int m, int s)
{
    double t, d1, d2;
    int r, iy, im, id, hmsf[4];
    t = utc + utcoffset;
    r = eraD2dtf("UTC", 0, DJM0, t, &iy, &im, &id, hmsf);
    if (r != 0) return NAN;
    iy += year;
    im += month;
    id += day;
    hmsf[0] += h;
    hmsf[1] += m;
    hmsf[2] += s;
    r = eraDtf2d("UTC", iy, im, id, h, m, s, &d1, &d2);
    if (r != 0) return NAN;
    t = d1 - DJM0 + d2;
    t -= utcoffset;
    return t;
}
