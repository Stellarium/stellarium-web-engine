/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

#include <math.h>

/*  RINGS OF SATURN by Olson, et al, BASIC Code from Sky & Telescope, May 1995.
 *  As converted from BASIC to C by pmartz@dsd.es.com (Paul Martz)
 *  Adapted to xephem by Elwood Charles Downey.
 */
void satrings(
    double sb, double sl, double sr, // Saturn hlat, hlong, sun dist
    double el, double er,            // Earth hlong, sun dist
    double JD,                       // Julian date
    double *etiltp, double *stiltp)  // tilt from earth and sun, rads south
{
    double t, i, om;
    double x, y, z;
    double la, be;
    double s, b, sp, bp;

    t = (JD - 2451545.) / 365250.;
    i = (28.04922 - 0.13 * t + 0.0004 * t * t) * DD2R;
    om = (169.53 + 13.826 * t + 0.04 * t * t) * DD2R;

    x = sr * cos(sb) * cos(sl) - er * cos(el);
    y = sr * cos(sb) * sin(sl) - er * sin(el);
    z = sr * sin(sb);

    la = atan(y / x);
    if (x < 0) la += M_PI;
    be = atan(z / sqrt(x * x + y * y));

    s = sin(i) * cos(be) * sin(la - om) - cos(i) * sin(be);
    b = atan(s / sqrt(1. - s * s));
    sp = sin(i) * cos(sb) * sin(sl - om) - cos(i) * sin(sb);
    bp = atan(sp / sqrt(1.0 - sp * sp));

    *etiltp = b;
    *stiltp = bp;
}

