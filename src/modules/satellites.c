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
#include "designation.h"

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
    sgp4_elsetrec_t *elsetrec; // Orbit elements.
    int number;
    double stdmag;
    double pvg[2][3];
    double pvo[2][3];
    double vmag;
    const char *model;

    // Launch and decay dates in UTC MJD.  Zero if not known.
    double launch_date;
    double decay_date;

    bool error; // Set if we got an error computing the position.
    json_value *data; // Data passed in the constructor.
    double max_brightness; // Cached max_brightness value.

    // Linked list of currently visible on screen.
    satellite_t *visible_next, *visible_prev;
};

// Module class.
typedef struct satellites {
    obj_t   obj;
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

static double max3(double x, double y, double z)
{
    return max(x, max(y, z));
}

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
        sat = (void*)module_add_new(&sats->obj, "tle_satellite", json);
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
    satellites_t *sats = (satellites_t*)obj;
    const char *data;
    double last_epoch = 0;
    int size, code, nb;
    char buf[128];

    if (sats->loaded) return 0;
    if (!sats->jsonl_url) return 0;

    data = asset_get_data2(sats->jsonl_url, ASSET_USED_ONCE, &size, &code);
    if (!code) return 0; // Sill loading.
    if (!data) return 0; // Got error;
    nb = load_jsonl_data(sats, data, size, sats->jsonl_url, &last_epoch);
    LOG_I("Parsed %d satellites (latest epoch: %s)", nb,
          format_time(buf, last_epoch, 0, "YYYY-MM-DD"));
    if (last_epoch < unix_to_mjd(sys_get_unix_time()) - 2)
        LOG_W("Warning: satellites data seems outdated.");
    sats->loaded = true;
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


    vec3_mul(-DAU2M, sat->pvg[0], e_pos);
    vec3_add(obs->earth_pvh[0], sat->pvg[0], s_pos);
    vec3_mul(-DAU2M, s_pos, s_pos);
    elong = eraSepp(e_pos, s_pos);
    e_r = asin(EARTH_RADIUS / vec3_norm(e_pos));
    s_r = asin(SUN_RADIUS / vec3_norm(s_pos));

    // XXX: for the moment we don't consider the different kind of shadows.
    if (vec3_norm(s_pos) < vec3_norm(e_pos)) return 1.0;
    if (e_r + s_r < elong) return 1.0; // No eclipse.
    return 0.0;
}

