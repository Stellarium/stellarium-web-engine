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

#define SATELLITE_DEFAULT_MAG 7.0
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
    char            name[16];
};

/*
 * Type: satellite_t
 * Represents an individual satellite.
 */
typedef struct satellite {
    obj_t obj;
    char name[26];
    char name2[26]; // Extra name.
    sgp4_elsetrec_t *elsetrec; // Orbit elements.
    int number;
    double stdmag; // Taken from the qsmag data.
    double pvg[3]; // XXX: rename that.
    double pvo[2][4];
    double vmag;
    bool error; // Set if we got an error computing the position.
    json_value *data; // Data passed in the constructor.
} satellite_t;

// Module class.
typedef struct satellites {
    obj_t   obj;
    char    *norad_url; // Online norad files location.
    qsmag_t *qsmags; // Hash table id -> qsmag entry.
    int     qsmags_status;
    char    *jsonl_url;   // jsonl file in noctuasky server format.
    bool    loaded;
    double  hints_mag_offset;
    bool    visible;
} satellites_t;


static int satellites_init(obj_t *obj, json_value *args)
{
    satellites_t *sats = (void*)obj;
    sats->visible = true;
    return 0;
}

static int satellites_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    satellites_t *sats = (void*)obj;
    if (strcmp(type, "norad") == 0)
        sats->norad_url = strdup(url);
    else if (strcmp(type, "jsonl/sat") == 0)
        sats->jsonl_url = strdup(url);
    else
        return 1;
    return 0;
}

/*
 * Parse a TLE sources file and add all the satellites.
 *
 * Return:
 *   The number of satellites parsed, or a negative number in case of error.
 */
static int parse_tle_file(satellites_t *sats, const char *data,
                          double *last_epoch)
{
    const char *line0, *line1, *line2;
    char id[16];
    int i, nb = 0, sat_num;
    satellite_t *sat;
    double startmfe, stopmfe, deltamin;
    qsmag_t *qsmag;

    *last_epoch = 0;
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

        snprintf(id, sizeof(id), "NORAD %.5s", line1 + 2);
        sat = (void*)module_add_new(&sats->obj, "tle_satellite", id, NULL);
        sat_num = atoi(line1 + 2);

        sat->obj.oid = oid_create("NORA", sat_num);
        sat->stdmag = NAN;
        strcpy(sat->obj.type, "Asa"); // Otype code.

        // If the sat is in the qsmag file, set its stdmag.
        sat->number = atoi(line2 + 2);
        // Copy and strip name.
        memcpy(sat->name, line0, 24);
        for (i = 23; sat->name[i] == ' '; i--) sat->name[i] = '\0';

        // Use qsmag values if available.
        HASH_FIND_INT(sats->qsmags, &sat->number, qsmag);
        if (qsmag) {
            if (qsmag->stdmag)
                sat->stdmag = qsmag->stdmag;
            if (*qsmag->name) {
                snprintf(sat->name2, sizeof(sat->name2), "%s", sat->name);
                snprintf(sat->name, sizeof(sat->name), "%s", qsmag->name);
            }
        }

        sat->elsetrec = sgp4_twoline2rv(
                line1, line2, 'c', 'm', 'i',
                &startmfe, &stopmfe, &deltamin);

        *last_epoch = max(*last_epoch, sgp4_get_satepoch(sat->elsetrec));
        nb++;
    }
    return nb;
error:
    LOG_E("Cannot parse TLE file");
    return nb;
}

static void strip_str(char *str)
{
    int i;
    for (i = strlen(str) - 1; i >= 0; i--) {
        if (str[i] != ' ') break;
        str[i] = '\0';
    }
}

