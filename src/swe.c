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
        snprintf(time_str, sizeof(time_str), "%f: ", get_log_time());

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
                printf(" *(%s)* ", obj_info_type_str(attr->type));
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

/*
 * Test of positions compared with skyfield computed values.
 */

typedef struct
{
    const char *name;
    double utc;
    double ut1;
    double longitude;
    double latitude;
    double ra;
    double dec;
    double alt;
    double az;
    double pos[3]; // ICRF, observer centric.
    double geo[3];
    int planet;
    const char *klass;
    const char *json;
    // Precisions in arcsec.
    double precision_radec;
    double precision_azalt;
} pos_test_t;

static void test_pos(pos_test_t t)
{
    obj_t *obj;
    observer_t obs;
    double ra, dec, az, alt;
    double sep, pvo[2][4], p[4];

    // Convert the coordinates angles in the test to radian.
    t.ra *= DD2R;
    t.dec *= DD2R;
    t.alt *= DD2R;
    t.az *= DD2R;

    obs = *core->observer;
    obj_set_attr((obj_t*)&obs, "utc", t.utc);
    obj_set_attr((obj_t*)&obs, "longitude", t.longitude * DD2R);
    obj_set_attr((obj_t*)&obs, "latitude", t.latitude * DD2R);
    obs.pressure = 0;
    observer_update(&obs, false);

    if (t.planet)
        obj = core_get_planet(t.planet);
    else if (t.klass)
        obj = obj_create_str(t.klass, t.json);
    else
        assert(false);
    assert(obj);

    obj_get_pvo(obj, &obs, pvo);
    sep = eraSepp(pvo[0], t.pos) * DR2D * 3600;
    if (sep > t.precision_radec) {
        LOG_E("Error %s", t.name);
        LOG_E("ICRF (observer) error: %.5f arcsec", sep);
        assert(false);
    }

    convert_framev4(&obs, FRAME_ICRF, FRAME_JNOW, pvo[0], p);
    eraC2s(p, &ra, &dec);

    convert_framev4(&obs, FRAME_ICRF, FRAME_OBSERVED, pvo[0], p);
    eraC2s(p, &az, &alt);

    sep = eraSeps(ra, dec, t.ra, t.dec) * DR2D * 3600;
    if (sep > t.precision_radec) {
        LOG_E("Error: %s", t.name);
        LOG_E("Apparent radec JNow error: %.5f arcsec", sep);
        LOG_E("Ref ra: %f°, dec: %f°",
              eraAnp(t.ra) * DR2D, eraAnpm(t.dec) * DR2D);
        LOG_E("Tst ra: %f°, dec: %f°",
              eraAnp(ra) * DR2D, eraAnpm(dec) * DR2D);
        assert(false);
    }

    sep = eraSeps(az, alt, t.az, t.alt) * DR2D * 3600;
    if (sep > t.precision_azalt) {
        LOG_E("Error: %s", t.name);
        LOG_E("Apparent azalt error: %.5f arcsec", sep);
        LOG_E("Ref az: %f°, alt: %f°",
              eraAnp(t.az) * DR2D, eraAnpm(t.alt) * DR2D);
        LOG_E("Tst az: %f°, alt: %f°",
              eraAnp(az) * DR2D, eraAnpm(alt) * DR2D);
        assert(false);
    }

    obj_release(obj);
}


static void test_ephemeris(void)
{
    static const pos_test_t POS_TESTS[] = {
        #include "ephemeris_tests.inl"
    };
    int i;
    for (i = 0; i < ARRAY_SIZE(POS_TESTS); i++) {
        test_pos(POS_TESTS[i]);
    }
}

// Test accuracy of eraEpv00 function in distant past.
static void test_epv00(void)
{
    // Values computed with skyfield.
    struct {
        double tt;
        double pos[3];
    } DATA[] = {
        {2459130.209134074, {0.96697733, 0.23110802, 0.10018191}},
        {-1930719.791178426, {-1.00728366, -0.04192629, -0.00642386}},
    };
    int i;
    double pvh[2][3], pvb[2][3];
    for (i = 0; i < ARRAY_SIZE(DATA); i++) {
        eraEpv00(0, DATA[i].tt, pvh, pvb);
        assert(eraSepp(pvh[0], DATA[i].pos) < 0.2 * DD2R);
    }
}


