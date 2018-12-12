/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "utils/utils.h"
#include "erfa.h"
#include <stdio.h>
#include <string.h>


// XXX: incomplete.
const char *format_time(char *buf, double t, double utcofs,
                        const char *format)
{
    int iy, im, id, ihmsf[4], ofs;
    if (isnan(t)) {
        strcpy(buf, "XXXXX");
        return buf;
    }
    t += utcofs;
    ofs = round(utcofs * 24);
    eraD2dtf("UTC", 0, ERFA_DJM0, t, &iy, &im, &id, ihmsf);
    if (format && strcmp(format, "YYYY-MM-DD") == 0) {
        sprintf(buf, "%.4d-%.2d-%.2d", iy, im, id);
    } else if (format && strcmp(format, "HH:mm") == 0) {
        sprintf(buf, "%.2d:%.2d", ihmsf[0], ihmsf[1]);
    } else {
        sprintf(buf, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d (UTC%d)",
                iy, im, id, ihmsf[0], ihmsf[1], ihmsf[2], ofs);
    }
    return buf;
}

const char *format_angle(char *buf, double a, char type, int ndp,
                         const char *fmt)
{
    int hmsf[4], dmsf[4];
    char s[2] = {};
    if (isnan(a)) return "NAN";
    if (!fmt) ndp = 1;
    if (type == 'h') {
        fmt = fmt ?: "%s%02dh%02dm%02d.%01ds";
        eraA2tf(ndp, a, &s[0], hmsf);
        sprintf(buf, fmt, s, hmsf[0], hmsf[1], hmsf[2], hmsf[3]);
    } else {
        fmt = fmt ?: "%s%02d°%02d'%02d.%01d\"";
        eraA2af(ndp, a, &s[0], dmsf);
        sprintf(buf, fmt, s, dmsf[0], dmsf[1], dmsf[2], dmsf[3]);
    }
    return buf;
}

const char *format_hangle(char *buf, double a)
{
    int hmsf[4];
    char s;
    if (isnan(a)) return "NAN";
    eraA2tf(0, a, &s, hmsf);
    sprintf(buf, "%c%02dh%02dm%02ds", s, hmsf[0], hmsf[1], hmsf[2]);
    return buf;
}

const char *format_dangle(char *buf, double a)
{
    int dmsf[4];
    char s;
    if (isnan(a)) return "NAN";
    eraA2af(0, a, &s, dmsf);
    sprintf(buf, "%c%02d°%02d'%02d\"", s, dmsf[0], dmsf[1], dmsf[2]);
    return buf;
}

// Convert a distance (int AU) to a string.
const char *format_dist(char *buf, double d)
{
    double light_year;
    double meter;

    if (isnan(d)) return "NAN";

    light_year = d * ERFA_AULT / ERFA_DAYSEC / ERFA_DJY;
    meter = d * ERFA_DAU;

    if (light_year >= 0.1) {
        sprintf(buf, "%.1f light years", light_year);
        return buf;
    }
    if (d >= 0.1) {
        sprintf(buf, "%.1f AU", d);
        return buf;
    }
    if (meter >= 1000) {
        sprintf(buf, "%.1f km", meter / 1000);
        return buf;
    }
    sprintf(buf, "%.0f m", meter);
    return buf;
}
