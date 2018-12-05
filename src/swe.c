/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <sys/time.h>

#ifndef LOG_TIME
#   define LOG_TIME 1
#endif


EMSCRIPTEN_KEEPALIVE
const char *get_compiler_str(void)
{
    return SWE_COMPILER_STR;
}

static double get_log_time()
{
    static double origin = 0;
    double time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = tv.tv_sec + (double)tv.tv_usec / (1000 * 1000);
    if (!origin) origin = time;
    return time - origin;
}

void dolog(int level, const char *msg,
           const char *func, const char *file, int line, ...)
{
    const bool use_colors = !DEFINED(__APPLE__) && !DEFINED(__EMSCRIPTEN__);
    char *msg_formatted, *full_msg;
    const char *format;
    char time_str[32] = "";
    va_list args;

    va_start(args, line);
    vasprintf(&msg_formatted, msg, args);
    va_end(args);

    if (use_colors && level >= NOC_LOG_WARN) {
        format = "\e[33;31m%s%-60s\e[m %s (%s:%d)";
    } else {
        format = "%s%-60s %s (%s:%d)";
    }

    if (DEFINED(LOG_TIME))
        sprintf(time_str, "%f: ", get_log_time());

    file = file + max(0, (int)strlen(file) - 20); // Truncate file path.
    asprintf(&full_msg, format, time_str, msg_formatted, func, file, line);
    sys_log(full_msg);

    free(msg_formatted);
    free(full_msg);
}

/*
 * Function: gen_doc
 * Print out generated documentation about the defined classes.
 */
void swe_gen_doc(void)
{
    obj_klass_t *klasses, *klass;
    attribute_t *attr;

    printf("/*\n");
    printf("File: SWE Classes\n");
    klasses = obj_get_all_klasses();
    LL_FOREACH(klasses, klass) {
        printf("\n\n");
        printf("Class: %s\n", klass->id);
        printf("\n");
        if (!klass->attributes) continue;
        printf("Attributes:\n");
        for (attr = &klass->attributes[0]; attr->name; attr++) {
            printf("  %s - %s", attr->name, attr->desc ?: "");
            if (attr->is_prop) {
                printf(" *(%s%s%s)* ",
                       attr->type, attr->hint ? ", " : "", attr->hint ?: "");
            } else {
                printf(" *(function)*");
            }
            printf("\n");
        }
    }
    printf("*/\n");
}


#ifdef __EMSCRIPTEN__

// Expose erfaA2tf to js.
EMSCRIPTEN_KEEPALIVE
char *a2tf_json(int resolution, double angle)
{
    char s;
    int hmsf[4];
    char *ret;
    eraA2tf(resolution, angle, &s, hmsf);
    asprintf(&ret, "{"
            "\"sign\": \"%c\","
            "\"hours\": %d,"
            "\"minutes\": %d,"
            "\"seconds\": %d,"
            "\"fraction\": %d}",
            s, hmsf[0], hmsf[1], hmsf[2], hmsf[3]);
    return ret;
}

// Expose erfaA2af to js.
EMSCRIPTEN_KEEPALIVE
char *a2af_json(int resolution, double angle)
{
    char s;
    int dmsf[4];
    char *ret;
    eraA2af(resolution, angle, &s, dmsf);
    asprintf(&ret, "{"
            "\"sign\": \"%c\","
            "\"degrees\": %d,"
            "\"arcminutes\": %d,"
            "\"arcseconds\": %d,"
            "\"fraction\": %d}",
            s, dmsf[0], dmsf[1], dmsf[2], dmsf[3]);
    return ret;
}

#endif

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

/*
    Some data from USNO to test from.

    Atlanta (UTC + 4)
    Sunday
    6 September 2009      Eastern Daylight Time

                     SUN
    Begin civil twilight       6:50 a.m.
    Sunrise                    7:15 a.m.
    Sun transit                1:36 p.m.
    Sunset                     7:56 p.m.
    End civil twilight         8:21 p.m.

                     MOON
    Moonrise                   8:17 p.m. on preceding day
    Moon transit               2:37 a.m.
    Moonset                    9:05 a.m.
    Moonrise                   8:44 p.m.
    Moonset                   10:05 a.m. on following day
*/

// Convenience function to call eraDtf2d.
static double dtf2d(int iy, int im, int id, int h, int m, double s)
{
    double d1, d2;
    int r;
    r = eraDtf2d("", iy, im, id, h, m, s, &d1, &d2);
    assert(r == 0);
    return d1 - DJM0 + d2;
}

