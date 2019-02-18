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
    uint32_t x = 0, f = 0;
    int div = 1, sign = 1, nb;
    char c;
    while (*str == ' ') str++;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    // Whole part.
    nb = 0;
    while (true) {
        c = *str++;
        if (c == '.') break;
        if (c < '0' || c > '9') return -1;
        if (nb++ > 8) return -1;
        x = x * 10 + (c - '0');
    }
    // Fract part.
    nb = 0;
    while (true) {
        c = *str++;
        if (c == ' ' || c == '\0' || c == '\n') break;
        if (c < '0' || c > '9') return -1;
        if (nb++ > 8) return -1;
        f = f * 10 + (c - '0');
        div *= 10;
    }
    *ret = sign * (x + ((double)f / div));
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
/*
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
*/

// Parse 4 digit hexa flag (A804)
static int parse_flags(const char str[static 4])
{
    int i, ret = 0;
    uint8_t c;
    for (i = 0; i < 4; i++) {
        ret *= 16;
        c = str[i];
             if (c >= '0' && c <= '9') ret += (c - '0');
        else if (c >= 'A' && c <= 'F') ret += 10 + (c - 'A');
    }
    return ret;
}

// Replace spaces at the end of a string with '\0'
static void rstrip(char *str, int len)
{
    while (str[len - 1] == ' ') {
        len -= 1;
        str[len] = '\0';
    }
}


static int parse_int(const char *str, int len, int *number)
{
    int i, v = 0;
    char c;
    for (i = 0; i < len; i++) {
        v *= 10;
        c = str[i];
        /**/ if (c >= '0' && c <= '9') v += c - '0';
        else return -1;
    }
    *number = v;
    return 0;
}

// Parse packed number: [0-9a-zA-Z][0-9]*
static int parse_packed_int(const char *str, int len, int *number)
{
    int i, v = 0;
    char c;
    for (i = 0; i < len; i++) {
        v *= 10;
        c = str[i];
        /**/ if (c >= '0' && c <= '9') v += c - '0';
        else if (c >= 'A' && c <= 'Z') v += 10 + c - 'A';
        else if (c >= 'a' && c <= 'z') v += 36 + c - 'a';
        else return -1;
    }
    *number = v;
    return 0;
}

/*
 * Function: mpc_parse_line
 * Parse a line in the Minor Planet Center extended format.
 *
 * https://www.minorplanetcenter.net/data.
 * https://minorplanetcenter.net/Extended_Files/
 *         Extended_MPCORB_Data_Format_Manual.pdf
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
                   int    *flags)
{
    int r;
    if (len < 160) return -1;
    if (line[5] == ' ') { // Got number.
        if (parse_packed_int(line, 5, number)) return -1;
    } else {
        *number = 0;
    }

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
    *flags = parse_flags(line + 161);

    // The readable designation (167-194) might have the name instead.
    if (line[175] != ' ' && !(line[175] >= '0' && line[175] <= '9')) {
        memcpy(name, line + 175, 19);
        rstrip(name, 19);
        memset(desig, 0, 24);
    } else {
        memcpy(desig, line + 175, 19);
        rstrip(desig, 19);
        memset(name, 0, 24);
    }

    // Extra designations.  For the moment we only parse it if the designation
    // wasn't set yet.
    if (!desig[0] && len >= 227) {
        memcpy(desig, line + 217, 10);
        rstrip(desig, 10);
    }
    return 0;
}

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
 *   epoch      - Epoch in MJD TT.
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
                         char   desig[static 64])
{
    int year, month, dayi;
    double dayf, djm0;

    if (len < 160) return -1;
    if (line[0] != ' ') {
        if (parse_int(line + 0, 4, number)) return -1;
    } else {
        *number = 0;
    }
    *orbit_type = line[4];

    if (parse_int(line + 14, 4, &year))         return -1;
    if (parse_int(line + 19, 2, &month))        return -1;
    if (parse_float(line + 22, &dayf))          return -1;
    if (eraCal2jd(year, month, (int)dayf, &djm0, peri_time)) return -1;
    *peri_time += fmod(dayf, 1.0);

    if (parse_float(line + 30, peri_dist))      return -1;
    if (parse_float(line + 41, e))              return -1;
    if (parse_float(line + 51, peri))           return -1;
    if (parse_float(line + 61, node))           return -1;
    if (parse_float(line + 71, i))              return -1;

    if (line[81] != ' ') {
        if (parse_int(line + 81, 4, &year))             return -1;
        if (parse_int(line + 85, 2, &month))            return -1;
        if (parse_int(line + 87, 2, &dayi))             return -1;
        if (eraCal2jd(year, month, dayi, &djm0, epoch)) return -1;
    } else {
        *epoch = 0.0;
    }

    if (parse_float(line + 91, h))              return -1;
    if (parse_float(line + 96, g))              return -1;

    memset(desig, 0, 64);
    memcpy(desig, line + 102, 56);
    rstrip(desig, 56);

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
        "10.5", "-10.6", "0.1", "1.0", "478.878313", "109.8611716"
    };
    for (i = 0; i < ARRAY_SIZE(values); i++) {
        r = parse_float(values[i], &x);
        assert(r == 0);
        y = atof(values[i]);
        assert(x == y);
    }
}

static void test_parse_comet(void)
{
    int r;
    const char *line =
        "    CJ95O010  1997 03 29.4673  0.928143  0.994910  130.7602"
        "  283.2592   89.0370  20190217  -2.0  4.0  "
        "C/1995 O1 (Hale-Bopp)                                    MPC106342";
    int n;
    char o, desig[64];
    double peri_time, peri_dist, e, peri, node, i, epoch, h, g;

    r = mpc_parse_comet_line(
            line, strlen(line), &n, &o, &peri_time, &peri_dist, &e, &peri,
            &node, &i, &epoch, &h, &g, desig);
    assert(r == 0);
    assert(n == 0);
    assert(o == 'C');
    assert(peri_dist == 0.928143);
    assert(e == 0.994910);
    assert(peri == 130.7602);
    assert(node == 283.2592);
    assert(i == 89.0370);
    assert(h == -2.0);
    assert(g == 4.0);
    assert(strcmp(desig, "C/1995 O1 (Hale-Bopp)") == 0);
}

TEST_REGISTER(NULL, test_parse_float, TEST_AUTO);
TEST_REGISTER(NULL, test_parse_comet, TEST_AUTO);
#endif
