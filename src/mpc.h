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
 * Enum: MPC_ORBIT_TYPE
 *
 * As defined here:
 * https://www.minorplanetcenter.net/iau/info/MPOrbitFormat.html
 */
enum {
    MPC_ATIRA = 1,
    MPC_ATEN,
    MPC_APOLLO,
    MPC_AMOR,
    MPC_OBJ_WITH_Q_INF_1_665_AU,
    MPC_HUNGARIA,
    MPC_PHOCAEA,
    MPC_JUPITER_TROJAN,
    MPC_DISTANT_OBJECT,
};

/*
 * Function: mpc_parse_line
 * Parse a line in the Minor Planet Center compact orbit format:
 * https://www.minorplanetcenter.net/iau/info/MPOrbitFormat.html
 *
 * Parameters:
 *   line       - Pointer to a string.  Doesn't have to be NULL terminated.
 *   len        - Length of the line.
 *   number     - Number if the asteroid has received one, else 0.
 *   name       - Name if the asteroid has received one.
 *   desig      - Principal designation.
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
 *
 * Return:
 *   0 in case of success, an error code otherwise.
 */
int mpc_parse_line(const char *line, int len,
                   int    *number,
                   char   name[static 24],
                   char   desig[static 24],
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
                   int    *flags);

/*
 * Function: mpc_parse_comet_line
 * Parse a line in the Minor Planet Center comet orbit format:
 * https://www.minorplanetcenter.net/iau/info/CometOrbitFormat.html
 * (Ephemerides and Orbital Elements Format)
 *
 * Parameters:
 *   line       - Pointer to a string.  Doesn't have to be NULL terminated.
 *   len        - Length of the line.
 *   number     - Periodic comet number.
 *   orbit_type - Orbit type (generally 'C', 'P', or 'D').
 *   peri_time  - Time of perihelion passage (MJD TT).
 *   peri_dist  - Perihelion distance (AU).
 *   e          - Orbital eccentricity.
 *   peri       - Argument of perhielion, J2000.0 (degrees).
 *   node       - Longitude of the ascending node, J2000.0 (degrees).
 *   i          - Inclination to the ecliptic, J2000.0 (degrees).
 *   epoch      - Epoch in MJD TT, or zero if not set.
 *   h          - Absolute magnitude, H.
 *   g          - Slope parameter, G.
 *   desig      - Designation and Name. e.g: 'C/1995 O1 (Hale-Bopp)'.
 *                Zero padded.
 */
int mpc_parse_comet_line(const char *line, int len,
                         int    *number,
                         char   *orbit_type,
                         double *peri_time,
                         double *peri_dist,
                         double *e,
                         double *peri,
                         double *node,
                         double *i,
                         double *epoch,
                         double *h,
                         double *g,
                         char   desig[static 64]);
