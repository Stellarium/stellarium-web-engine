/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/* Some astronomy related algorithms to extends erfa library. */

#include <stdbool.h>

/************ Healpix utils ************************************************/
/* Get a 3x3 matrix that map uv coordinates to the xy healpix coordinate
 * covering a healpix pixel.
 */
void healpix_get_mat3(int nside, int pix, double out[3][3]);

/* Compute position from healpix xy coordinates. */
void healpix_xy2vec(const double xy[2], double out[3]);

/* Convert healpix xyf coordinate to a nest pix index. */
int healpix_xyf2nest(int nside, int ix, int iy, int face_num);

/* Convert healpix nest coordinate to xyf coordinates. */
int healpix_nest2xyf(int nside, int pix, int *ix, int *iy, int *face_num);

/* Convert healpix nest index to cartesian 3d vector. */
void healpix_pix2vec(int nside, int pix, double out[3]);

/* Convert healpix nest index to polar angle. */
void healpix_pix2ang(int nside, int pix, double *theta, double *phi);

/* Convert polar angle to healpix next index
 *
 * Parameters:
 *   nside  - Nside parameter of the healpix map.
 *   theta  - Colatitude in radians measured southward from north pole in
 *            [0, π].
 *   phi    - Longitude in radians, measured eastward in [0, 2π].
 *   pix    - Pix index output.
 */
void healpix_ang2pix(int nside, double theta, double phi, int *pix);

int healpix_vec2pix(int nside, const double vec[3]);

/*
 * Function: healpix_get_neighbours
 * Returns the neighboring pixels of nest pixel
 *
 * On exit, out contains (in this order)
 * the pixel numbers of the SW, W, NW, N, NE, E, SE and S neighbor
 * of pix.  If a neighbor does not exist (this can only be the case
 * for the W, N, E and S neighbors), its entry is set to -1.
 */
void healpix_get_neighbours(int nside, int pix, int out[8]);

/*
 * Function: healpix_get_boundaries
 * Return the four corner position of a given healpix nest pixel
 */
void healpix_get_boundaries(int nside, int pix, double out[4][3]);

/*
 * Function: healpix_get_bounding_cap
 * Return the cap containing the given healpix nest pixel
 */
void healpix_get_bounding_cap(int nside, int pix, double out[4]);

/* Compute moon position.
 *
 * inputs:
 *   jde: Julian ephemeris day.
 * outputs
 *   lambda: geocentric longitude (mean equinox and ecliptic).
 *   beta: gecentric latitude (mean equinox and ecliptic).
 *   dist: distance to earth center (AU).
 */
int moon_pos(double jde, double *lambda, double *beta, double *dist);

/*
 * Function: pluto_pos
 * Compute Pluto position.
 *
 * Parameters:
 *   tt_mjd     - TT time (MJD)
 *   pos        - Output position, heliocentric, ICRF, AU
 *
 * Return:
 *   zero.
 */
int pluto_pos(double tt_mjd, double pos[3]);


/* Compute delta-t
 *
 * inputs:
 *   ut: time (in UT)
 * return:
 *   delta-t value.
 */
double deltat(double ut);

/* Compute saturn rings orientation
 *
 * inputs:
 *   sb: Saturn heliocentric ecliptic latitude (rad).
 *   sl: Saturn heliocentric ecliptic longitude (rad).
 *   sr: Saturn distance to sun (AU).
 *   jd: time (JDE?)
 * outputs:
 *   etiltp: tilt from Earth (rad south).
 *   stiltp: tilt from Sun (rad south).
 */
void satrings(
    double sb, double sl, double sr,
    double el, double er,
    double jd,
    double *etiltp, double *stiltp);

/* Format functions to simplify printf.
 *
 * All those functions take a char buffer as a first argument, and return
 * the same buffer.
 */
const char *format_time(char *buf, double jdm, double utcoffset,
                        const char *format);
const char *format_angle(char *buf, double a, char type, int ndp,
                         const char *fmt);
const char *format_dangle(char *buf, double a);
const char *format_hangle(char *buf, double a);
const char *format_dist(char *buf, double d); // d in AU.


/*
 * Function: refraction
 * Refraction computation
 *
 * The refa and refb parameters should be pre-computed with the
 * <refraction_prepare> function.
 *
 * inputs:
 *   v: cartesian coordinates (Z up).
 *   refa: refraction A argument.
 *   refb: refraction B argument.
 *
 * outputs:
 *   out: corrected cartesian coordinates.
 *
 */
void refraction(const double v[3], double refa, double refb,
                double out[3]);
void refraction_inv(const double v[3], double refa, double refb,
                    double out[3]);

/*
 * Function: refraction_prepare
 * Compute the constants A and B used in the refraction computation.
 *
 * Parameters:
 *   phpa   - Pressure at the observer (millibar).
 *   tc     - Temperature at the observer (deg C).
 *   rh     - Relative humidity at the observer (range 0-1).
 *   refa   - Output of the A coefficient.
 *   refb   - Output of the B coefficient.
 */
void refraction_prepare(double phpa, double tc, double rh,
                        double *refa, double *refb);


/* Galilean satellites positions using l1.2 semi-analytic theory by
 * L.Duriez.
 * ftp://ftp.imcce.fr/pub/ephem/satel/galilean/L1/L1.2/
 */
int l12(double tt1, double tt2, int ks, double pv[2][3]);


/* Tass1.7 model of Saturnian Satellites.
 *
 * Parameters:
 *   body - integer in the list:
 *      MIMAS     0
 *      ENCELADUS 1
 *      TETHYS    2
 *      DIONE     3
 *      RHEA      4
 *      TITAN     5
 *      IAPETUS   6
 *      HYPERION  7
 */
void tass17(double jd, int body, double xyz[3], double xyzdot[3]);

/* Gust86 model of Uranus Satellites.
 *
 * Parameters:
 *   body - integer in the list:
 *      MIRANDA   0
 *      ARIEL     1
 *      UMBRIEL   2
 *      TITANIA   3
 *      OBERON    4
 */
void gust86(double jd, int body, double xyz[3], double xyzdot[3]);

/*
 * Find which constellation a point is located in.
 *
 * Parameters:
 *   pos    - A cartesian position in ICRS.
 *   id     - Get the name of the constellation.
 *
 * Returns:
 *   The index of the constellation.
 *
 */
int find_constellation_at(const double pos[3], char id[5]);

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
        double wd);       // variation of w in time (rad/day).

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
                           double *i,
                           double *o,
                           double *w,
                           double *a,
                           double *n,
                           double *e,
                           double *ma);

/*
 * Function: bv_to_rgb
 * Convert a B-V color index value to an RGB color.
 */
void bv_to_rgb(double bv, double rgb[3]);