static bool load_qsmag(satellites_t *sats)
{
    int size, id;
    float stdmag;
    const char *data, *line;
    char url[1024], name[16];
    qsmag_t *qsmag;

    if (sats->qsmags_status >= 200 && sats->qsmags_status < 400) return true;
    if (sats->qsmags_status) return false;
    if (!sats->norad_url) return false;

    snprintf(url, sizeof(url), "%s/%s", sats->norad_url, "qs.mag");
    data = asset_get_data2(url, ASSET_USED_ONCE, &size, &sats->qsmags_status);
    if (sats->qsmags_status && !data)
        LOG_E("Error while loading %s: %d", url, sats->qsmags_status);
    if (!data) return false;

    for (line = data; line; line = strchr(line, '\n')) {
        if (*line == '\n') line++;
        if (!(*line)) break;
        memset(name, 0, sizeof(name));
        if (sscanf(line, "%5d%*13c%14c %f", &id, name, &stdmag) < 2) {
            LOG_W("Incorrect qsmag line");
            continue;
        }
        if (id <= 1 || id >= 99999)
            continue;

        qsmag = calloc(1, sizeof(*qsmag));
        qsmag->id = id;
        qsmag->stdmag = stdmag;
        assert(sizeof(qsmag->name) == 16 && sizeof(name) == 16);
        memcpy(qsmag->name, name, 16);
        strip_str(qsmag->name);
        HASH_ADD_INT(sats->qsmags, id, qsmag);
    }
    return true;
}

static bool load_norad_data(satellites_t *sats)
{
    int size, code, nb;
    const char *data;
    double last_epoch;
    char url[1024];
    char buf[128];
    if (sats->loaded) return true;
    if (!sats->norad_url) return false;

    // Only visual for the moment.
    snprintf(url, sizeof(url), "%s/%s", sats->norad_url, "visual.txt");
    data = asset_get_data2(url, ASSET_USED_ONCE, &size, &code);
    if (!data && code) {
        LOG_E("Cannot load %s: %d", url, code);
        sats->loaded = true;
    }
    if (!data) return false;
    nb = parse_tle_file(sats, data, &last_epoch);
    LOG_D("Parsed %d satellites (latest epoch: %s)", nb,
          format_time(buf, last_epoch, 0, "YYYY-MM-DD"));
    sats->loaded = true;
    return true;
}

static int load_jsonl_data(satellites_t *sats, const char *data, int size,
                           const char *url, double *last_epoch)
{
    const char *line = NULL;
    int len, line_idx = 0, nb = 0;
    char *uncompressed_data;
    json_value *json;
    satellite_t *sat;

    // XXX: should use a more robust gz uncompression function for external
    // data.
    uncompressed_data = z_uncompress_gz(data, size, &size);
    if (!uncompressed_data) {
        LOG_E("Cannot uncompress gz file: %s", url);
        return -1;
    }

    data = uncompressed_data;

    *last_epoch = 0;
    while (iter_lines(data, size, &line, &len)) {
        line_idx++;
        json = json_parse(line, len);
        if (!json) goto error;
        sat = (void*)module_add_new(&sats->obj, "tle_satellite", NULL, json);
        json_value_free(json);
        if (!sat) goto error;
        *last_epoch = max(*last_epoch, sgp4_get_satepoch(sat->elsetrec));
        nb++;
        continue;
error:
        LOG_E("Cannot create sat from %s:%d", url, line_idx);
    }

    free(uncompressed_data);
    return nb;
}

static int satellites_update(obj_t *obj, double dt)
{
    PROFILE(satellites_update, 0);
    satellites_t *sats = (satellites_t*)obj;
    const char *data;
    double last_epoch = 0;
    int size, code, nb;
    char buf[128];

    if (sats->loaded) return 0;

    if (sats->jsonl_url) {
        data = asset_get_data2(sats->jsonl_url, ASSET_USED_ONCE, &size, &code);
        if (!code) return 0; // Sill loading.
        if (data) {
            nb = load_jsonl_data(sats, data, size, sats->jsonl_url,
                                 &last_epoch);
            LOG_I("Parsed %d satellites (latest epoch: %s)", nb,
                  format_time(buf, last_epoch, 0, "YYYY-MM-DD"));
        }

        sats->loaded = true;
    }

    if (sats->norad_url) {
        if (!load_qsmag(sats)) return 0;
        if (!load_norad_data(sats)) return 0;
    }

    return 0;
}

static int satellites_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(satellites_render, 0);
    satellites_t *sats = (void*)obj;
    obj_t *child;
    if (!sats->visible) return false;
    MODULE_ITER(obj, child, "tle_satellite")
        obj_render(child, painter);
    return 0;
}

