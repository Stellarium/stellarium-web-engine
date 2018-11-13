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
 * Type: qsmag_t
 * Represent an entry in the qs.mag file, that contains some extra
 * information about visible satellites.  For the moment we only use it
 * for the vmag.
 */
typedef struct qsmag qsmag_t;
struct qsmag {
    UT_hash_handle  hh;    // For the global map.
    int             id;
    double          stdmag;
};

/*
 * Type: satellite_t
 * Represents an individual satellite.
 */
typedef struct satellite {
    obj_t obj;
    char name[26];
    sgp4_elsetrec_t *elsetrec; // Orbit elements.
    int number;
    double stdmag; // Taken from the qsmag data.
} satellite_t;

// Module class.
typedef struct satellites {
    obj_t   obj;
    qsmag_t *qsmags; // Hash table id -> qsmag entry.
    int     qsmags_status;
    bool    loaded;
} satellites_t;


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
    const char *line0, *line1, *line2;
    char id[16];
    int i, nb = 0, sat_num;
    satellite_t *sat;
    double startmfe, stopmfe, deltamin;
    qsmag_t *qsmag;

    while (*data) {
        line0 = data;
        line1 = strchr(line0, '\n');
        if (!line1) goto error;
        line1 += 1;
        line2 = strchr(line1, '\n');
        if (!line2) goto error;
        line2 += 1;
        data  = strchr(line2, '\n');
        if (!data) break;
        data += 1;

        sprintf(id, "NORAD %.5s", line1 + 2);
        sat = (satellite_t*)obj_create("tle_satellite", id, (obj_t*)sats, NULL);
        sat_num = atoi(line1 + 2);
        sat->obj.oid = oid_create("NORA", sat_num);
        sat->stdmag = NAN;
        strcpy(sat->obj.type, "Asa"); // Otype code.

        // If the sat is in the qsmag file, set its stdmag.
        sat->number = atoi(line2 + 2);
        HASH_FIND_INT(sats->qsmags, &sat->number, qsmag);
        if (qsmag) sat->stdmag = qsmag->stdmag;

        // Copy and strip name.
        memcpy(sat->name, line0, 24);
        for (i = 23; sat->name[i] == ' '; i--) sat->name[i] = '\0';

        sat->elsetrec = sgp4_twoline2rv(
                line1, line2, 'c', 'm', 'i',
                &startmfe, &stopmfe, &deltamin);

        // Register the name in the global ids db.
        identifiers_add("NAME", sat->name, sat->obj.oid, 0, "Asa ",
                        sat->stdmag, NULL, NULL);
        nb++;
    }
    return nb;
error:
    LOG_E("Cannot parse TLE file");
    return nb;
}

static bool load_qsmag(satellites_t *sats)
{
    int size, id;
    double stdmag;
    const char *comp_data;
    const char *URL = "https://data.stellarium.org/norad/qs.mag.gz";
    char *data, *line;
    qsmag_t *qsmag;

    if (sats->qsmags_status < 400) return true;
    if (sats->qsmags_status) return false;
    comp_data = asset_get_data(URL, &size, &sats->qsmags_status);
    if (sats->qsmags_status && !comp_data)
        LOG_E("Error while loading qs.mag: %d", sats->qsmags_status);
    if (!comp_data) return false;

    // Uncompress and parse the data.
    data = z_uncompress_gz(comp_data, size, &size);
    for (line = data; line; line = strchr(line, '\n')) {
        if (*line == '\n') line++;
        if (!(*line)) break;
        if (strlen(line) < 34 || line[34] < '0' || line[34] > '9') continue;
        id = atoi(line);
        stdmag = atof(line + 33);
        qsmag = calloc(1, sizeof(*qsmag));
        qsmag->id = id;
        qsmag->stdmag = stdmag;
        HASH_ADD_INT(sats->qsmags, id, qsmag);
    }
    free(data);
    return true;
}

static bool load_data(satellites_t *sats)
{
    int size, code, nb;
    const char *data;
    const char *URL = "https://data.stellarium.org/norad/visual.txt";
    if (sats->loaded) return true;
    data = asset_get_data(URL, &size, &code);
    if (!data) return false;
    nb = parse_tle_file(sats, data);
    LOG_D("Parsed %d satellites", nb);
    sats->loaded = true;
    return true;
}