static void test_events(void)
{
    obj_t *sun, *moon;
    double t, djm0, djm;
    const double sec = 1. / 24 / 60 / 60;
    core_init();

    // Set atlanta coordinates.
    core->observer->elong = -84.4 * DD2R;
    core->observer->phi = 33.8 * DD2R;
    // USNO computation of refraction.
    core->observer->pressure = 0;
    core->observer->horizon = -34.0 / 60.0 * DD2R;
    sun = obj_get(NULL, "sun", 0);
    moon = obj_get(NULL, "moon", 0);
    assert(sun && moon);
    eraCal2jd(2009, 9, 6, &djm0, &djm);

    // Sun:
    // Get next rising.
    t = compute_event(core->observer, sun, EVENT_RISE, djm, djm + 1, sec);
    assert(fabs(t - dtf2d(2009, 9, 6, 11, 15, 0)) < 1. / 24 / 60);
    // Get next setting.
    t = compute_event(core->observer, sun, EVENT_SET, djm, djm + 1, sec);
    assert(fabs(t - dtf2d(2009, 9, 6, 23, 56, 0)) < 1. / 24 / 60);

    // Moon:
    // Get next rising.
    t = compute_event(core->observer, moon, EVENT_RISE, djm, djm + 1, sec);
    assert(fabs(t - dtf2d(2009, 9, 6, 0, 17, 0)) < 1. / 24 / 60);
    // Get next setting.
    t = compute_event(core->observer, moon, EVENT_SET, djm, djm + 1, sec);
    assert(fabs(t - dtf2d(2009, 9, 6, 13, 5, 0)) < 1. / 24 / 60);
}

/*
 * Test of positions compared with pyephem.
 *
 * Notes: pyephem uses three different ways to specify ra/dec.  From the
 * doc:
 *
 * a_ra, a_dec - Astrometric Geocentric Position for the star atlas epoch.
 * g_ra, g_dec - Apparent Geocentric Position for the epoch-of-date.
 * ra, dec - Apparent Topocentric Position for the epoch-of-date
 */
static void test_ephemeris(void)
{
    obj_t *obj;
    observer_t *obs;
    const struct {
        const char *name;
        double date;
        double lon;
        double lat;
        double a_ra;
        double a_dec;
        double ra;
        double dec;
        double alt;
        double az;
    } ephs[] = {
        // Values generated with tools/compute-ephemeris.py.
        {"Sun", 55080.7083, -84.39, 33.75,
         165.35, 6.26, 165.48, 6.20, 61.24, 161.26},
        {"Moon", 55080.7083, -84.39, 33.75,
         5.12, 7.42, 4.88, 6.89, -41.29, 321.14},
        {"HIP 11767", 55080.7083, -84.39, 33.75, // Polaris
         37.96, 89.26, 41.07, 89.30, 33.47, 359.25},
        {"Jupiter", 55080.7083, -84.39, 33.75,
         321.89, -16.14, 322.03, -16.09, -68.04, 40.04},
        {"Io", 55080.7083, -84.39, 33.75,
         321.88, -16.14, 322.03, -16.10, -68.04, 40.05},
    };
    int i;
    struct { double apparent_radec[4], apparent_azalt[4]; } got;
    struct { double apparent_radec[4], apparent_azalt[4]; } expected;
    // 40 arcsec, limited by test data truncation after 2 digits..
    const double precisionRadec = 40.0 / 3600;
    // 2 arcmin, limited by refraction settings
    const double precisionAzAlt = 2.0 / 60;
    double sep, apparent_radec_icrf[4];

    core_init();
    for (i = 0; i < ARRAY_SIZE(ephs); i++) {
        obs = core->observer;
        obj_set_attr((obj_t*)obs, "utc", "f", ephs[i].date);
        obj_set_attr((obj_t*)obs, "longitude", "f", ephs[i].lon * DD2R);
        obj_set_attr((obj_t*)obs, "latitude", "f", ephs[i].lat * DD2R);
        obs->refraction = false;
        observer_update(obs, false);
        obj = obj_get(NULL, ephs[i].name, 0);
        assert(obj);

        obj_update(obj, core->observer, 0);
        obj_get_attr(obj, "radec", "v4", apparent_radec_icrf);
        convert_directionv4(obs, FRAME_ICRF, FRAME_JNOW,
                          apparent_radec_icrf, got.apparent_radec);
        convert_directionv4(obs, FRAME_ICRF, FRAME_OBSERVED,
                          apparent_radec_icrf, got.apparent_azalt);

        eraS2c(ephs[i].ra * DD2R, ephs[i].dec * DD2R, expected.apparent_radec);
        eraS2c(ephs[i].az * DD2R, ephs[i].alt * DD2R, expected.apparent_azalt);

        sep = eraSepp(got.apparent_radec, expected.apparent_radec) * DR2D;
        if (sep > precisionRadec) {
            LOG_E("Error: %s", ephs[i].name);
            LOG_E("Apparent radec JNow error: %.5f°", sep);
            assert(false);
        }
        sep = eraSepp(got.apparent_azalt, expected.apparent_azalt) * DR2D;
        if (sep > precisionAzAlt) {
            LOG_E("Error: %s", ephs[i].name);
            LOG_E("Apparent azalt error: %.5f°", sep);
            double az, alt, dist;
            eraP2s(expected.apparent_azalt, &az, &alt, &dist);
            az = eraAnp(az);
            LOG_E("Ref az: %f°, alt: %f°, %f AU", az * DR2D, alt * DR2D, dist);
            eraP2s(got.apparent_azalt, &az, &alt, &dist);
            az = eraAnp(az);
            LOG_E("Tst az: %f°, alt: %f°, %f AU", az * DR2D, alt * DR2D, dist);
            assert(false);
        }
        obj_release(obj);
    }
}

TEST_REGISTER(NULL, test_events, 0);
TEST_REGISTER(NULL, test_ephemeris, TEST_AUTO);

#endif

