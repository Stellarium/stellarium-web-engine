/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Helper functions to parse minor planet data.
 */

#include <stdbool.h>

/*
 * Function: mpc_parse_line
 * Parse a line in the Minor Planet Center compact orbit format:
 * https://www.minorplanetcenter.net/iau/info/MPOrbitFormat.html
 *
 * Parameters:
 *   line       - Pointer to a string.  Doesn't have to be NULL terminated.
 *   h          - Absolute magnitude, H.
 *   g          - Slope parameter, G.
 *   epoch      - Epoch in MJD TT.
 *   m          - Mean anomaly at the epoch in degree.
 *   peri       - Argument of perhielion, J2000.0 (degrees).
 *   node       - Longitude of the ascending node, J2000.0 (degrees).
 *   i          - Inclination to the ecliptic, J2000.0 (degrees).
 *   e          - Orbital eccentricity.
 *   n          - Mean daily motion (degrees per day).
 *   a          - Semimajor axis (AU).
 *   flags      - 4 hexdigit flags.
 *   readable_desig - Readable desig.
 *
 * Return:
 *   0 in case of success, an error code otherwise.
 */
int mpc_parse_line(const char *line,
                   char   desig[static 8],
                   double *h,
                   double *g,
                   double *epoch,
                   double *m,
                   double *peri,
                   double *node,
                   double *i,
                   double *e,
                   double *n,
                   double *a,
                   int    *flags,
                   char   readable_desig[static 32]);
/*
 * Parse a 7 char packed designation
 *
 * The algo is described there:
 * https://www.minorplanetcenter.net/iau/info/PackedDes.html
 *
 * XXX: this should be directly done by mpc_parse_line I guess.
 */
int mpc_parse_designation(
        const char str[7], char type[4], bool *permanent);