static double compute_max_brightness(
        const sgp4_elsetrec_t *elsetrec, double stdmag)
{
    double perigree;
    perigree = sgp4_get_perigree_height(elsetrec);
    return stdmag - 15.75 + 2.5 * log10(perigree * perigree);
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
    range = vec3_norm(sat->pvo[0]) * DAU2M / 1000; // Distance in km.

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

/*
 * Compute the otype from a json list of otype
 * Parameters:
 *   val    - A json value, anything but an array of string is an error.
 *   base   - The base otype value.  This is used as a fallback in case of
 *            error or if we don't find any known otype that is a child of
 *            base.
 */
static const char *otype_from_json(const json_value *val, const char *base)
{
    int i;
    const char *ret = NULL;
    if (!val || val->type != json_array)
        return base;
    for (i = 0; i < val->u.array.length; i++) {
        if (val->u.array.values[i]->type != json_string)
            return base;
        ret = val->u.array.values[i]->u.string.ptr;
        if (otype_match(ret, base))
            return ret;
    }
    return base;
}

/*
 * Parse a date of the form yyyy-mm-dd into a MJD value.
 */
static int parse_date(const char *str, double *out)
{
    int iy, im, id, r;
    double d1, d2;

    assert(str);
    r = sscanf(str, "%d-%d-%d", &iy, &im, &id);
    if (r != 3) goto error;
    r = eraDtf2d("UTC", iy, im, id, 0, 0, 0, &d1, &d2);
    if (r) goto error;
    *out = d1 - DJM0 + d2;
    return 0;

error:
    LOG_W("Cannot parse date '%s'", str);
    *out = 0;
    return -1;
}

static int satellite_init(obj_t *obj, json_value *args)
{
    // Support creating a satellite using noctuasky model data json values.
    satellite_t *sat = (satellite_t*)obj;
    const char *tle1, *tle2, *name = NULL,
               *launch_date = NULL, *decay_date = NULL;
    double startmfe, stopmfe, deltamin;
    int r;
    const json_value *types = NULL;

    sat->vmag = SATELLITE_DEFAULT_MAG;
    sat->stdmag = SATELLITE_DEFAULT_MAG;

    if (args) {
        r = jcon_parse(args, "{",
            "?types", JCON_VAL(types),
            "model_data", "{",
                "norad_number", JCON_INT(sat->number, 0),
                "?mag", JCON_DOUBLE(sat->stdmag, SATELLITE_DEFAULT_MAG),
                "tle", "[", JCON_STR(tle1), JCON_STR(tle2), "]",
                "?launch_date", JCON_STR(launch_date),
                "?decay_date", JCON_STR(decay_date),
            "}",
            "?names", "[", JCON_STR(name), "]",
        "}");
        if (r) {
            LOG_E("Cannot parse satellite json data");
            assert(false);
            return -1;
        }
        sat->elsetrec = sgp4_twoline2rv(tle1, tle2, 'c', 'm', 'i',
                                        &startmfe, &stopmfe, &deltamin);
        strncpy(sat->obj.type, otype_from_json(types, "Asa"), 4);

        sat->data = json_copy(args);
        sat->max_brightness = compute_max_brightness(
                sat->elsetrec, sat->stdmag);

        if (launch_date) parse_date(launch_date, &sat->launch_date);
        if (decay_date) parse_date(decay_date, &sat->decay_date);

        // Determin what 3d model to use.
        if (name && strncmp(name, "NAME STARLINK", 13) == 0)
            sat->model = "Starlink";
        if (sat->number == 25544) sat->model = "ISS";
        if (sat->number == 20580) sat->model = "HST";
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

// Check if the satellite is currently in orbit.
static bool satellite_is_operational(const satellite_t *sat, double utc)
{
    // For the moment, if we don't know the launch or decay date, we 10
    // years before/after the satellite epoch.
    double start, end, epoch;
    epoch = sgp4_get_satepoch(sat->elsetrec);
    start = sat->launch_date ? sat->launch_date - 1 : epoch - 3600;
    end = sat->decay_date ? sat->decay_date + 1 : epoch + 3600;
    return utc > start && utc < end;
}

/*
 * Update an individual satellite.
 */
static int satellite_update(satellite_t *sat, const observer_t *obs)
{
    double pv[2][3];
    char buf[128];
    int r;

    if (sat->error) return 0;
    assert(sat->elsetrec);
    if (!satellite_is_operational(sat, obs->utc)) return 0;

    // Orbit computation.
    r = sgp4(sat->elsetrec, obs->utc, pv[0],  pv[1]);
    if (r && r != 6) { // 6 = satellite decayed, don't log this case.
        obj_get_name((obj_t*)sat, buf, sizeof(buf));
        LOG_W("Satellite position error for %s (%d), err=%d",
              buf, sat->number, r);
    }
    if (r) {
        sat->error = true;
        return 0;
    }
    assert(!isnan(pv[0][0]) && !isnan(pv[0][1]));

    vec3_mul(1000.0 * DM2AU, pv[0], pv[0]);
    vec3_mul(1000.0 * DM2AU * 60 * 60 * 24, pv[1], pv[1]);
    true_equator_to_j2000(obs, pv, pv);

    vec3_copy(pv[0], sat->pvg[0]);
    vec3_copy(pv[1], sat->pvg[1]);

    position_to_apparent(obs, ORIGIN_GEOCENTRIC, false, pv, pv);
    vec3_copy(pv[0], sat->pvo[0]);
    vec3_copy(pv[1], sat->pvo[1]);

    sat->vmag = satellite_compute_vmag(sat, obs);
    return 0;
}

static int satellite_get_info(const obj_t *obj, const observer_t *obs, int info,
                              void *out)
{
    double pvo[2][4];
    double bounds[2][3], radius;
    satellite_t *sat = (satellite_t*)obj;

    satellite_update(sat, obs);
    switch (info) {
    case INFO_PVO:
        vec3_copy(sat->pvo[0], pvo[0]);
        vec3_copy(sat->pvo[1], pvo[1]);
        pvo[0][3] = 1;
        pvo[1][3] = 0;
        memcpy(out, pvo, sizeof(pvo));
        if (sat->error || !satellite_is_operational(sat, obs->utc))
            return 1;
        return 0;
    case INFO_VMAG:
        *(double*)out = sat->vmag;
        return 0;
    case INFO_RADIUS:
        if (painter_get_3d_model_bounds(NULL, sat->model, bounds) == 0) {
            radius = max3(bounds[1][0] - bounds[0][0],
                          bounds[1][1] - bounds[0][1],
                          bounds[1][2] - bounds[0][2]) / 2 * DM2AU;
            *(double*)out = radius / vec3_norm(sat->pvo[0]);
            return 0;
        }
        return 1;
    case INFO_POLE:
        vec3_normalize(sat->pvg[0], (double*)out);
        return 0;
    }
    return 1;
}

static json_value *satellite_get_json_data(const obj_t *obj)
{
    satellite_t *sat = (satellite_t*)obj;
    if (sat->data)
        return json_copy(sat->data);
    return json_object_new(0);
}

/*
 * Find the best name to display
 *
 * If the satellite is selected or if there are no NAME designations,
 * return the first designation.  Otherwise, return the first NAME
 * designation smaller than 20 bytes, or the smaller designation if none
 * of them is.
 */
static bool satellite_get_short_name(const satellite_t *sat, bool selected,
                                     char *out, int size)
{
    int i;
    json_value *jnames;
    const char* name;
    char buf[256];
    int len, best_name_len = size;

    *out = '\0';
    if (!sat->data) return false;
    jnames = json_get_attr(sat->data, "names", json_array);
    if (!jnames || jnames->u.array.length == 0) return false;
    if (selected) goto use_first_dsgn;

    for (i = 0; i < jnames->u.array.length; ++i) {
        if (jnames->u.array.values[i]->type != json_string) continue;
        name = jnames->u.array.values[i]->u.string.ptr;
        if (strncmp(name, "NAME ", 5) != 0) continue;
        designation_cleanup(name, buf, sizeof(buf), DSGN_TRANSLATE);
        len = strlen(buf);
        if (len < best_name_len) {
            best_name_len = len;
            snprintf(out, size, "%s", buf);
            if (len < 20) break;
        }
    }
    if (*out) return true;

use_first_dsgn:
    if (jnames->u.array.values[0]->type != json_string) return false;
    name = jnames->u.array.values[0]->u.string.ptr;
    designation_cleanup(name, out, size, DSGN_TRANSLATE);
    return true;
}

/*
 * Compute the rotation from ICRF to Local Vertical Local Horizontal
 * for 3d models rendering.
 */
static void get_lvlh_rot(const observer_t *obs, const double pvo[2][3],
                         double out[3][3])
{
    /*
     * X Point forward
     * Y Points overheard, away from Earth.
     */
    vec3_normalize(pvo[1], out[0]);
    vec3_add(obs->obs_pvg[0], pvo[0], out[1]);
    vec3_normalize(out[1], out[1]);
    vec3_cross(out[0], out[1], out[2]);
    vec3_normalize(out[2], out[2]);
    vec3_cross(out[2], out[0], out[1]);
}

static void satellite_render_model(const satellite_t *sat,
                                   const painter_t *painter_)
{
    double p_win[4], model_mat[4][4] = MAT4_IDENTITY;
    double lvlh_rot[3][3];
    painter_t painter = *painter_;
    json_value *args, *uniforms;

    if (!sat->model) return;
    if (!painter_project(&painter, FRAME_ICRF, sat->pvo[0], false, true, p_win))
        return;
    mat4_itranslate(model_mat, sat->pvo[0][0], sat->pvo[0][1], sat->pvo[0][2]);
    mat4_iscale(model_mat, DM2AU, DM2AU, DM2AU);

    get_lvlh_rot(painter.obs, sat->pvo, lvlh_rot);
    mat4_mul_mat3(model_mat, lvlh_rot, model_mat);

    args = json_object_new(0);
    uniforms = json_object_push(args, "uniforms", json_object_new(0));
    json_object_push(uniforms, "u_light.ambient", json_double_new(0.05));
    json_object_push(args, "use_ibl", json_boolean_new(true));
    paint_3d_model(&painter, sat->model, model_mat, args);
    json_builder_free(args);
}

static double get_model_alpha(const satellite_t *sat, const painter_t *painter,
                              double *model_size)
{
    double bounds[2][3], dim_au, angle, point_size;
    const double max_dim_au = 110 * DM2AU; // No sat is larger than that.

    if (!sat->model) return 0;

    // First check with the max possible dimension, to avoid loading the
    // model if we can.
    angle = max_dim_au / vec3_norm(sat->pvo[0]);
    point_size = core_get_point_for_apparent_angle(painter->proj, angle);
    if (point_size < 5) return 0;

    if (painter_get_3d_model_bounds(NULL, sat->model, bounds) != 0)
        return 0;
    dim_au = max3(bounds[1][0] - bounds[0][0],
                  bounds[1][1] - bounds[0][1],
                  bounds[1][2] - bounds[0][2]) * DM2AU;
    angle = dim_au / vec3_norm(sat->pvo[0]);
    point_size = core_get_point_for_apparent_angle(painter->proj, angle);
    *model_size = point_size;
    return smoothstep(5, 20, point_size);
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
    double color[4], model_alpha, model_size;
    double radius;
    char buf[256];
    const double label_color[4] = RGBA(124, 205, 124, 205);
    const double white[4] = RGBA(255, 255, 255, 255);
    satellite_t *sat = (satellite_t*)obj;
    const bool selected = core->selection && obj == core->selection;
    const double hints_limit_mag = painter.hints_limit_mag +
                                   g_satellites->hints_mag_offset - 2.5;

    satellite_update(sat, painter.obs);
    vmag = sat->vmag;
    if (sat->error || !satellite_is_operational(sat, painter.obs->utc))
        return 0;

    if (!painter_project(&painter, FRAME_ICRF, sat->pvo[0], false, true, p_win))
        return 0;

    model_alpha = get_model_alpha(sat, &painter, &model_size);

    if (!model_alpha && !selected &&
            vmag > painter.stars_limit_mag && vmag > hints_limit_mag)
        return 0;

    core_get_point_for_mag(vmag, &size, &luminance);

    // Render model if possible.
    model_alpha = get_model_alpha(sat, &painter, &model_size);
    if (model_alpha > 0) {
        satellite_render_model(sat, &painter);
        painter.color[3] *= 1.0 - model_alpha;
        core_report_luminance_in_fov(model_size * 0.005, false);
    }

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
        .obj = obj,
    };
    paint_2d_points(&painter, 1, &point);

    // Render name if needed.
    size = max(8, size);

    if (g_satellites->hints_visible &&
        (selected || vmag <= hints_limit_mag - 1.5)) {

        // Use actual pixel radius on screen.
        if (satellite_get_info(obj, painter.obs, INFO_RADIUS, &radius) == 0) {
            radius = core_get_point_for_apparent_angle(painter.proj, radius);
            size = max(size, radius);
        }

        if (satellite_get_short_name(sat, selected, buf, sizeof(buf))) {
            labels_add_3d(buf, FRAME_ICRF, sat->pvo[0], false, size + 1,
                          FONT_SIZE_BASE - 3, selected ? white : label_color, 0,
                          0, selected ? TEXT_BOLD : TEXT_FLOAT, 0, obj);
        }
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

    if (!sat->data) goto fallback;
    names = json_get_attr(sat->data, "names", json_array);
    if (!names || names->u.array.length == 0) goto fallback;
    for (i = 0; i < names->u.array.length; i++) {
        if (names->u.array.values[i]->type != json_string) goto fallback;
        name = names->u.array.values[i]->u.string.ptr;
        f(obj, user, NULL, name);
    }
    return;

fallback:
    snprintf(buf, sizeof(buf), "%05d", sat->number);
    f(obj, user, "NORAD", buf);
}

static int satellites_list(const obj_t *obj,
                           double max_mag, uint64_t hint,
                           const char *sources, void *user,
                           int (*f)(void *user, obj_t *obj))
{
    obj_t *child;
    satellite_t *sat;
    bool test_vmag = !isnan(max_mag);

    DL_FOREACH(obj->children, child) {
        sat = (void*)child;
        if (sat->error) continue;
        if (test_vmag && sat->max_brightness > max_mag)
            continue;
        if (f(user, child)) break;
    }
    return 0;
}

/*
 * Experimental.
 * Fast computation of observed altitude
 */
int satellite_get_altitude(const obj_t *obj, const observer_t *obs,
                           double *out)
{
    double pos[3], obs_pos[3], speed[3], sep, alt, theta;
    satellite_t *sat = (void*)obj;
    bool r;

    if (sat->error) return -1;
    assert(sat->elsetrec);

    r = sgp4(sat->elsetrec, obs->utc, pos, speed);
    if (r != 0) return -1;
    vec3_mul(1000.0 * DM2AU, pos, pos);

    // True equator to j2000.
    // Why do we need that?
    mat3_mul_vec3(obs->rnp, pos, pos);

    eraGd2gc(1, obs->elong, obs->phi, obs->hm, obs_pos);
    vec3_mul(DM2AU, obs_pos, obs_pos);
    theta = eraEra00(DJM0, obs->ut1);
    vec2_rotate(theta, obs_pos, obs_pos);
    vec3_sub(pos, obs_pos, pos);
    sep = eraSepp(pos, obs_pos);
    alt = M_PI / 2 - fabs(sep);
    *out = alt;
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
    .get_json_data  = satellite_get_json_data,
    .render         = satellite_render,
    .get_designations = satellite_get_designations
};
OBJ_REGISTER(satellite_klass)

static obj_klass_t satellites_klass = {
    .id             = "satellites",
    .size           = sizeof(satellites_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE | OBJ_LISTABLE,
    .init           = satellites_init,
    .add_data_source = satellites_add_data_source,
    .render_order   = 31, // After planets.
    .update         = satellites_update,
    .render         = satellites_render,
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

static void check_sat(
        int norad_number, const char *tle1, const char *tle2, double stdmag,
        int iy, int im, int id, int h, int m, double s,
        double ha_alt, double ha_az, double ha_dist, double ha_vmag,
        double alt_err, double az_err, double dist_err, double vmag_err)
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
    obj = obj_create_str("tle_satellite", json);
    assert(obj);

    obs = *core->observer;
    obs.elong = 121.5654 * DD2R;
    obs.phi = 25.0330 * DD2R;
    eraDtf2d("UTC", iy, im, id, h, m, s, &d1, &d2);
    obj_set_attr((obj_t*)&obs, "utc", d1 - DJM0 + d2 - 8. / 24);
    observer_update(&obs, false);

    obj_get_pos(obj, &obs, FRAME_OBSERVED, pos);
    eraC2s(pos, &az, &alt);
    az = eraAnp(az);
    obj_get_info(obj, &obs, INFO_DISTANCE, &dist);
    obj_get_info(obj, &obs, INFO_VMAG, &vmag);
    assert(fabs(alt * DR2D - ha_alt) < alt_err);
    assert(fabs(az * DR2D - ha_az) < az_err);
    assert(fabs(dist * DAU2M / 1000 - ha_dist) < dist_err);
    assert(fabs(ha_vmag - vmag) < vmag_err);

    satellite_get_altitude(obj, &obs, &alt);
    assert(fabs(ha_alt - alt * DR2D) < 1);

    obj_release(obj);
}

static void test_satellites(void)
{
    // Compare some position/vmag with values from heavens above.

    check_sat(20625,
        "1 20625U 90046B   20114.21029927  .00000256  00000-0  15749-3 0  9996",
        "2 20625  70.9963 124.1539 0015477 319.2677  40.7287 14.14651825545059",
        2.7,
        2020, 4, 23, 19, 59, 39,
        14, 298, 2167, 5.4,
        1, 1, 1, 1);

    check_sat(13552, // Cosmos 1408
        "1 13552U 82092A   20113.74575265  .00000921  00000-0  32526-4 0  9999",
        "2 13552  82.5670 333.7928 0017326 336.3418  23.7022 15.27738669 55547",
        4.2,
        2020, 4, 24, 19, 26, 37,
        62, 84, 557, 2.5,
        1, 1, 1, 1);

    check_sat(43563, // FALCON 9 R/B
        "1 43563U 18059B   20113.32331434  .00030378  00000-0  12213-2 0  9996",
        "2 43563  27.0685   0.0377 5648508  60.6275 343.9164  4.65462966 29379",
        2.4,
        2020, 4, 23, 17, 59, 13,
        80, 193, 1601, 3.5,
        1, 2, 1, 1);

    check_sat(25544, // ISS
        "1 25544U 98067A   20115.55025390  .00016717  00000-0  10270-3 0  9027",
        "2 25544  51.6412 253.9367 0001868 190.8144 169.2966 15.49324997 23698",
        -1.8,
        2020, 4, 24, 4, 18, 58,
        86, 130, 422, -3.8,
        1, 12, 1, 1); // Huge azimuth error here!

    /*
    check_sat(24773, // Cosmos 2341
        "1 24773U 97017B   20114.54830681 +.00000118 +00000-0 +10994-3 0  9996",
        "2 24773 082.9195 321.7152 0023111 287.2537 130.1372 13.73646213153587",
        4.2,
        2020, 4, 24, 18, 27, 41,
        0, 359, 3727, 7.3,
        1, 1, 2, 1);
    */

    check_sat(21903, // Cosmos 21903
        "1 21903U 92012B   20114.59131381 +.00000135 +00000-0 +12504-3 0  9992",
        "2 21903 082.9337 132.3220 0044214 070.4659 060.4640 13.74510053410361",
        4.2,
        2020, 4, 24, 18, 38, 40,
        84, 93, 967, 4.0,
        1, 5, 1, 1);

    check_sat(44729, // STARLINK-1024
        "1 44729C 19074S   20115.19248806  .00009322  00000-0  63846-3 0  1150",
        "2 44729  52.9955 118.1239 0001220  90.6225 328.8155 15.05572044    13",
        4.0,
        2020, 4, 24, 18, 48, 8,
        10, 219, 1810, 5.5,
        1, 1, 3, 1);
}

TEST_REGISTER(NULL, test_satellites, TEST_AUTO);

#endif // COMPILE_TESTS
