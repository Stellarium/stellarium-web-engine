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
 * Type: satellite_t
 * Represents an individual satellite.
 */
typedef struct satellite satellite_t;
struct satellite {
    obj_t obj;
    char name[26];
    sgp4_elsetrec_t *elsetrec; // Orbit elements.
    int number;
    double stdmag;
    double pvg[3]; // XXX: rename that.
    double pvo[2][4];
    double vmag;
    bool error; // Set if we got an error computing the position.
    json_value *data; // Data passed in the constructor.

    // Linked list of currently visible on screen.
    satellite_t *visible_next, *visible_prev;
};

// Module class.
typedef struct satellites {
    obj_t   obj;
    char    *norad_url; // Online norad files location.
    char    *jsonl_url;   // jsonl file in noctuasky server format.
    bool    loaded;
    int     update_pos; // Index of the position for iterative update.
    bool    visible;
    double  hints_mag_offset;
    bool    hints_visible;

    satellite_t *render_current;
    satellite_t *visibles; // Linked list of currently visible satellites.
} satellites_t;

// Static instance.
static satellites_t *g_satellites = NULL;

static int satellites_init(obj_t *obj, json_value *args)
{
    satellites_t *sats = (void*)obj;
    assert(!g_satellites);
    g_satellites = sats;
    sats->visible = true;
    sats->hints_visible = true;
    return 0;
}

static int satellites_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    satellites_t *sats = (void*)obj;
    if (strcmp(key, "jsonl/sat") != 0)
        return -1;
    sats->jsonl_url = strdup(url);
    return 0;
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

    return 0;
}

static void add_to_visible(satellites_t *sats, satellite_t *sat)
{
    if (sat->visible_prev) return;
    DL_APPEND2(sats->visibles, sat, visible_prev, visible_next);
}

static int satellite_render(const obj_t *obj, const painter_t *painter);

static int satellites_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(satellites_render, 0);

    satellites_t *sats = (void*)obj;
    int i, r;
    const int update_nb = 32;
    satellite_t *child, *tmp;

    if (!sats->visible) return false;

    // If the current selection is a satellite, make sure it is flagged
    // as visible.
    if (core->selection && core->selection->parent == obj) {
        add_to_visible(sats, (void*)core->selection);
    }

    // Render all the flagged visible satellites, remove those that are
    // no longer visible.
    DL_FOREACH_SAFE2(sats->visibles, child, tmp, visible_next) {
        r = satellite_render(&child->obj, painter);
        if (r == 0 && &child->obj != core->selection) {
            DL_DELETE2(sats->visibles, child, visible_prev, visible_next);
            child->visible_prev = NULL;
        }
    }

    // Then iter part of the full list as well.
    for (   i = 0, child = sats->render_current ?: (void*)sats->obj.children;
            child && i < update_nb;
            i++, child = (void*)child->obj.next) {
        if (child->visible_prev) continue; // Was already rendered.
        r = satellite_render(&child->obj, painter);
        if (r == 1) add_to_visible(sats, child);
    }
    sats->render_current = child;
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
    double illumination, fracil, phase_angle, range;
    double observed[3];
    double ph[3];

    convert_frame(obs, FRAME_ICRF, FRAME_OBSERVED, false,
                        sat->pvo[0], observed);
    if (observed[2] < 0.0) return 99; // Below horizon.
    illumination = satellite_compute_earth_shadow(sat, obs);
    if (illumination == 0.0) {
        // Eclipsed.
        return 17.0;
    }
    if (isnan(sat->stdmag)) return SATELLITE_DEFAULT_MAG;

    vec3_sub(sat->pvo[0], obs->sun_pvo[0], ph);
    phase_angle = eraSepp(sat->pvo[0], ph);
    fracil = 0.5 * cos(phase_angle) + 0.5;
    range = vec3_norm(sat->pvo[0]) * DAU / 1000; // Distance in km.

    // If we have a std mag value,
    // We use the formula:
    // mag = stdmag - 15.75 + 2.5 * log10 (range * range / fracil)
    // where : stdmag = standard magnitude as defined above
    //         range  = distance from observer to satellite, km
    //         fracil = fraction of satellite illuminated,
    //                  [ 0 <= fracil <= 1 ]
    // (https://www.prismnet.com/~mmccants/tles/mccdesc.html)

    return sat->stdmag - 15.75 + 2.5 * log10(range * range / fracil);
}