static obj_t *satellites_get_by_oid(
        const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (!oid_is_catalog(oid, "NORA")) return NULL;
    MODULE_ITER(obj, child, "tle_satellite") {
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


    vec3_mul(-DAU, sat->pvg, e_pos);
    vec3_add(obs->earth_pvh[0], sat->pvg, s_pos);
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
    double illumination, fracil, elong, range;
    double observed[3];

    convert_frame(obs, FRAME_ICRF, FRAME_OBSERVED, false,
                        sat->pvo[0], observed);
    if (observed[2] < 0.0) return 99; // Below horizon.
    illumination = satellite_compute_earth_shadow(sat, obs);
    if (illumination == 0.0) {
        // Eclipsed.
        return 17.0;
    }
    if (isnan(sat->stdmag)) return SATELLITE_DEFAULT_MAG;

    // If we have a std mag value,
    // We use the formula:
    // mag = stdmag - 15.75 + 2.5 * log10 (range * range / fracil)
    // where : stdmag = standard magnitude as defined above
    //         range  = distance from observer to satellite, km
    //         fracil = fraction of satellite illuminated,
    //                  [ 0 <= fracil <= 1 ]
    // (https://www.prismnet.com/~mmccants/tles/mccdesc.html)

    range = vec3_norm(sat->pvo[0]) * DAU / 1000; // Distance in km.
    elong = eraSepp(obs->sun_pvo[0], sat->pvo[0]);
    fracil = 0.5 * (1. + cos(elong));
    return sat->stdmag - 15.75 + 2.5 * log10(range * range / fracil);
}

static int satellite_init(obj_t *obj, json_value *args)
{
    // Support creating a satellite using noctuasky model data json values.
    satellite_t *sat = (satellite_t*)obj;
    const char *tle1, *tle2, *name = NULL, *type = "Asa";
    double startmfe, stopmfe, deltamin;
    int r;

    sat->vmag = SATELLITE_DEFAULT_MAG;
    sat->stdmag = SATELLITE_DEFAULT_MAG;

    if (args) {
        r = jcon_parse(args, "{",
            "types", "[", JCON_STR(type), "]",
            "!model_data", "{",
                "!norad_number", JCON_INT(sat->number),
                "mag", JCON_DOUBLE(sat->stdmag),
                "!tle", "[", JCON_STR(tle1), JCON_STR(tle2), "]",
            "}",
            "short_name", JCON_STR(name),
        "}");
        if (r) {
            LOG_E("Cannot parse satellite json data");
            assert(false);
            return -1;
        }
        sat->obj.oid = oid_create("NORA", sat->number);
        sat->elsetrec = sgp4_twoline2rv(tle1, tle2, 'c', 'm', 'i',
                                        &startmfe, &stopmfe, &deltamin);
        if (name)
            snprintf(sat->name, sizeof(sat->name), "%s", name);
        strncpy(sat->obj.type, type, 4);

        sat->data = json_copy(args);
    }

    return 0;
}

static void satellite_del(obj_t *obj)
{
    satellite_t *sat = (satellite_t*)obj;
    free(sat->elsetrec);
    json_builder_free(sat->data);
}

/*
 * Transform a position from true equator to J2000 mean equator (ICRF).
 */
static void true_equator_to_j2000(const observer_t *obs,
                                  const double pv[2][3],
                                  double out[2][3])
{
    vec3_copy(pv[0], out[0]);
    vec3_copy(pv[1], out[1]);
    mat3_mul_vec3(obs->rnp, out[0], out[0]);
}

/*
 * Update an individual satellite.
 */
static int satellite_update(satellite_t *sat, const observer_t *obs)
{
    double pv[2][3];

    if (sat->error) return 0;
    assert(sat->elsetrec);
    // Orbit computation.
    if (!sgp4(sat->elsetrec, obs->utc, pv[0],  pv[1])) {
        LOG_W("Cannot compute satellite position (%s, %d)",
              sat->name, sat->number);
        sat->error = true;
        return 0;
    }
    assert(!isnan(pv[0][0]) && !isnan(pv[0][1]));

    vec3_mul(1000.0 / DAU, pv[0], pv[0]);
    vec3_mul(1000.0 / DAU, pv[1], pv[1]);
    true_equator_to_j2000(obs, pv, pv);

    vec3_copy(pv[0], sat->pvg);

    position_to_apparent(obs, ORIGIN_GEOCENTRIC, false, pv, pv);
    vec3_copy(pv[0], sat->pvo[0]);
    vec3_copy(pv[1], sat->pvo[1]);
    sat->pvo[0][3] = 1.0; // AU
    sat->pvo[1][3] = 0.0;

    sat->vmag = satellite_compute_vmag(sat, obs);
    return 0;
}

static int satellite_get_info(const obj_t *obj, const observer_t *obs, int info,
                              void *out)
{
    satellite_t *sat = (satellite_t*)obj;
    satellite_update(sat, obs);
    switch (info) {
    case INFO_PVO:
        memcpy(out, sat->pvo, sizeof(sat->pvo));
        return 0;
    case INFO_VMAG:
        *(double*)out = sat->vmag;
        return 0;
    }
    return 1;
}


/*
 * Render an individual satellite.
 */
static int satellite_render(const obj_t *obj, const painter_t *painter_)
{
    double vmag, size, luminance, p_win[4];
    painter_t painter = *painter_;
    point_t point;
    double color[4];
    const double label_color[4] = RGBA(124, 205, 124, 205);
    const double white[4] = RGBA(255, 255, 255, 255);
    satellite_t *sat = (satellite_t*)obj;
    const satellites_t *sats = (satellites_t*)obj->parent;
    const bool selected = core->selection && obj->oid == core->selection->oid;
    const double hints_limit_mag = painter.hints_limit_mag +
                                   sats->hints_mag_offset - 2.5;

    satellite_update(sat, painter.obs);
    vmag = sat->vmag;
    if (sat->error) return 0;
    if (vmag > painter.stars_limit_mag && vmag > hints_limit_mag) return 0;

    if (!painter_project(&painter, FRAME_ICRF, sat->pvo[0], false, true, p_win))
        return 0;

    core_get_point_for_mag(vmag, &size, &luminance);

    // Render symbol if needed.
    if (vmag <= hints_limit_mag) {
        vec4_copy(label_color, color);
        symbols_paint(&painter, SYMBOL_ARTIFICIAL_SATELLITE, p_win,
                      VEC(24.0, 24.0), color, 0.0);
    }

    point = (point_t) {
        .pos = {p_win[0], p_win[1]},
        .size = size,
        .color = {255, 255, 255, luminance * 255},
        .oid = obj->oid,
    };
    paint_2d_points(&painter, 1, &point);

    // Render name if needed.
    size = max(8, size);
    if (*sat->name && (selected || vmag <= hints_limit_mag - 1.5)) {
        labels_add_3d(sat->name, FRAME_ICRF, sat->pvo[0], false, size,
                      FONT_SIZE_BASE - 2, selected ? white : label_color, 0,
                      0, selected ? TEXT_BOLD : TEXT_FLOAT, 0, obj->oid);
    }

    return 0;
}

static void satellite_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user,
             const char *cat, const char *str))
{
    satellite_t *sat = (void*)obj;
    json_value *names;
    char *name;
    int i;
    char buf[32];
    if (sat->data) {
        names = json_get_attr(sat->data, "names", json_array);
        for (i = 0; i < names->u.array.length; ++i) {
            name = names->u.array.values[i]->u.string.ptr;
            if (strstr(name, "NAME "))
                f(obj, user, "NAME", name + 5);
            else {
                f(obj, user, "", name);
            }
        }
    } else {
        snprintf(buf, sizeof(buf), "%05d", (int)oid_get_index(obj->oid));
        if (*sat->name)
            f(obj, user, "NAME", sat->name);
        if (*sat->name2)
            f(obj, user, "NAME", sat->name2);
        f(obj, user, "NORAD", buf);
    }
}

static json_value *satellite_data_fn(obj_t *obj, const attribute_t *attr,
                                     const json_value *args)
{
    satellite_t *sat = (void*)obj;
    if (!args && sat->data) return json_copy(sat->data);
    return NULL;
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
    .del            = satellite_del,
    .get_info       = satellite_get_info,
    .render         = satellite_render,
    .get_designations = satellite_get_designations,
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = satellite_data_fn),
        {}
    },
};
OBJ_REGISTER(satellite_klass)

static obj_klass_t satellites_klass = {
    .id             = "satellites",
    .size           = sizeof(satellites_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE | OBJ_LISTABLE,
    .init           = satellites_init,
    .add_data_source = satellites_add_data_source,
    .render_order   = 30,
    .update         = satellites_update,
    .render         = satellites_render,
    .get_by_oid     = satellites_get_by_oid,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(satellites_t, visible)),
        PROPERTY(hints_mag_offset, TYPE_MAG,
                 MEMBER(satellites_t, hints_mag_offset)),
        {}
    }
};
OBJ_REGISTER(satellites_klass)