static int satellites_update(obj_t *obj, const observer_t *obs, double dt)
{
    satellites_t *sats = (satellites_t*)obj;
    satellite_t *sat;
    if (!load_qsmag(sats)) return 0;
    if (!load_data(sats)) return 0;

    OBJ_ITER(sats, sat, "tle_satellite") {
        obj_update((obj_t*)sat, obs, dt);
    }

    return 0;
}

static int satellites_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *child;
    OBJ_ITER(obj, child, "tle_satellite")
        obj_render(child, painter);
    return 0;
}

static obj_t *satellites_get_by_oid(
        const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (!oid_is_catalog(oid, "NORA")) return NULL;
    OBJ_ITER(obj, child, "tle_satellite") {
        if (child->oid == oid) {
            child->ref++;
            return child;
        }
    }
    return NULL;
}

/*
 * Compute the amount of light the satellite receives from the Sun, taking
 * into account the Earth shadow.  Return a value from 0 (totally eclipsed)
 * to 1 (totally illuminated).
 */
static double satellite_compute_earth_shadow(const satellite_t *sat,
                                             const observer_t *obs)
{
    double e_pos[3]; // Earth position from sat.
    double s_pos[3]; // Sun position from sat.
    double e_r, s_r;
    double elong;
    const double SUN_RADIUS = 695508000; // (m).
    const double EARTH_RADIUS = 6371000; // (m).

    vec3_mul(-DAU, sat->obj.pvg[0], e_pos);
    vec3_add(obs->earth_pvh[0], sat->obj.pvg[0], s_pos);
    vec3_mul(-DAU, s_pos, s_pos);
    elong = eraSepp(e_pos, s_pos);
    e_r = asin(EARTH_RADIUS / vec3_norm(e_pos));
    s_r = asin(SUN_RADIUS / vec3_norm(s_pos));

    // XXX: for the moment we don't consider the different kind of shadows.
    if (vec3_norm(s_pos) < vec3_norm(e_pos)) return 1.0;
    if (e_r + s_r < elong) return 1.0; // No eclipse.
    return 0.0;
}

static double satellite_compute_vmag(const satellite_t *sat,
                                     const observer_t *obs)
{
    double illumination, fracil, elong, sun_pos[4], sat_pos[4], range;
    double observed[4];

    convert_coordinates(obs, FRAME_ICRS, FRAME_OBSERVED, 0,
                        sat->obj.pvg[0], observed);
    if (observed[2] < 0.0) return 99; // Below horizon.
    illumination = satellite_compute_earth_shadow(sat, obs);
    if (illumination == 0.0) {
        // Eclipsed.
        return 17.0;
    }
    if (isnan(sat->stdmag)) return sat->obj.vmag; // Default value.

    // If we have a std mag value,
    // We use the formula:
    // mag = stdmag - 15.75 + 2.5 * log10 (range * range / fracil)
    // where : stdmag = standard magnitude as defined above
    //         range  = distance from observer to satellite, km
    //         fracil = fraction of satellite illuminated,
    //                  [ 0 <= fracil <= 1 ]
    // (https://www.prismnet.com/~mmccants/tles/mccdesc.html)

    // Sun position from earth, CIRS.
    vec3_mul(-1, obs->earth_pvh[0], sun_pos);
    sun_pos[3] = 1.0;
    convert_coordinates(obs, FRAME_ICRS, FRAME_ICRS, 0, sun_pos, sun_pos);

    vec4_copy(sat->obj.pvg[0], sat_pos);
    convert_coordinates(obs, FRAME_ICRS, FRAME_ICRS, 0, sat_pos, sat_pos);

    range = vec3_norm(sat_pos) * DAU / 1000; // Distance in km.
    elong = eraSepp(sun_pos, sat_pos);
    fracil = 0.5 * (1. + cos(elong));
    return sat->stdmag - 15.75 + 2.5 * log10(range * range / fracil);
}

