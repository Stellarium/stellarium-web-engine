/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <assert.h>
#include <math.h>
#define PI (3.141592653589793238462643)

static void vec3_cross(const double a[3], const double b[3], double out[3])
{
    double tmp[3];
    tmp[0] = a[1] * b[2] - a[2] * b[1];
    tmp[1] = a[2] * b[0] - a[0] * b[2];
    tmp[2] = a[0] * b[1] - a[1] * b[0];
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

static double vec3_norm2(const double v[3])
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

static double vec3_norm(const double v[3])
{
    return sqrt(vec3_norm2(v));
}

static double vec3_dot(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static double kepler(double m, double de, double precision)
{
    double e0, e1;
    e0 = m + de * sin(m) * (1.0 + de * cos(m));
    do {
        e1 = e0;
        e0 = e1 - (e1 - de * sin(e1) - m) / (1.0 - de * cos(e1));
    } while (fabs(e0 - e1) > precision);
    return e0;
}

/*
 * Function: orbit_compute_pv
 * Compute position and speed from orbit elements.
 *
 * Parameters:
 *   precision - Precision for the kepler equation in rad.
 *               set to 0.0 to use a faster non looping algorithm.
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
        double precision,
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
    double m, v, r, rdot, rfdot, u, ae, ae2;
    // Get the number of day since element date.
    d = mjd - d;
    // Compute the mean anomaly.
    m = n * d + ma;
    m = fmod(m, 2.0 * PI);

    // Compute true anomaly.
    // We use an proximation to solve the Kepler equation without a loop.
    // See: http://www.stargazing.net/kepler/ellipse.html
    if (precision == 0.0) {
        v = m + ((2.0 * e - pow(e, 3) / 4) * sin(m) +
                  5.0 / 4 * pow(e, 2) * sin(2 * m) +
                  13.0 / 12 * pow(e, 3) * sin(3 * m));
    } else {
        ae = kepler(m, e, precision);
        ae2 = ae / 2.0;
        v = 2.0 * atan2(sqrt((1.0 + e) / (1.0 - e)) * sin(ae2), cos(ae2));
    }

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

/*
 * Function: orbit_elements_from_pv
 * Compute Kepler orbit element from a body positon and speed.
 *
 * The unit of the position, speed and mu input should match, so for example
 * if p and v are using AU and day, then mu should be in (AU)³(day)⁻².
 *
 * Parameters:
 *   p      - Cartesian position from parent body.
 *   v      - Cartesian speed from parent body.
 *   mu     - Standard gravitational parameter (μ).
 *   i      - Output inclination (rad).
 *   o      - Output longitude of the Ascending Node (rad).
 *   w      - Output argument of Perihelion (rad).
 *   a      - Output mean distance (Semi major axis).
 *   n      - Output daily motion (rad/day).
 *   e      - Output eccentricity.
 *   ma     - Output mean Anomaly (rad).
 */
int orbit_elements_from_pv(const double p[3], const double v[3], double mu,
                           double *k_in,
                           double *k_om,
                           double *k_w,
                           double *k_a,
                           double *k_n,
                           double *k_ec,
                           double *k_ma)
{
    // This code is inspired by the algo used in
    // https://github.com/RazerM/orbital.git.

    int k;
    double h[3], n[3], ev[3], np, nv2, pdv, en, a, e, i, om, w, f, ec, ma;
    const double epsilon = 1e-15;
    assert(mu);

    vec3_cross(p, v, h);
    vec3_cross((double[]){0, 0, 1}, h, n);

    // Eccentricity vector.
    np = vec3_norm(p);
    nv2 = vec3_norm2(v);
    pdv = vec3_dot(p, v);
    for (k = 0; k < 3; k++) {
        ev[k] = 1.0 / mu * ((nv2 - mu / np) * p[k] - pdv * v[k]);
    }

    // Specific orbital energy.
    en = vec3_norm2(v) / 2.0 - mu / vec3_norm(p);

    a = -mu / (2.0 * en);
    e = vec3_norm(ev);
    i = acos(h[2] / vec3_norm(h));

    if (fabs(i) < epsilon) {
        // For non-inclined orbits, long of asc node is undefined.
        // set to zero by convention
        om = 0.0;
        if (fabs(e) < epsilon) w = 0.0;
        else w = acos(ev[0] / vec3_norm(ev));
    } else {
        // Right ascension of ascending node is the angle
        // between the node vector and its x component.
        om = acos(n[0] / vec3_norm(n));
        if (n[1] < 0.0) om = 2.0 * PI - om;
        // Argument of periapsis is angle between
        // node and eccentricity vectors.
        w = acos(vec3_dot(n, ev) / (vec3_norm(n) * vec3_norm(ev)));
    }

    if (fabs(e) < epsilon) {
        if (fabs(i) < epsilon) {
            // True anomaly is angle between position
            // vector and its x component.
            f = acos(p[0] / vec3_norm(p));
            if (v[0] > 0) f = 2.0 * PI - f;
        } else {
            // True anomaly is angle between node
            // vector and position vector.
            f = acos(vec3_dot(n, p) / (vec3_norm(n) * vec3_norm(p)));
            if (vec3_dot(n, v) > 0.0) f = 2 * PI - f;
        }
    } else {
        if (ev[2] < 0.0) w = 2.0 * PI - w;
        // True anomaly is angle between eccentricity
        // vector and position vector.
        f = acos(vec3_dot(ev, p) / (vec3_norm(ev) * vec3_norm(p)));
        if (vec3_dot(p, v) < 0.0) f = 2.0 * PI - f;
    }

    // Convert true anomaly to eccentric anomaly.
    ec = atan2(sqrt(1.0 - e * e) * sin(f), e + cos(f));
    ma = ec - e * sin(ec);

    *k_in = i;
    *k_om = om;
    *k_w = w;
    *k_a = a;
    *k_n = sqrt(mu / a * a * a);
    *k_ec = e;
    *k_ma = ma;

    return 0;
}
