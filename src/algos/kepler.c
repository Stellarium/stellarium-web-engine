/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>

/*
 * Compute kepler element.
 *
 * Parameters:
 *   mjd    - Time of the position (MJD).
 *   pos    - Get the result of the computation.
 *   k_jd   - date (MJD).
 *   k_in   - inclination (rad).
 *   k_om   - Longitude of the Ascending Node (rad).
 *   k_w    - Argument of Perihelion (rad).
 *   k_a    - Mean distance (Semi major axis).
 *   k_n    - Daily motion (rad/day).
 *   k_ec   - Eccentricity.
 *   k_ma   - Mean Anomaly (rad).
 *   k_omd  - variation of om in time (rad/day).
 *   k_wd   - variation of w in time (rad/day).
 */
int kepler_solve(double mjd, double pos[3],
                 double k_jd,      // date (MJD).
                 double k_in,      // inclination (rad).
                 double k_om,      // Longitude of the Ascending Node (rad).
                 double k_w,       // Argument of Perihelion (rad).
                 double k_a,       // Mean distance (Semi major axis).
                 double k_n,       // Daily motion (rad/day).
                 double k_ec,      // Eccentricity.
                 double k_ma,      // Mean Anomaly (rad).
                 double k_omd,     // variation of om in time (rad/day).
                 double k_wd       // variation of w in time (rad/day).
        )

{
    double d, m, v, o, w, r;
    // Get the number of day since element date.
    d = mjd - k_jd;
    // Compute the mean anomaly.
    m = k_n * d + k_ma;
    m = fmod(m, 2 * M_PI);
    // Compute true anomaly.
    v = m + ((2.0 * k_ec - pow(k_ec, 3) / 4) * sin(m) +
              5.0 / 4 * pow(k_ec, 2) * sin(2 * m) +
              13.0 / 12 * pow(k_ec, 3) * sin(3 * m));
    // Compute radius vector.
    o = k_om + d * k_omd;
    w = k_w + d * k_wd;
    r = k_a * (1 - pow(k_ec, 2)) / (1 + k_ec * cos(v));
    // Compute position into the plane of the ecliptic.
    pos[0] = r * (cos(o) * cos(v + w) -
                  sin(o) * sin(v + w) * cos(k_in));
    pos[1] = r * (sin(o) * cos(v + w) +
                  cos(o) * sin(v + w) * cos(k_in));
    pos[2] = r * (sin(v + w) * sin(k_in));
    return 0;
}
