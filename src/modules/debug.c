/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Debug module.  This just adds a menu in the GUI to do run some testing
 * scripts.  Not compiled in release.
 */

#include "swe.h"

#if DEBUG

static void debug_gui(obj_t *obj, int location);
static obj_klass_t debug_klass = {
    .id = "debug",
    .size = sizeof(obj_t),
    .flags = OBJ_MODULE,
    .gui = debug_gui,
};
OBJ_REGISTER(debug_klass)

// A list of event + target that we can jump to.
typedef struct {
    const char *name;
    const char *date;
    const char *location;
    const char *target;
} target_t;

target_t TARGETS[] = {
    {.name = "Lunar eclipse Taipei",
     .date = "2018-07-28 04:21:00 UTC+08",
     .location = "25.03°N 121.57°E",
     .target = "Moon"},
    {.name = "Europa shadow on Jupiter",
     .date = "2018-07-19 21:46:00 UTC+08",
     .location = "25.03°N 121.57°E",
     .target = "Jupiter"},
    // Taken from:
    //    https://www.universetoday.com/
    //    wp-content/uploads/2014/11/gany-io20090816.gif
    {.name = "Io shadow on Ganymede",
     .date = "2009-08-16 16:44:00 UTC+00",
     .location = "10.32°N 123.75°E", // Cebu, Philippines.
     .target = "Ganymede"},
    {.name = "2017 August 21 Solar eclipse",
     .date = "2017-08-21 18:30:00 UTC+00",
     .location = "36.17°N 86.78°W", // Nashville, USA.
     .target = "Moon"},
};

static int parse_date(const char *str, double *utc)
{
    int iy, im, id, ihr, imn, sec, ofs;
    double d1, d2;
    sscanf(str, "%d-%d-%d %d:%d:%d UTC+%d",
           &iy, &im, &id, &ihr, &imn, &sec, &ofs);
    eraDtf2d("UTC", iy, im, id, ihr, imn, sec, &d1, &d2);
    *utc = d1 - DJM0 + d2 - ofs / 24.0;
    return 0;
}

static int parse_location(const char *str, double *lon, double *lat)
{
    double lon_d, lat_d;
    char lon_c, lat_c;
    sscanf(str, "%lf°%c %lf°%c", &lat_d, &lat_c, &lon_d, &lon_c);
    if (lat_c == 'S') lat_d = -lat_d;
    if (lon_c == 'W') lon_d = -lon_d;
    *lon = lon_d * DD2R;
    *lat = lat_d * DD2R;
    return 0;
}

static void show_target(const target_t *t)
{
    double utc, lon, lat;
    if (!DEFINED(SWE_GUI)) return;
    if (!gui_button(t->name, 0)) return;
    LOG_D("Jump to target: %s", t->name);
    parse_date(t->date, &utc);
    parse_location(t->location, &lon, &lat);
    obj_set_attr((obj_t*)core->observer, "utc", utc);
    obj_set_attr((obj_t*)core->observer, "longitude", lon);
    obj_set_attr((obj_t*)core->observer, "latitude", lat);
}

static void debug_gui(obj_t *obj, int location)
{
    int i;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Tests")) {
        for (i = 0; i < ARRAY_SIZE(TARGETS); i++)
            show_target(&TARGETS[i]);
        gui_tab_end();
    }
}

#endif
