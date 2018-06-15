/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * This is a simplified version of the ELP-2000/82 lunar theorie based on
 * Astronomical Algorithms by Jean Meeus.
 *
 * http://www.stargazing.net/kepler/moon3.html
 */

#include <math.h>
#include <stdlib.h>

#define NB_TERMS 60
/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

static const int PERIODIC_TERMS_LONG_RADIUS[NB_TERMS][6];
static const int PERIODIC_TERMS_LATITUDE[NB_TERMS][5];

static inline double sina(double x) {return sin(x * DD2R);}
static inline double cosa(double x) {return cos(x * DD2R);}

// JDE: julian ephemeris day
int moon_pos(double jde, double *lambda, double *beta, double *dist)
{
    double t;                   // Time in Julian Centuries.
    double t2, t3, t4;
    // All the angles are in degree!
    double lp;                  // Mean longitude.
    double d;                   // Mean elongation.
    double m;                   // Sun mean anomaly.
    double mp;                  // Moon mean anomaly.
    double f;                   // Moon arg of latitude.
    double a1, a2, a3;
    double sl, sr, sb;
    double e, e2;
    int i;
    const int *pt;
    double term_l, term_r, term_b;

    t = (jde - 2451545) / 36525;
    t2 = t * t;
    t3 = t2 * t;
    t4 = t3 * t;

    lp = 218.3164591 + 481267.88134236 * t - 0.0013268 * t2 +
         t3 / 538841 - t4 / 65194000;
    lp = fmod(lp, 360);

    d = 297.8502042 + 445267.1115168 * t - 0.0016300 * t2 + t3 / 545868 -
        t4 / 113065000;
    d = fmod(d, 360);

    m = 357.5291092 + 35999.0502909 * t - 0.0001536 * t2 + t3 / 24490000;
    m = fmod(m, 360);

    mp = 134.9634114 + 477198.8676313 * t + 0.0089970 * t2 + t3 / 69699
         - t4 / 14712000;
    mp = fmod(mp, 360);

    f = 93.2720993 + 483202.0175273 * t - 0.0034029 * t2 - t3 / 3526000
        + t4 / 863310000;
    f = fmod(f, 360);

    a1 = 119.75 + 131.849 * t;
    a2 = 53.09 + 479264.290 * t;
    a3 = 313.45 + 481266.484 * t;

    e = 1 - 0.002516 * t - 0.0000074 * t2;
    e2 = e * e;

    sl = sr = sb = 0;
    for (i = 0; i < NB_TERMS; i++) {
        pt = (const int*)&PERIODIC_TERMS_LONG_RADIUS[i];
        term_l = pt[4] * sina(pt[0] * d + pt[1] * m + pt[2] * mp + pt[3] * f);
        term_r = pt[5] * cosa(pt[0] * d + pt[1] * m + pt[2] * mp + pt[3] * f);
        if (abs(pt[1]) == 1) {term_l *= e; term_r *= e;}
        if (abs(pt[1]) == 2) {term_l *= e2; term_r *= e2;}
        sl += term_l;
        sr += term_r;
        pt = (const int*)&PERIODIC_TERMS_LATITUDE[i];
        term_b = pt[4] * sina(pt[0] * d + pt[1] * m + pt[2] * mp + pt[3] * f);
        if (abs(pt[1]) == 1) term_b *= e;
        if (abs(pt[2]) == 2) term_b *= e2;
        sb += term_b;
    }
    sl += 3958 * sina(a1) + 1962 * sina(lp - f) + 318 * sina(a2);
    sb += -2235 * sina(lp) + 382 * sina(a3) + 175 * sina(a1 - f)
          + 175 * sina(a1 + f) + 127 * sina(lp - mp) - 115 * sina(lp + mp);

    // Geocentric longitude and latitude (mean equinox and ecliptic).
    *lambda = (lp + sl / 1000000) * DD2R;
    *beta = (sb / 1000000) * DD2R;
    *dist = 385000.56 + sr / 1000;

    // XXX: Add nutation correction?
    return 0;
}