static int satellite_init(obj_t *obj, json_value *args)
{
    // Support creating a satellite using noctuasky model data json values.
    satellite_t *sat = (satellite_t*)obj;
    const char *tle1, *tle2, *name = NULL, *type = NULL;
    double startmfe, stopmfe, deltamin;
    int r;

    sat->vmag = SATELLITE_DEFAULT_MAG;
    sat->stdmag = SATELLITE_DEFAULT_MAG;

    if (args) {
        r = jcon_parse(args, "{",
            "?types", "[", JCON_STR(type), "]",
            "model_data", "{",
                "norad_number", JCON_INT(sat->number, 0),
                "?mag", JCON_DOUBLE(sat->stdmag, SATELLITE_DEFAULT_MAG),
                "tle", "[", JCON_STR(tle1), JCON_STR(tle2), "]",
            "}",
            "?names", "[", JCON_STR(name), "]",
        "}");
        if (r) {
            LOG_E("Cannot parse satellite json data");
            assert(false);
            return -1;
        }
        sat->obj.oid = oid_create("NORA", sat->number);
        sat->elsetrec = sgp4_twoline2rv(tle1, tle2, 'c', 'm', 'i',
                                        &startmfe, &stopmfe, &deltamin);
        if (name && strncmp(name, "NAME ", 5) == 0)
            snprintf(sat->name, sizeof(sat->name), "%s", name + 5);
        strncpy(sat->obj.type, type ?: "Asa", 4);

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
 * Note: return 1 if the satellite is actually visible on screen.
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
    const bool selected = core->selection && obj->oid == core->selection->oid;
    const double hints_limit_mag = painter.hints_limit_mag +
                                   g_satellites->hints_mag_offset - 2.5;

    satellite_update(sat, painter.obs);
    vmag = sat->vmag;
    if (sat->error) return 0;
    if (!selected && vmag > painter.stars_limit_mag && vmag > hints_limit_mag)
        return 0;

    if (!painter_project(&painter, FRAME_ICRF, sat->pvo[0], false, true, p_win))
        return 0;

    core_get_point_for_mag(vmag, &size, &luminance);

    // Render symbol if needed.
    if (g_satellites->hints_visible && (selected || vmag <= hints_limit_mag)) {
        vec4_copy(label_color, color);
        symbols_paint(&painter, SYMBOL_ARTIFICIAL_SATELLITE, p_win,
                      VEC(24.0, 24.0), selected ? white : color, 0.0);
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
    if (g_satellites->hints_visible && *sat->name &&
        (selected || vmag <= hints_limit_mag - 1.5)) {
        labels_add_3d(sat->name, FRAME_ICRF, sat->pvo[0], false, size + 1,
                      FONT_SIZE_BASE - 3, selected ? white : label_color, 0,
                      0, selected ? TEXT_BOLD : TEXT_FLOAT, 0, obj->oid);
    }

    return 1;
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
            f(obj, user, NULL, name);
        }
    } else {
        snprintf(buf, sizeof(buf), "%05d", (int)oid_get_index(obj->oid));
        if (*sat->name)
            f(obj, user, "NAME", sat->name);
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

static int satellites_list(const obj_t *obj, observer_t *obs,
                           double max_mag, uint64_t hint,
                           const char *sources, void *user,
                           int (*f)(void *user, obj_t *obj))
{
    obj_t *child;
    satellite_t *sat;
    double vmag;
    bool test_vmag = !isnan(max_mag);

    DL_FOREACH(obj->children, child) {
        sat = (void*)child;
        if (sat->error) continue;
        if (test_vmag && obj_get_info(child, obs, INFO_VMAG, &vmag) == 0 &&
                vmag > max_mag)
            continue;
        if (f && f(user, child)) break;
    }
    return 0;
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
    .list           = satellites_list,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(satellites_t, visible)),
        PROPERTY(hints_mag_offset, TYPE_FLOAT,
                 MEMBER(satellites_t, hints_mag_offset)),
        PROPERTY(hints_visible, TYPE_BOOL, MEMBER(satellites_t, hints_visible)),
        {}
    }
};
OBJ_REGISTER(satellites_klass)

#ifdef COMPILE_TESTS

static void check_sat(int norad_number, const char *tle1, const char *tle2,
                      double stdmag,
                      int iy, int im, int id, int h, int m, double s,
                      double ha_alt, double ha_az, double ha_dist,
                      double ha_vmag)
{
    observer_t obs;
    char json[1204];
    obj_t *obj;
    double d1, d2, vmag, dist, pos[4], alt, az;

    snprintf(json, sizeof(json),
             "{\"model_data\":{\"mag\": %f,"
             "\"norad_number\": %d,"
             "\"tle\": [\"%s\",\"%s\"]}}",
             stdmag, norad_number, tle1, tle2);
    obj = obj_create_str("tle_satellite", NULL, json);
    assert(obj);

    obs = *core->observer;
    obs.elong = 121.5654 * DD2R;
    obs.phi = 25.0330 * DD2R;
    eraDtf2d("UTC", iy, im, id, h - 8, m, s, &d1, &d2);
    obj_set_attr((obj_t*)&obs, "utc", d1 - DJM0 + d2);
    observer_update(&obs, false);

    obj_get_pos(obj, &obs, FRAME_OBSERVED, pos);
    eraC2s(pos, &az, &alt);
    az = eraAnp(az);
    obj_get_info(obj, &obs, INFO_DISTANCE, &dist);
    obj_get_info(obj, &obs, INFO_VMAG, &vmag);
    assert(fabs(alt * DR2D - ha_alt) < 1);
    assert(fabs(az * DR2D - ha_az) < 2);
    assert(fabs(dist * DAU / 1000 - ha_dist) < 1);
    assert(fabs(ha_vmag - vmag) < 1);
}

static void test_satellites(void)
{
    // Compare some position/vmag with values from heavens above.

    check_sat(20625,
        "1 20625U 90046B   20114.21029927  .00000256  00000-0  15749-3 0  9996",
        "2 20625  70.9963 124.1539 0015477 319.2677  40.7287 14.14651825545059",
        2.7,
        2020, 4, 23, 19, 59, 39,
        14, 298, 2167, 5.4);

    check_sat(13552, // Cosmos 1408
        "1 13552U 82092A   20113.74575265  .00000921  00000-0  32526-4 0  9999",
        "2 13552  82.5670 333.7928 0017326 336.3418  23.7022 15.27738669 55547",
        4.2,
        2020, 4, 24, 19, 26, 37,
        62, 84, 557, 2.5);

    check_sat(43563, // FALCON 9 R/B
        "1 43563U 18059B   20113.32331434  .00030378  00000-0  12213-2 0  9996",
        "2 43563  27.0685   0.0377 5648508  60.6275 343.9164  4.65462966 29379",
        2.4,
        2020, 4, 23, 17, 59, 13,
        80, 193, 1601, 3.5);
}

TEST_REGISTER(NULL, test_satellites, TEST_AUTO);

#endif // COMPILE_TESTS