static int satellite_init(obj_t *obj, json_value *args)
{
    // Support creating a dso using noctuasky model data json values.
    satellite_t *sat = (satellite_t*)obj;
    json_value *model, *tle;
    const char *tle1, *tle2, *name;
    double startmfe, stopmfe, deltamin;
    int norad_num = 0;

    sat->obj.vmag = 7.0; // Default value.
    model = json_get_attr(args, "model_data", json_object);
    if (model) {
        norad_num = json_get_attr_i(model, "norad_num", 0);
        if ((tle = json_get_attr(model, "tle", json_array))) {
            tle1 = tle->u.array.values[0]->u.string.ptr;
            tle2 = tle->u.array.values[1]->u.string.ptr;
            sat->elsetrec = sgp4_twoline2rv(
                    tle1, tle2, 'c', 'm', 'i',
                    &startmfe, &stopmfe, &deltamin);
            if (!norad_num) norad_num = atoi(tle1 + 2);
        }
        sat->stdmag = json_get_attr_f(model, "mag", NAN);
    }
    sat->obj.oid = oid_create("NORA", norad_num);
    if ((name = json_get_attr_s(args, "short_name")))
        snprintf(sat->name, sizeof(sat->name), "%s", name);
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

    vec3_copy(p, obj->pvg[0]);
    vec3_copy(v, obj->pvg[1]);
    obj->pvg[0][3] = obj->pvg[1][3] = 1.0; // AU.

    sat->obj.vmag = satellite_compute_vmag(sat, obs);
    return 0;
}

/*
 * Render an individual satellite.
 */
static int satellite_render(const obj_t *obj, const painter_t *painter_)
{
    double mag, size, luminance, p[4] = {0, 0, 0, 1}, p_win[4];
    painter_t painter = *painter_;
    point_t point;
    double color[3] = {1, 1, 1};
    double label_color[4] = RGBA(124, 255, 124, 255);
    satellite_t *sat = (satellite_t*)obj;

    if (obj->vmag > painter.mag_max) return 0;
    vec3_copy(obj->pvg[0], p);
    convert_coordinates(core->observer, FRAME_ICRS, FRAME_VIEW, 0, p, p);

    // Skip if not visible.
    if (!project(painter.proj, PROJ_TO_WINDOW_SPACE, 2, p, p_win)) return 0;
    mag = core_get_observed_mag(obj->vmag);
    core_get_point_for_mag(mag, &size, &luminance);

    // Render symbol if needed.  For the moment we always do it if the
    // mag is lower than 7.  We probably need an option to control that.
    if (mag < 7.0) {
        symbols_paint(&painter, SYMBOL_ARTIFICIAL_SATELLITE, p_win, 12.0,
                      label_color, 0.0);
        // Still render an invisible point for the selection.
        // XXX: should be done in symbols_paint!
        luminance = 0;
    }

    point = (point_t) {
        .pos = {p_win[0], p_win[1]},
        .size = size,
        .color = {color[0], color[1], color[2], luminance},
        .oid = obj->oid,
    };
    paint_points(&painter, 1, &point, FRAME_WINDOW);

    // Render name if needed.
    if (*sat->name && obj->vmag <= painter.label_mag_max) {
        labels_add(sat->name, p_win, size, 13, label_color, 0,
                   ANCHOR_AROUND, 0);
    }

    return 0;
}

void satellite_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user,
             const char *cat, const char *str))
{
    char buf[32];
    sprintf(buf, "%05d", (int)(obj->oid));
    f(obj, user, "NORAD", buf);
}

/*
 * Meta class declarations.
 */

static obj_klass_t satellite_klass = {
    .id             = "tle_satellite",
    .size           = sizeof(satellite_t),
    .flags          = 0,
    .render_order   = 30,
    .init           = satellite_init,
    .update         = satellite_update,
    .render         = satellite_render,
    .get_designations = satellite_get_designations,

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
        PROPERTY("vmag"),
        PROPERTY("type"),
        {}
    },
};
OBJ_REGISTER(satellite_klass)

static obj_klass_t satellites_klass = {
    .id             = "satellites",
    .size           = sizeof(satellites_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = satellites_init,
    .render_order   = 30,
    .update         = satellites_update,
    .render         = satellites_render,
    .get_by_oid     = satellites_get_by_oid,
};
OBJ_REGISTER(satellites_klass)