static void test_clipping(void)
{
    double ra, de, lon, lat, pos[3], az, alt, fov, utc1, utc2;
    int order, pix;
    bool r;
    observer_t obs = *core->observer;
    projection_t proj;
    painter_t painter;

    // Setup observer, pointing at the target coordinates.
    // (NGC 4676 viewed from Paris, 2019-06-14 23:16:00 UTC).
    eraDtf2d("UTC", 2019, 6, 14, 23, 16, 0, &utc1, &utc2);
    obj_set_attr((obj_t*)&obs, "utc", utc1 - DJM0 + utc2);
    lat = 48.85341 * DD2R;
    lon = 2.3488 * DD2R;
    obj_set_attr((obj_t*)&obs, "longitude", lon);
    obj_set_attr((obj_t*)&obs, "latitude", lat);
    observer_update(&obs, false);
    // Compute aziumth and altitude position.
    eraTf2a('+', 12, 46, 10.6, &ra);
    eraAf2a('+', 30, 44,  2.6, &de);
    eraS2c(ra, de, pos);
    convert_frame(&obs, FRAME_ICRF, FRAME_OBSERVED, true, pos, pos);
    eraC2s(pos, &az, &alt);
    obj_set_attr((obj_t*)&obs, "pitch", alt);
    obj_set_attr((obj_t*)&obs, "yaw", az);
    observer_update(&obs, false);

    // Setup a projection and a painter.
    fov = 0.5 * DD2R;
    projection_init(&proj, PROJ_STEREOGRAPHIC, fov, 800, 600);
    painter = (painter_t) {
        .obs = &obs,
        .proj = &proj,
    };

    // Compute target healpix index at max order.
    order = 12;
    healpix_ang2pix(1 << order, M_PI / 2 - de, ra, &pix);
    // Check that all the healpix tiles are not clipped from the max order down
    // to the order zero.
    for (; order >= 0; order--) {
        r = painter_is_healpix_clipped(&painter, FRAME_ICRF, order, pix);
        if (r) LOG_E("Clipping error %d %d", order, pix);
        assert(!r);
        pix /= 4;
    }
}

static void test_iter_lines(void)
{
    const char *data, *line;
    int len;

    // Normal case.
    data = "AB\nCD\n";
    line = NULL;
    assert(iter_lines(data, strlen(data), &line, &len));
    assert(line[0] == 'A' && len == 2);
    assert(iter_lines(data, strlen(data), &line, &len));
    assert(line[0] == 'C' && len == 2);
    assert(!iter_lines(data, strlen(data), &line, &len));

    // No \n at the end of last line.
    data = "AB\nCD";
    line = NULL;
    assert(iter_lines(data, strlen(data), &line, &len));
    assert(line[0] == 'A' && len == 2);
    assert(iter_lines(data, strlen(data), &line, &len));
    assert(line[0] == 'C' && len == 2);
    assert(!iter_lines(data, strlen(data), &line, &len));

    // Not null terminated string.
    data = "AB\nCD\nX";
    line = NULL;
    assert(iter_lines(data, strlen(data) - 1, &line, &len));
    assert(line[0] == 'A' && len == 2);
    assert(iter_lines(data, strlen(data) - 1, &line, &len));
    assert(line[0] == 'C' && len == 2);
    assert(!iter_lines(data, strlen(data) - 1, &line, &len));
}

static void test_jcon(void)
{
    const char *str;
    json_value *json;
    int x, d_x, d_y, e_x = -1, l_0, r;

    #define STR(...) #__VA_ARGS__
    str = STR({"x": 1, "d": {"x": 10, "y": 20}, "l": [3, 4]});
    #undef STR
    json = json_parse(str, strlen(str));
    assert(json);

    r = jcon_parse(json, "{",
        "x", JCON_INT(x, 0),
        "l", "[", JCON_INT(l_0, 0), "]",
        "d", "{",
            "x", JCON_INT(d_x, 0),
            "y", JCON_INT(d_y, 0),
        "}",
        "?e", "{",
            "x", JCON_INT(e_x, 1),
        "}",
    "}");
    assert(r == 0 && x == 1 && d_x == 10 && d_y == 20 && e_x == 1 &&
           l_0 == 3);

    r = jcon_parse(json, "{",
        "e", "{",
            "x", JCON_INT(e_x, 1),
        "}",
    "}");
    assert(r != 0);

    json_value_free(json);
}

static void test_u8(void)
{
    char buf[8];
    assert(u8_char_len("a") == 1);
    assert(u8_char_len("é") == 2);
    assert(u8_char_len("你") == 3);
    u8_upper(buf, "Vénus", sizeof(buf));
    assert(strcmp(buf, "VÉNUS") == 0);
    u8_lower(buf, "Vénus", sizeof(buf));
    assert(strcmp(buf, "vénus") == 0);

    // Make sure we don't segfault on overflow.
    u8_upper(buf, "A long line", sizeof(buf));
    assert(strlen(buf) < sizeof(buf));
    u8_lower(buf, "A long line", sizeof(buf));
    assert(strlen(buf) < sizeof(buf));

    u8_remove_accents(buf, "Vénus", sizeof(buf));
    assert(strcmp(buf, "Venus") == 0);
}

TEST_REGISTER(NULL, test_ephemeris, TEST_AUTO);
TEST_REGISTER(NULL, test_epv00, TEST_AUTO);
TEST_REGISTER(NULL, test_clipping, TEST_AUTO);
TEST_REGISTER(NULL, test_iter_lines, TEST_AUTO);
TEST_REGISTER(NULL, test_jcon, TEST_AUTO);
TEST_REGISTER(NULL, test_u8, TEST_AUTO);

#endif

