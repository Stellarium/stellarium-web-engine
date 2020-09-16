/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Pluto position computation, based on Meeus, Astronomical Algorithms
 * 2nd ed (1998) Chap 37.
 */

#include <math.h>

typedef struct {
    int J, S, P;
    int lon_A, lon_B;
    int lat_A, lat_B;
    int rad_A, rad_B;
} coef_t;

#define NB_COEFFS 43
#define D2R (1.745329251994329576923691e-2)

static const coef_t T[NB_COEFFS] =
{
    {0, 0, 1, -19799805,  19850055, -5452852, -14974862,  66865439, 68951812},
    {0, 0, 2,    897144,  -4954829,  3527812,   1672790, -11827535,  -332538},
    {0, 0, 3,    611149,   1211027, -1050748,    327647,   1593179, -1438890},
    {0, 0, 4,   -341243,   -189585,   178690,   -292153,    -18444,   483220},
    {0, 0, 5,    129287,    -34992,    18650,    100340,    -65977,   -85431},
    {0, 0, 6,    -38164,     30893,   -30697,    -25823,     31174,    -6032},
    {0, 1, -1,    20442,     -9987,     4878,     11248,     -5794,    22161},
    {0, 1, 0,     -4063,     -5071,      226,       -64,      4601,     4032},
    {0, 1, 1,     -6016,     -3336,     2030,      -836,     -1729,      234},
    {0, 1, 2,     -3956,      3039,       69,      -604,      -415,      702},
    {0, 1, 3,      -667,      3572,     -247,      -567,       239,      723},
    {0, 2, -2,     1276,       501,      -57,         1,        67,      -67},
    {0, 2, -1,     1152,      -917,     -122,       175,      1034,     -451},
    {0, 2, 0,       630,     -1277,      -49,      -164,      -129,      504},
    {1, -1, 0,     2571,      -459,     -197,       199,       480,     -231},
    {1, -1, 1,      899,     -1449,      -25,       217,         2,     -441},
    {1, 0, -3,    -1016,      1043,      589,      -248,     -3359,      265},
    {1, 0, -2,    -2343,     -1012,     -269,       711,      7856,    -7832},
    {1, 0, -1,     7042,       788,      185,       193,        36,    45763},
    {1, 0, 0,      1199,      -338,      315,       807,      8663,     8547},
    {1, 0, 1,       418,       -67,     -130,       -43,      -809,     -769},
    {1, 0, 2,       120,      -274,        5,         3,       263,     -144},
    {1, 0, 3,       -60,      -159,        2,        17,      -126,       32},
    {1, 0, 4,       -82,       -29,        2,         5,       -35,      -16},
    {1, 1, -3,      -36,       -29,        2,         3,       -19,       -4},
    {1, 1, -2,      -40,         7,        3,         1,       -15,        8},
    {1, 1, -1,      -14,        22,        2,        -1,        -4,       12},
    {1, 1, 0,         4,        13,        1,        -1,         5,        6},
    {1, 1, 1,         5,         2,        0,        -1,         3,        1},
    {1, 1, 3,        -1,         0,        0,         0,         6,       -2},
    {2, 0, -6,        2,         0,        0,        -2,         2,        2},
    {2, 0, -5,       -4,         5,        2,         2,        -2,       -2},
    {2, 0, -4,        4,        -7,       -7,         0,        14,       13},
    {2, 0, -3,       14,        24,       10,        -8,       -63,       13},
    {2, 0, -2,      -49,       -34,       -3,        20,       136,     -236},
    {2, 0, -1,      163,       -48,        6,         5,       273,     1065},
    {2, 0, 0,         9,       -24,       14,        17,       251,      149},
    {2, 0, 1,        -4,         1,       -2,         0,       -25,       -9},
    {2, 0, 2,        -3,         1,        0,         0,         9,       -2},
    {2, 0, 3,         1,         3,        0,         0,        -8,        7},
    {3, 0, -2,       -3,        -1,        0,         1,         2,      -10},
    {3, 0, -1,        5,        -3,        0,         0,        19,       35},
    {3, 0, 0,         0,         0,        1,         0,        10,        3},
};

int pluto_pos(double tt_mjd, double pos[3])
{
    /* Sin and cos of J2000.0 mean obliquity (IAU 1976) */
    const double SINEPS = 0.3977771559319137;
    const double COSEPS = 0.9174820620691818;

    double sum_lon = 0, sum_lat = 0, sum_rad = 0;
    double a, sin_a, cos_a;
    int i;
    double L, B, R, x, y, z;

    /* get julian centuries since J2000 */
    const double t = (tt_mjd - 51544.5) / 36525;

    /* calculate mean longitudes for jupiter, saturn and pluto */
    const double J =  34.35 + 3034.9057 * t;
    const double S =  50.08 + 1222.1138 * t;
    const double P = 238.96 +  144.9600 * t;

    /* calc periodic terms */
    for (i = 0; i < NB_COEFFS; i++) {
        a = T[i].J * J + T[i].S * S + T[i].P * P;
        sin_a = sin(a * D2R);
        cos_a = cos(a * D2R);

        sum_lon += T[i].lon_A * sin_a + T[i].lon_B * cos_a;
        sum_lat += T[i].lat_A * sin_a + T[i].lat_B * cos_a;
        sum_rad += T[i].rad_A * sin_a + T[i].rad_B * cos_a;
    }

    L = (238.958116 + 144.96 * t + sum_lon * 0.000001) * D2R;
    B = (-3.908239 + sum_lat * 0.000001) * D2R;
    R = (40.7241346 + sum_rad * 0.0000001);

    /* convert to rectangular coord */
    x = R * cos(L) * cos(B);
    y = R * sin(L) * cos(B);
    z = R * sin(B);

    /* rotate to equatorial J2000 */
    pos[0] = x;
    pos[1] = y * COSEPS - z * SINEPS;
    pos[2] = y * SINEPS + z * COSEPS;

    return 0;
}
