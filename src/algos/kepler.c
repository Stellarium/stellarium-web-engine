/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>
#define PI (3.141592653589793238462643)

/*
 * Function: orbit_compute_pv
 * Compute position and speed from orbit elements.
 *
 * Parameters:
 *   mjd    - Time of the position (MJD).
 *   pos    - Get the computed position.
 *   speed  - Get the computed speed (can be NULL).
 *   d      - Orbit base epoch (MJD).
 *   i      - inclination (rad).
 *   o      - Longitude of the Ascending Node (rad).
 *   w      - Argument of Perihelion (rad).
 *   a      - Mean distance (Semi major axis).
 *   n      - Daily motion (rad/day).
 *   e      - Eccentricity.
 *   ma     - Mean Anomaly (rad).
 *   od     - variation of o in time (rad/day).
 *   wd     - variation of w in time (rad/day).
 *
 * Return:
 *   zero.
 */
int orbit_compute_pv(
        double mjd, double pos[3], double speed[3],
        double d,         // epoch date (MJD).
        double i,         // inclination (rad).
        double o,         // Longitude of the Ascending Node (rad).
        double w,         // Argument of Perihelion (rad).
        double a,         // Mean distance (Semi major axis).
        double n,         // Daily motion (rad/day).
        double e,         // Eccentricity.
        double ma,        // Mean Anomaly (rad).
        double od,        // variation of o in time (rad/day).
        double wd)        // variation of w in time (rad/day).
{
    double m, v, r, rdot, rfdot, u;
    // Get the number of day since element date.
    d = mjd - d;
    // Compute the mean anomaly.
    m = n * d + ma;
    m = fmod(m, 2.0 * PI);

    // Compute true anomaly.
    // We use an proximation to solve the Kepler equation without a loop.
    // See: http://www.stargazing.net/kepler/ellipse.html
    v = m + ((2.0 * e - pow(e, 3) / 4) * sin(m) +
              5.0 / 4 * pow(e, 2) * sin(2 * m) +
              13.0 / 12 * pow(e, 3) * sin(3 * m));

    // Compute radius vector.
    o = o + d * od;
    w = w + d * wd;
    r = a * (1 - pow(e, 2)) / (1 + e * cos(v));
    u = v + w;
    // Compute position into the plane of the ecliptic.
    pos[0] = r * (cos(o) * cos(u) - sin(o) * sin(u) * cos(i));
    pos[1] = r * (sin(o) * cos(u) + cos(o) * sin(u) * cos(i));
    pos[2] = r * (sin(u) * sin(i));

    // Compute speed if required.
    if (!speed) return 0;
    rdot = n * a * (e * sin(v)) / sqrt(1.0 - e * e);
    rfdot = n * a * (1.0 + e * cos(v)) / sqrt(1.0 - e * e);
    speed[0] = rdot * (cos(u) * cos(o) - sin(u) * sin(o) * cos(i)) +
               rfdot * (-sin(u) * cos(o) - cos(u) * sin(o) * cos(i));
    speed[1] = rdot * (cos(u) * sin(o) + sin(u) * cos(o) * cos(i)) +
               rfdot * (-sin(u) * sin(o) + cos(u) * cos(o) * cos(i));
    speed[2] = rdot * (sin(u) * sin(i)) + rfdot * (cos(u) * sin(i));
    return 0;
}
