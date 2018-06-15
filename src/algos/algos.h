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

/* Convert healpix nest index to cartesian 3d vector. */
void healpix_pix2vec(int nside, int pix, double out[3]);

/* Convert healpix nest index to polar angle. */
void healpix_pix2ang(int nside, int pix, double *theta, double *phi);

/* Convert polar angle to healpix next index */
void healpix_ang2pix(int nside, double theta, double phi, int *pix);

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

/* Parse a string into a mjd time
 *
 * inputs
 *   scale  char[]  "UTC".
 *   str    char[]  the iso time string.
 *
 * returns:
 *   mjd    double  the MJD time.
 *
 * returned value:
 *   int    status (0 if no error).
 */
int time_parse_iso(const char *scale, const char *str, double *mjd);

/* Set time from gregorian calendar value.  Any value equal to -1 means we
 * don't change it.
 */
double time_set_dtf(double utc, double utcoffset,
                    int year, int month, int day,
                    int h, int m, int s);

/* Increase time using gregorian calendar value. */
double time_add_dtf(double utc, double utcoffset, int year,
                    int month, int day,
                    int h, int m, int s);

/* Format functions to simplify printf.
 *
 * All those functions take a char buffer as a first argument, and return
 * the same buffer.
 */
const char *format_time(char *buf, double jdm, double utcoffset,
                        const char *format);
const char *format_dangle(char *buf, double a);
const char *format_hangle(char *buf, double a);
const char *format_dist(char *buf, double d); // d in AU.


/* Refraction computation
 *
 * Using A*tan(z)+B*tan^3(z) model, with Newton-Raphson correction.
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
void refraction(const double v[3], double refa, double refb, double out[3]);


/* Galilean satellites positions using l1.2 semi-analytic theory by
 * L.Duriez.
 * ftp://ftp.imcce.fr/pub/ephem/satel/galilean/L1/L1.2/
 */
int l12(double tt1, double tt2, int ks, double pv[2][3]);