static const int PERIODIC_TERMS_LONG_RADIUS[NB_TERMS][6] =
{
    {0,  0,  1,  0, 6288774, -20905355},
    {2,  0, -1,  0, 1274027,  -3699111},
    {2,  0,  0,  0,  658314,  -2955968},
    {0,  0,  2,  0,  213618,   -569925},
    {0,  1,  0,  0, -185116,     48888},
    {0,  0,  0,  2, -114332,     -3149},
    {2,  0, -2,  0,   58793,    246158},
    {2, -1, -1,  0,   57066,   -152138},
    {2,  0,  1,  0,   53322,   -170733},
    {2, -1,  0,  0,   45758,   -204586},
    {0,  1, -1,  0,  -40923,   -129620},
    {1,  0,  0,  0,  -34720,    108743},
    {0,  1,  1,  0,  -30383,    104755},
    {2,  0,  0, -2,   15327,     10321},
    {0,  0,  1,  2,  -12528,         0},
    {0,  0,  1, -2,   10980,     79661},
    {4,  0, -1,  0,   10675,    -34782},
    {0,  0,  3,  0,   10034,    -23210},
    {4,  0, -2,  0,    8548,    -21636},
    {2,  1, -1,  0,   -7888,     24208},
    {2,  1,  0,  0,   -6766,     30824},
    {1,  0, -1,  0,   -5163,     -8379},
    {1,  1,  0,  0,    4987,    -16675},
    {2, -1,  1,  0,    4036,    -12831},
    {2,  0,  2,  0,    3994,    -10445},
    {4,  0,  0,  0,    3861,    -11650},
    {2,  0, -3,  0,    3665,     14403},
    {0,  1, -2,  0,   -2689,     -7003},
    {2,  0, -1,  2,   -2602,         0},
    {2, -1, -2,  0,    2390,     10056},
    {1,  0,  1,  0,   -2348,      6322},
    {2, -2,  0,  0,    2236,     -9884},
    {0,  1,  2,  0,   -2120,      5751},
    {0,  2,  0,  0,   -2069,         0},
    {2, -2, -1,  0,    2048,     -4950},
    {2,  0,  1, -2,   -1773,      4130},
    {2,  0,  0,  2,   -1595,         0},
    {4, -1, -1,  0,    1215,     -3958},
    {0,  0,  2,  2,   -1110,         0},
    {3,  0, -1,  0,    -892,      3258},
    {2,  1,  1,  0,    -810,      2616},
    {4, -1, -2,  0,     759,     -1897},
    {0,  2, -1,  0,    -713,     -2117},
    {2,  2, -1,  0,    -700,      2354},
    {2,  1, -2,  0,     691,         0},
    {2, -1,  0, -2,     596,         0},
    {4,  0,  1,  0,     549,     -1423},
    {0,  0,  4,  0,     537,     -1117},
    {4, -1,  0,  0,     520,     -1571},
    {1,  0, -2,  0,    -487,     -1739},
    {2,  1,  0, -2,    -399,         0},
    {0,  0,  2, -2,    -381,     -4421},
    {1,  1,  1,  0,     351,         0},
    {3,  0, -2,  0,    -340,         0},
    {4,  0, -3,  0,     330,         0},
    {2, -1,  2,  0,     327,         0},
    {0,  2,  1,  0,    -323,      1165},
    {1,  1, -1,  0,     299,         0},
    {2,  0,  3,  0,     294,         0},
    {2,  0, -1, -2,       0,      8752},
};


static const int PERIODIC_TERMS_LATITUDE[NB_TERMS][5] = {
    {0,  0,  0,  1, 5128122},
    {0,  0,  1,  1,  280602},
    {0,  0,  1, -1,  277693},
    {2,  0,  0, -1,  173237},
    {2,  0, -1,  1,   55413},
    {2,  0, -1, -1,   46271},
    {2,  0,  0,  1,   32573},
    {0,  0,  2,  1,   17198},
    {2,  0,  1, -1,    9266},
    {0,  0,  2, -1,    8822},
    {2, -1,  0, -1,    8216},
    {2,  0, -2, -1,    4324},
    {2,  0,  1,  1,    4200},
    {2,  1,  0, -1,   -3359},
    {2, -1, -1,  1,    2463},
    {2, -1,  0,  1,    2211},
    {2, -1, -1, -1,    2065},
    {0,  1, -1, -1,   -1870},
    {4,  0, -1, -1,    1828},
    {0,  1,  0,  1,   -1794},
    {0,  0,  0,  3,   -1749},
    {0,  1, -1,  1,   -1565},
    {1,  0,  0,  1,   -1491},
    {0,  1,  1,  1,   -1475},
    {0,  1,  1, -1,   -1410},
    {0,  1,  0, -1,   -1344},
    {1,  0,  0, -1,   -1335},
    {0,  0,  3,  1,    1107},
    {4,  0,  0, -1,    1021},
    {4,  0, -1,  1,     833},
    {0,  0,  1, -3,     777},
    {4,  0, -2,  1,     671},
    {2,  0,  0, -3,     607},
    {2,  0,  2, -1,     596},
    {2, -1,  1, -1,     491},
    {2,  0, -2,  1,    -451},
    {0,  0,  3, -1,     439},
    {2,  0,  2,  1,     422},
    {2,  0, -3, -1,     421},
    {2,  1, -1,  1,    -366},
    {2,  1,  0,  1,    -351},
    {4,  0,  0,  1,     331},
    {2, -1,  1,  1,     315},
    {2, -2,  0, -1,     302},
    {0,  0,  1,  3,    -283},
    {2,  1,  1, -1,    -229},
    {1,  1,  0, -1,     223},
    {1,  1,  0,  1,     223},
    {0,  1, -2, -1,    -220},
    {2,  1, -1, -1,    -220},
    {1,  0,  1,  1,    -185},
    {2, -1, -2, -1,     181},
    {0,  1,  2,  1,    -177},
    {4,  0, -2, -1,     176},
    {4, -1, -1, -1,     166},
    {1,  0,  1, -1,    -164},
    {4,  0,  1, -1,     132},
    {1,  0, -1, -1,    -119},
    {4, -1,  0, -1,     115},
    {2, -2,  0,  1,     107},
};
