/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "sgp4.h"

/*
 * Artificial satellites module
 */

/*
 * Type: satellite_t
 * Represents an individual satellite.
 */
typedef struct satellite {
    obj_t obj;
    char name[26];
    sgp4_elsetrec_t *elsetrec; // Orbit elements.
} satellite_t;

static int satellite_update(obj_t *obj, const observer_t *obs, double dt);
static int satellite_render(const obj_t *obj, const painter_t *painter);

static obj_klass_t satellite_klass = {
    .id             = "satellite",
    .size           = sizeof(satellite_t),
    .flags          = 0,
    .render_order   = 30,
    .update         = satellite_update,
    .render         = satellite_render,

    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        PROPERTY("ra"),
        PROPERTY("dec"),
        PROPERTY("distance"),
        PROPERTY("alt"),
        PROPERTY("az"),
        PROPERTY("radec"),
        PROPERTY("azalt"),
        PROPERTY("rise"),
        PROPERTY("set"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {}
    },
};

OBJ_REGISTER(satellite_klass)

// Module class.
typedef struct satellites {
    obj_t   obj;
    bool    loaded;
} satellites_t;

static int satellites_init(obj_t *obj, json_value *args);
static int satellites_update(obj_t *obj, const observer_t *obs, double dt);
static int satellites_render(const obj_t *obj, const painter_t *painter);

static obj_klass_t satellites_klass = {
    .id             = "satellites",
    .size           = sizeof(satellites_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = satellites_init,
    .render_order   = 30,
    .update         = satellites_update,
    .render         = satellites_render,
};

OBJ_REGISTER(satellites_klass)

static int satellites_init(obj_t *obj, json_value *args)
{
    return 0;
}

/*
 * Parse a TLE sources file and add all the satellites.
 *
 * Return:
 *   The number of satellites parsed, or a negative number in case of error.
 */
static int parse_tle_file(satellites_t *sats, const char *data)
{
    const char *line1, *line2, *line3;
    char id[16];
    int i, nb = 0;
    satellite_t *sat;
    double startmfe, stopmfe, deltamin;

    while (*data) {
        line1 = data;
        line2 = strchr(line1, '\n') + 1;
        line3 = strchr(line2, '\n') + 1;
        data  = strchr(line3, '\n') + 1;

        sprintf(id, "SAT %.6s", line2);
        sat = (satellite_t*)obj_create("satellite", id, (obj_t*)sats, NULL);

        // Copy and strip name.
        memcpy(sat->name, line1, 24);
        for (i = 23; sat->name[i] == ' '; i--) sat->name[i] = '\0';

        sat->elsetrec = sgp4_twoline2rv(
                line2, line3, 'c', 'm', 'i',
                &startmfe, &stopmfe, &deltamin);

        nb++;
    }
    return nb;
}

static bool load_data(satellites_t *sats)
{
    int size, code, nb;
    char *data;
    const char *URL = "https://data.stellarium.org/norad/visual.txt";
    if (sats->loaded) return true;

    // First try from cache.
    data = sys_storage_load("satellites", "visual.txt", &size, &code);
    if (data) {
        sats->loaded = true;
        nb = parse_tle_file(sats, data);
        LOG_D("Parsed %d satellites", nb);
        free(data);
        return true;
    }

    // Not in cache yet, load it online.
    data = (char*)asset_get_data(URL, &size, &code);
    if (!data) return false;
    sys_storage_store("satellites", "visual.txt", data, size);
    return false;
}

static int satellites_update(obj_t *obj, const observer_t *obs, double dt)
{
    satellites_t *sats = (satellites_t*)obj;
    satellite_t *sat;
    if (!load_data(sats)) return 0;

    OBJ_ITER(sats, sat, &satellite_klass) {
        obj_update((obj_t*)sat, obs, dt);
    }

    return 0;
}

static int satellites_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, &satellite_klass)
        obj_render(child, painter);
    return 0;
}

/*
 * Update an individual satellite.
 */
static int satellite_update(obj_t *obj, const observer_t *obs, double dt)
{
    double p[3], v[3];
    satellite_t *sat = (satellite_t*)obj;
    sgp4(sat->elsetrec, obs->tt, p,  v); // Orbit computation.

    vec3_mul(1000.0 / DAU, p, p);
    vec3_mul(1000.0 / DAU, v, v);

    vec3_copy(p, obj->pos.pvg[0]);
    vec3_copy(v, obj->pos.pvg[1]);
    obj->pos.unit = 1.0; // AU.

    // For the moment we have no information about the magnitude.
    obj->vmag = 7.0;

    // XXX: We need to get ride of this!
    compute_coordinates(obs, obj->pos.pvg, obj->pos.unit,
                        &obj->pos.ra, &obj->pos.dec,
                        NULL, NULL, NULL, NULL,
                        &obj->pos.az, &obj->pos.alt);
    return 0;
}

/*
 * Render an individual satellite.
 */
static int satellite_render(const obj_t *obj, const painter_t *painter_)
{
    double mag, size, luminance, p[4] = {0, 0, 0, 1}, p_ndc[4];
    painter_t painter = *painter_;
    point_t point;
    double color[3] = {1, 1, 1};
    double label_color[4] = RGBA(124, 255, 124, 255);
    satellite_t *sat = (satellite_t*)obj;

    if (obj->vmag > painter.mag_max) return 0;
    vec3_copy(obj->pos.pvg[0], p);
    convert_coordinates(core->observer, FRAME_ICRS, FRAME_VIEW, 0, p, p);

    // Skip if not visible.
    if (!project(painter.proj, PROJ_TO_NDC_SPACE, 2, p, p_ndc)) return 0;
    mag = core_get_observed_mag(obj->vmag);
    core_get_point_for_mag(mag, &size, &luminance);
    point = (point_t) {
        .pos = {p[0], p[1], p[2], p[3]},
        .size = size,
        .color = {color[0], color[1], color[2], luminance},
    };
    strcpy(point.id, obj->id);
    paint_points(&painter, 1, &point, FRAME_VIEW);

    // Render name if needed.
    if (*sat->name && obj->vmag <= painter.label_mag_max) {
        if (project(painter.proj, PROJ_TO_NDC_SPACE, 2, p, p)) {
            labels_add(sat->name, p, size, 13, label_color, ANCHOR_AROUND, 0);
        }
    }

    return 0;
}
