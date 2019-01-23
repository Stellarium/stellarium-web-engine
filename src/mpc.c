/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "mpc.h"

#include "erfa.h" // Used for eraDtf2d.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Faster than atof.
static inline int parse_float(const char *str, double *ret)
{
    uint32_t x = 0;
    int div = 1, sign = 1, nb = 0;
    char c;
    while (*str == ' ') str++;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    // Whole part.
    while (true) {
        c = *str++;
        if (c == '.') break;
        if (c < '0' || c > '9') return -1;
        if (nb++ > 8) return -1;
        x = x * 10 + (c - '0');
    }
    // Fract part.
    while (true) {
        c = *str++;
        if (c == ' ' || c == '\0' || c == '\n') break;
        if (c < '0' || c > '9') return -1;
        if (nb++ > 8) return -1;
        x = x * 10 + (c - '0');
        div *= 10;
    }
    *ret = sign * ((double)x / div);
    return 0;
}

static int unpack_char(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return 10 + c - 'A';
    else if (c >= 'a' && c < 'z')
        return 36 + c - 'a';
    else {
        return -1;
    }
}

// Unpack epoch field in MJD.
// Return 0 for success.
static int unpack_epoch(const char epoch[static 5], double *ret)
{
    int year, month, day, r;
    double d1, d2;
    year = ((epoch[0] - 'I') + 18) * 100;
    year += (epoch[1] - '0') * 10;
    year += (epoch[2] - '0');
    month = unpack_char(epoch[3]);
    day = unpack_char(epoch[4]);
    r = eraDtf2d("", year, month, day, 0, 0, 0, &d1, &d2);
    if (r != 0) {
        *ret = 0;
        return -1;
    }
    *ret = d1 - ERFA_DJM0 + d2;
    return 0;
}


/*
 * Parse a 7 char packed designation
 *
 * The algo is described there:
 * https://www.minorplanetcenter.net/iau/info/PackedDes.html
 */
int mpc_parse_designation(
        const char str[7], char type[4], bool *permanent)
{
    int ret;
    // 5 chars => permanent designations.
    if (strlen(str) == 5) {
        *permanent = true;
        // Minor planet.
        if (str[4] >= '0' && str[4] <= '9') {
            memcpy(type, "MPl ", 4);
            ret = atoi(str + 1) + 10000 * unpack_char(str[0]);
        }
        // Comet.
        else if (str[4] == 'P' || str[4] == 'D') {
            memcpy(type, "Com ", 4);
            ret = atoi(str);
        }
        // Natural satellites (not supported yet).
        else {
            LOG_W("MPC natural satellites not supported");
            memcpy(type, "    ", 4);
            ret = 0;
        }
    }
    // 7 char -> Provisional designation.
    // Not supported yet.
    else {
        *permanent = false;
        memcpy(type, "    ", 4);
        ret = 0;
    }
    return ret;
}

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
                   char desgn[static 8],
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
                   char readable_desgn[static 32])
{
    int r;
    memcpy(desgn, line + 0, 7);
    desgn[7] = '\0';
    if (parse_float(line + 8, h)) return -1;
    if (parse_float(line + 14, g)) return -1;
    r = unpack_epoch(line + 20, epoch);
    if (r) return r;
    if (parse_float(line + 26, m))      return -1;
    if (parse_float(line + 37, peri))   return -1;
    if (parse_float(line + 48, node))   return -1;
    if (parse_float(line + 59, i))      return -1;
    if (parse_float(line + 70, e))      return -1;
    if (parse_float(line + 80, n))      return -1;
    if (parse_float(line + 92, a))      return -1;
    *flags = strtol(line + 161, NULL, 16);
    memcpy(readable_desgn, line + 166, 28);
    readable_desgn[28] = '\0';
    return 0;
}


/******* TESTS **********************************************************/

#if COMPILE_TESTS

#include "swe.h"

static void test_parse_float(void)
{
    double x, y;
    int r, i;
    const char *values[] = {
        "10.5", "-10.6", "0.1", "1.0", "478.878313",
    };
    for (i = 0; i < ARRAY_SIZE(values); i++) {
        r = parse_float(values[i], &x);
        assert(r == 0);
        y = atof(values[i]);
        assert(x == y);
    }
}

TEST_REGISTER(NULL, test_parse_float, TEST_AUTO);
#endif
