/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

core_t *core;   // The global core object.

#define CORE_MIN_FOV (1./3600 * DD2R)

static void core_on_fov_changed(obj_t *obj, const attribute_t *attr)
{
    // For the moment there is not point going further than 0.5°.
    projection_t proj;
    core_get_proj(&proj);
    core->fov = clamp(core->fov, CORE_MIN_FOV, proj.max_fov);
}

static void core_on_utcoffset_changed(obj_t *obj, const attribute_t *attr)
{
    core->utc_offset = clamp(core->utc_offset, -24 * 60, +24 * 60);
}

static void add_progressbar(void *user, const char *id, const char *label,
                            int v, int total)
{
    json_value *array = user, *val;
    val = json_object_new(0);
    json_object_push(val, "id", json_string_new(id));
    json_object_push(val, "label", json_string_new(label));
    json_object_push(val, "total", json_integer_new(total));
    json_object_push(val, "value", json_integer_new(v));
    json_array_push(array, val);
}

static json_value *core_fn_progressbars(obj_t *obj, const attribute_t *attr,
                                        const json_value *args)
{
    json_value *ret;
    ret = json_array_new(0);
    progressbar_list(ret, add_progressbar);
    return ret;
}

static obj_t *core_get(const obj_t *obj, const char *id, int flags)
{
    obj_t *module;
    obj_t *ret;
    uint64_t oid, nsid;
    DL_FOREACH(core->obj.children, module) {
        if (module->id && strcmp(module->id, id) == 0) return module;
        ret = obj_get(module, id, flags);
        if (ret) return ret;
    }
    oid = identifiers_search(id);
    if (oid) {
        DL_FOREACH(core->obj.children, module) {
            ret = obj_get_by_oid(module, oid, 0);
            if (ret) return ret;
        }
    }
    // Special case for nsid.
    if (sscanf(id, "NSID %" PRIx64, &nsid) == 1)
        return obj_get_by_nsid(obj, nsid);
    return NULL;
}

obj_t *core_get_module(const char *id)
{
    obj_t *module;
    DL_FOREACH(core->obj.children, module) {
        if (module->id && strcmp(module->id, id) == 0) return module;
    }
    return NULL;
}

static obj_t *core_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *module;
    obj_t *ret;
    DL_FOREACH(core->obj.children, module) {
        ret = obj_get_by_oid(module, oid, hint);
        if (ret) return ret;
    }
    return NULL;
}

static obj_t *core_get_by_nsid(const obj_t *obj, uint64_t nsid)
{
    obj_t *module;
    obj_t *ret;
    DL_FOREACH(core->obj.children, module) {
        ret = obj_get_by_nsid(module, nsid);
        if (ret) return ret;
    }
    return NULL;
}

static int core_list(const obj_t *obj, observer_t *obs,
                     double max_mag, uint64_t hint, void *user,
                     int (*f)(void *user, obj_t *obj))
{
    // XXX: won't stop if the callback return != 0.
    obj_t *module;
    int nb = 0;
    DL_FOREACH(core->obj.children, module) {
        nb += module_list_objs(module, obs, max_mag, hint, user, f);
    }
    return nb;
}

static int core_add_data_source(obj_t *obj, const char *url, const char *type,
                                json_value *args)
{
    obj_t *module;
    int r;
    DL_FOREACH(core->obj.children, module) {
        if (!(module->klass->flags & OBJ_MODULE)) continue;
        r = module_add_data_source(module, url, type, args);
        if (r == 1) continue; // Can't add here.
        return r;
    }
    return 1;
}

static int modules_sort_cmp(void *a, void *b)
{
    obj_t *at, *bt;
    at = a;
    bt = b;
    return cmp(obj_get_render_order(at), obj_get_render_order(bt));
}


/*
 * Function: core_get_proj
 * Get the core current view projection
 *
 * Parameters:
 *   proj   - Pointer to a projection_t instance that get initialized.
 */
void core_get_proj(projection_t *proj)
{
    double fovx, fovy;
    double aspect = core->win_size[0] / core->win_size[1];
    projection_compute_fovs(core->proj, core->fov, aspect, &fovx, &fovy);
    projection_init(proj, core->proj, fovx,
                    core->win_size[0], core->win_size[1]);
}

obj_t *core_get_obj_at(double x, double y, double max_dist)
{
    double pos[2] = {x, y};
    uint64_t oid, hint;
    if (!areas_lookup(core->areas, pos, max_dist, &oid, &hint))
        return NULL;
    if (!oid) return NULL;
    return obj_get_by_oid(NULL, oid, hint);
}

EMSCRIPTEN_KEEPALIVE
void core_add_default_sources(void)
{
    #define BASE_URL "https://data.stellarium.org/"
    module_add_data_source(NULL, BASE_URL "landscapes", NULL, NULL);

    // Bundled star survey.
    module_add_data_source(NULL, "asset://stars", "hips", NULL);
    // Online DSO survey.
    module_add_data_source(NULL, BASE_URL "surveys/dso", "hips", NULL);

    // Skyculture.  We load the western culture immediately so we don't have
    // to wait to parse the online directory index.json file.
    asset_set_alias(BASE_URL "skycultures", "asset://skycultures");
    module_add_data_source(NULL, BASE_URL "skycultures/western", "skyculture",
                        NULL);
    module_add_data_source(NULL, BASE_URL "skycultures", NULL, NULL);

    // HiPS surveys.
    module_add_data_source(NULL, BASE_URL "surveys", "hipslist", NULL);

    module_add_data_source(NULL, "https://alasky.unistra.fr/DSS/DSSColor",
                        "hips", NULL);

    // Asteroid data.
    module_add_data_source(NULL, "asset://mpcorb.dat", "mpc_asteroids", NULL);
    #undef BASE_URL
}

// Set the default init values.
static void core_set_default(void)
{
    double tsl;
    observer_t *obs = core->observer;

    // Reset to Taipei.
    obj_set_attr(&obs->obj, "latitude", "f", 25.066667 * DD2R);
    obj_set_attr(&obs->obj, "longitude", "f", 121.516667 * DD2R);
    obj_set_attr(&obs->obj, "elevation", "f", 0.0);
    obs->tt = unix_to_mjd(sys_get_unix_time());

    // We approximate the pressure from the altitude and the sea level
    // temperature in K (See Astrophysical Quantities, C.W.Allen, 3rd edition,
    // section 52).
    tsl = 15 + 273.15;  // Let say we have a see level temp of 15 deg C.
    obs->pressure = 1013.25 * exp(-obs->hm / (29.3 * tsl));
    obs->refraction = true;

    core->fov = 90 * DD2R;
    core->utc_offset = sys_get_utc_offset() / 60;

    core->proj = PROJ_STEREOGRAPHIC;
    core->lwmax = 5000;

    // Adjust those values to make the sky look good.
    core->star_linear_scale = 0.7;
    core->star_relative_scale = 1.5;
    core->lwmax_min = 0.004;
    core->lwmax_scale = 13.0;
    core->max_point_radius = 6.0;
    core->min_point_radius = 0.5;
    core->skip_point_radius = 0.2;
    tonemapper_update(&core->tonemapper, 1, 1, 1, core->lwmax);

    core->telescope_auto = true;
    observer_update(core->observer, false);
}

static void on_progressbar(const char *id)
{
    obj_changed((obj_t*)core, "progressbars");
}

// Callback for texture loading.
static uint8_t *texture_load_function(
        void *user, const char *url, int *code,
        int *w, int *h, int *bpp)
{
    const void *data;
    int size;
    data = asset_get_data(url, &size, code);
    if (!data) return NULL;
    return img_read_from_mem(data, size, w, h, bpp);
}

EMSCRIPTEN_KEEPALIVE
void core_init(double win_w, double win_h, double pixel_scale)
{
    char cache_dir[1024];
    obj_klass_t *module;

    if (core) {
        // Already initialized.
        core_set_default();
        return;
    }
    profile_init();
    texture_set_load_callback(NULL, texture_load_function);
    sprintf(cache_dir, "%s/%s", sys_get_user_dir(), ".cache");
    request_init(cache_dir);
    identifiers_init();

    core = (core_t*)obj_create("core", "core", NULL, NULL);
    core->win_size[0] = win_w;
    core->win_size[1] = win_h;
    core->win_pixels_scale = pixel_scale;
    module_add_sub(&core->obj, "hints");
    core->hints_mag_offset = 0;

    core->observer = (observer_t*)obj_create("observer", "observer",
                                             (obj_t*)core, NULL);

    for (module = obj_get_all_klasses(); module; module = module->next) {
        if (!(module->flags & OBJ_MODULE)) continue;
        obj_create(module->id, module->id, (obj_t*)core, NULL);
    }
    DL_SORT(core->obj.children, modules_sort_cmp);

    core->areas = areas_create();
    progressbar_add_listener(on_progressbar);

    core_set_default();
}

void core_release(void)
{
    obj_t *module;
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->del) module->klass->del(module);
    }
    profile_release();
}

void core_update_fov(double dt)
{
    double t;
    double save_fov = core->fov;

    if (core->fov_animation.duration) {
        core->fov_animation.t += dt / core->fov_animation.duration;
        t = smoothstep(0.0, 1.0, core->fov_animation.t);
        // Make sure we finish on an exact value.
        if (core->fov_animation.t >= 1.0) t = 1.0;
        if (core->fov_animation.dst_fov) {
            core->fov = mix(core->fov_animation.src_fov,
                            core->fov_animation.dst_fov, t);
        }
        if (core->fov_animation.t >= 1.0) {
            core->fov_animation.duration = 0.0;
            core->fov_animation.t = 0.0;
            core->fov_animation.dst_fov = 0.0;
        }
    }

    projection_t proj;

    const double ZOOM_FACTOR = 0.05;
    // Continuous zoom.
    core_get_proj(&proj);
    if (core->zoom) {
        core->fov *= pow(1. + ZOOM_FACTOR * (-core->zoom), dt/(1./60));
        if (core->fov > proj.max_fov)
            core->fov = proj.max_fov;
    }

    core->fov = clamp(core->fov, CORE_MIN_FOV, proj.max_fov);

    if (core->fov != save_fov)
        obj_changed((obj_t*)core, "fov");
}

static int core_update_direction(double dt)
{
    double v[4] = {1, 0, 0, 0}, q[4], t, az, al, vv[4];

    if (core->target.duration) {
        core->target.t += dt / core->target.duration;
        t = smoothstep(0.0, 1.0, core->target.t);
        // Make sure we finish on an exact value.
        if (core->target.t >= 1.0) t = 1.0;
        if (core->target.lock && core->target.move_to_lock) {
            // We are moving toward a potentially moving target, adjust the
            // destination
            obj_get_pos_observed(core->target.lock, core->observer, vv);
            eraC2s((double*)vv, &az, &al);
            quat_set_identity(core->target.dst_q);
            quat_rz(az, core->target.dst_q, core->target.dst_q);
            quat_ry(-al, core->target.dst_q, core->target.dst_q);
        }
        if (!core->target.lock || core->target.move_to_lock) {
            quat_slerp(core->target.src_q, core->target.dst_q, t, q);
            quat_mul_vec3(q, v, v);
            eraC2s(v, &core->observer->azimuth, &core->observer->altitude);
        }
        if (core->target.t >= 1.0) {
            core->target.duration = 0.0;
            core->target.t = 0.0;
            core->target.move_to_lock = false;
        }
        // Notify the changes.
        obj_changed(&core->observer->obj, "altitude");
        obj_changed(&core->observer->obj, "azimuth");
    }

    if (core->target.lock && !core->target.move_to_lock) {
        obj_get_pos_observed(core->target.lock, core->observer, v);
        eraC2s(v, &core->observer->azimuth, &core->observer->altitude);
        // Notify the changes.
        obj_changed(&core->observer->obj, "altitude");
        obj_changed(&core->observer->obj, "azimuth");
    }

    return 1;
}

EMSCRIPTEN_KEEPALIVE
int core_update(double dt)
{
    bool atm_visible;
    double lwmax;
    int r;
    obj_t *atm, *module;

    atm = core_get_module("atmosphere");
    assert(atm);
    obj_get_attr(atm, "visible", "b", &atm_visible);
    observer_update(core->observer, true);
    // Update telescope according to the fov.
    if (core->telescope_auto)
        telescope_auto(&core->telescope, core->fov);
    progressbar_update();

    // Update eye adaptation.
    lwmax = core->lwmax * core->lwmax_scale;
    lwmax = exp(logf(core->tonemapper.lwmax) +
                (logf(lwmax) - logf(core->tonemapper.lwmax)) * 0.1);
    tonemapper_update(&core->tonemapper, -1, -1, -1, lwmax);
    core->lwmax = core->lwmax_min; // Reset for next frame.

    core_update_direction(dt);

    DL_SORT(core->obj.children, modules_sort_cmp);
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->update) {
            r = module->klass->update(module, core->observer, dt);
            if (r < 0) LOG_E("Error updating module '%s'", module->id);
        }
    }

    return 0;
}

// Test whether the landscape hides the view below the horizon.
static bool is_below_horizon_hidden(void)
{
    obj_t *ls;
    bool visible;
    ls = core_get_module("landscapes");
    obj_get_attr(ls, "visible", "b", &visible);
    // XXX: we should let the lanscape module notify the core that it hides
    // the stars instead.
    return (visible && core->observer->altitude >= 0);
}

/*
 * Compute the magnitude of the dimmest visible star.
 */
static double compute_max_vmag(void)
{
    // Compute by dichotomy.
    const int max_iter = 16;
    const double min_l = 0.1;
    double m = 0, m1 = 0.0, m2 = 128.0;
    double r, l;
    int i;

    core_get_point_for_mag(m1, &r, &l);
    if (r == 0) return m1;

    for (i = 0; i < max_iter; i++) {
        m = (m1 + m2) / 2;
        core_get_point_for_mag(m, &r, &l);
        if (r && l < min_l) return m;
        *(r ? &m1 : &m2) = m;
    }
    // if (i >= max_iter) LOG_W("Too many iterations! (%f)", m);
    return m;
}

/*
 * Function: win_to_observed
 * Convert a window 2D position to a 3D azalt direction.
 *
 * Parameters:
 *   x    - The window x position.
 *   y    - The window y position.
 *   p    - Corresponding 3D unit vector in azalt (after refraction).
 */
static void win_to_observed(double x, double y, double p[3])
{
    projection_t proj;
    double pos[4] = {x, y};

    core_get_proj(&proj);
    // Convert to NDC coordinates.
    pos[0] = pos[0] / core->win_size[0] * 2 - 1;
    pos[1] = -1 * (pos[1] / core->win_size[1] * 2 - 1);
    project(&proj, PROJ_BACKWARD, 4, pos, pos);
    convert_frame(core->observer, FRAME_VIEW, FRAME_OBSERVED, true, pos, p);
}


EMSCRIPTEN_KEEPALIVE
int core_render(double win_w, double win_h, double pixel_scale)
{
    PROFILE(core_render, 0);
    obj_t *module;
    projection_t proj;
    double t;
    bool cst_visible;
    double max_vmag;

    // Used to make sure some values are not touched during render.
    struct {
        observer_t obs;
        double fov;
    } bck = {
        .obs = *core->observer,
        .fov = core->fov,
    };
    (void)bck;

    core->win_size[0] = win_w;
    core->win_size[1] = win_h;
    core->win_pixels_scale = pixel_scale;
    core_get_proj(&proj);

    observer_update(core->observer, true);
    max_vmag = compute_max_vmag();

    t = sys_get_unix_time();
    if (!core->prof.start_time) core->prof.start_time = t;
    // Reset counter every 60 frames.
    if (core->prof.nb_frames++ >= 60) {
        core->prof.fps = core->prof.nb_frames / (t - core->prof.start_time);
        obj_changed(&core->obj, "fps");
        core->prof.start_time = t;
        core->prof.nb_frames = 0;
    }

    if (!core->rend)
        core->rend = render_gl_create();
    labels_reset();

    // Show bayer only if the constellations are visible.
    module = core_get_module("constellations");
    assert(module);
    obj_get_attr(module, "visible", "b", &cst_visible);

    painter_t painter = {
        .rend = core->rend,
        .obs = core->observer,
        .transform = &mat4_identity,
        .fb_size = {win_w * pixel_scale, win_h * pixel_scale},
        .pixel_scale = pixel_scale,
        .proj = &proj,
        .stars_limit_mag = max_vmag,
        .hints_limit_mag = max_vmag + core->hints_mag_offset,
        .points_smoothness = 0.75,
        .color = {1.0, 1.0, 1.0, 1.0},
        .contrast = 1.0,
        .lines_width = 1.0,
        .flags = (core->fast_mode ? PAINTER_FAST_MODE : 0) |
            (is_below_horizon_hidden() ? PAINTER_HIDE_BELOW_HORIZON : 0) |
            (cst_visible ? PAINTER_SHOW_BAYER_LABELS : 0),
    };
    painter_update_caps(&painter);
    paint_prepare(&painter, win_w, win_h, pixel_scale);

    DL_FOREACH(core->obj.children, module) {
        obj_render(module, &painter);
    }

    // Render the viewport cap for debugging.
    // Reduce the cap size in compute_viewport_cap to see it and debug.
    if ((0)) {
        double r;
        double p[4];
        vec3_copy(painter.viewport_caps[FRAME_ICRF], p);
        p[3] = 0;
        r = acos(painter.viewport_caps[FRAME_ICRF][3]) * 2;
        obj_t* obj = obj_create("circle", "cap_circle", NULL, NULL);
        obj_set_attr(obj, "pos", "v4", p);
        obj_set_attr(obj, "frame", "d", FRAME_ICRF);
        double size[2] = {r, r};
        obj_set_attr(obj, "size", "v2", size);
        obj_render(obj, &painter);
        obj_release(obj);
    }

    // Flush all rendering pipeline
    paint_finish(&painter);

    // Do post render (e.g. for GUI)
    DL_FOREACH(core->obj.children, module) {
        obj_post_render(module, &painter);
    }

    core->fast_mode = false;

    assert(bck.obs.azimuth == core->observer->azimuth);
    assert(bck.obs.altitude == core->observer->altitude);
    assert(bck.fov == core->fov);
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void core_on_mouse(int id, int state, double x, double y)
{
    obj_t *module;
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->on_mouse) {
            module->klass->on_mouse(module, id, state, x, y);
        };
    }
}

EMSCRIPTEN_KEEPALIVE
void core_on_key(int key, int action)
{
    static char *SC[][3] = {
        {"A", "core.atmosphere"},
        {"G", "core.landscapes"},
        {"C", "core.constellations.lines"},
        {"R", "core.constellations.images"},
        {"Z", "core.lines.azimuthal"},
        {"E", "core.lines.equatorial"},
        {"M", "core.lines.meridian"},
        {"N", "core.dsos"},
        {"D", "core.dss"},
        {"S", "core.stars"},
        {"T", "core", "test"},
    };
    int i;
    bool v;
    obj_t *obj;
    const char *attr;
    char buf[128];

    core->inputs.keys[key] = (action != KEY_ACTION_UP);

    if (core->gui_want_capture_mouse) return;
    if (action != KEY_ACTION_DOWN) return;

    for (i = 0; i < ARRAY_SIZE(SC); i++) {
        if (SC[i][0][0] == key) {
            attr = SC[i][2] ?: "visible";
            obj = obj_get(NULL, SC[i][1], 0);
            obj_get_attr(obj, attr, "b", &v);
            obj_set_attr(obj, attr, "b", !v);
            return;
        }
    }
    if (key == ' ' && core->selection) {
        LOG_D("lock to %s", obj_get_name(core->selection, buf));
        obj_set_attr(&core->obj, "lock", "p", core->selection);
    }
}

void core_on_char(uint32_t c)
{
    int i;
    if (c > 0 && c < 0x10000) {
        for (i = 0; i < ARRAY_SIZE(core->inputs.chars); i++) {
            if (!core->inputs.chars[i]) {
                core->inputs.chars[i] = c;
                break;
            }
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void core_on_zoom(double k, double x, double y)
{
    double fov, pos_start[3], pos_end[3];
    double sal, saz, dal, daz;

    win_to_observed(x, y, pos_start);
    obj_get_attr(&core->obj, "fov", "f", &fov);
    fov /= k;
    obj_set_attr(&core->obj, "fov", "f", fov);
    win_to_observed(x, y, pos_end);

    // Adjust lat/az to keep the mouse point at the same position.
    eraC2s(pos_start, &saz, &sal);
    eraC2s(pos_end, &daz, &dal);
    core->observer->azimuth += (saz - daz);
    core->observer->altitude += (sal - dal);
    core->observer->altitude = clamp(core->observer->altitude,
                                     -M_PI / 2, +M_PI / 2);
    core->fast_mode = true;
    // Notify the changes.
    obj_changed(&core->observer->obj, "altitude");
    obj_changed(&core->observer->obj, "azimuth");
}


/*
 * Function: core_get_point_for_mag
 * Compute a point radius and luminosity from a observed magnitude.
 *
 * The function is almost linear, but when the points get too small,
 * I make the curve go to zero faster, so that the bright stars get a
 * higher contrast.  Also for very small points, we use a minimum radius
 * and instead lower the luminance.
 *
 * Parameters:
 *   mag       - The observed magnitude.
 *   radius    - Output radius in window pixels.
 *   luminance - Output luminance from 0 to 1, gamma corrected.  Ignored if
 *               set to NULL.
 */
void core_get_point_for_mag(double mag, double *radius, double *luminance)
{

    /*
     * Use the formulas from:
     * https://en.wikipedia.org/wiki/Surface_brightness
     */

    const double foveye = 60 * DD2R;
    double log_e, log_lw, ld, r, s, pr;
    const telescope_t *tel = &core->telescope;
    const double s_linear = core->star_linear_scale;
    const double s_relative = core->star_relative_scale;
    const double r_min = core->min_point_radius;

    /*
     * Compute illuminance (in lux = lum/m² = cd.sr/m²)
     * Get log10 of the value for optimisation.
     *
     * S = m + 2.5 * log10(A)         | S: vmag/arcmin², A: arcmin²
     * L = 10.8e4 * 10^(-0.4 * S)     | S: vmag/arcmin², L: cd/m²
     * E = L * A                      | E: lux (= cd.sr/m²), A: sr, L: cd/m²
     *
     * => E = 10.8e4 / R2AS^2 * 10^(-0.4 * m)
     * => log10(E) = log10(10.8e4 / R2AS^2) - 0.4 * m
     */
    log_e = log10(10.8e4 / (ERFA_DR2AS * ERFA_DR2AS)) - 0.4 * mag;

    /*
     * Apply optic from telescope light grasp.
     *
     * E' = E * Gl
     * Gmag = 2.5 * log10(Gl)
     *
     * Log10(E') = Log10(E) + Gmag / 2.5
     */
    log_e += tel->gain_mag / 2.5;

    /*
     * Compute luminance assuming a point radius of 2.5 arcmin.
     *
     * L = E / (pi * R^2)
     * => Log10(L) = Log10(E) - Log10(pi * R^2)
     */
    pr = 2.5 / 60 * DD2R;
    log_lw = log_e - log10(M_PI * pr * pr);

    // Apply eye adaptation.
    ld = tonemapper_map_log10(&core->tonemapper, log_lw);
    if (ld < 0) ld = 0; // Prevent math error.

    // Extra scale if the telescope magnification is not enough to reach the
    // current zoom level.
    s = (foveye / core->fov) / tel->magnification;
    // Compute r, using both manual adjustement factors.
    r = s_linear * pow(ld, s_relative / 2.0) * s;

    // If the radius is really too small, we don't render the star.
    if (r < core->skip_point_radius) {
        r = 0;
        ld = 0;
    }

    // If the radius is too small, we adjust the luminance.
    if (r > 0 && r < r_min) {
        ld *= (r / r_min) * (r / r_min) * (r / r_min);
        r = r_min;
    }

    ld = pow(ld, 1 / 2.2); // Gama correction.
    // Saturate radius after a certain point.
    // XXX: make it smooth.
    r = min(r, core->max_point_radius);
    *radius = r;
    if (luminance) *luminance = clamp(ld, 0, 1);
}


/*
 * Function: core_get_apparent_angle_for_point
 * Get angular radius of a round object from it's pixel radius on screen.
 *
 * For example this can be used after core_get_point_for_mag to estimate the
 * angular size a circle should have to exactly fit the object.
 *
 * Parameters:
 *   proj   - The projection used.
 *   r      - Radius on screen in window pixels.
 *
 * Return:
 *   Angular radius in radian.  This is the physical radius, not scaled by
 *   the fov.
 */
double core_get_apparent_angle_for_point(const projection_t *proj, double r)
{
    const double win_w = proj->window_size[0];
    return r * proj->scaling[0] / win_w * 2;
}

/*
 * Function: core_report_vmag_in_fov
 * Inform the core that an object with a given vmag is visible.
 *
 * This is used for the eyes adaptations algo.
 *
 * Parameters:
 *   vmag - The magnitude of the object.
 *   r    - Visible radius of the object (rad).
 *   sep  - Separation of the center of the object to the center of the
 *          screen.
 */
void core_report_vmag_in_fov(double vmag, double r, double sep)
{
    double lf, lum, r2;

    // Compute flux and luminance.
    vmag -= core->telescope.gain_mag;
    lf = 10.8e4 / (ERFA_DR2AS * ERFA_DR2AS) * pow(10, -0.4 * vmag);
    lum = lf / (M_PI * r * r);

    lum = min(lum, 700);
    r2 = r * (60 * DD2R / core->fov);
    lum *= pow(r2 / (60 * DD2R), 2);
    lum *= smoothstep(core->fov * 0.75, 0, max(0, sep - r));
    core_report_luminance_in_fov(lum, false);
}

void core_report_luminance_in_fov(double lum, bool fast_adaptation)
{
    core->lwmax = max(core->lwmax, lum);
    if (fast_adaptation && core->lwmax > core->tonemapper.lwmax) {
        tonemapper_update(&core->tonemapper, -1, -1, -1,
                          core->lwmax * core->lwmax_scale);
    }
}

static int do_core_lookat(double* pos, double duration) {
    double az, al;

    // Direct lookat.
    if (duration == 0.0) {
        eraC2s(pos, &core->observer->azimuth, &core->observer->altitude);
        return 0;
    }

    quat_set_identity(core->target.src_q);
    quat_rz(core->observer->azimuth, core->target.src_q, core->target.src_q);
    quat_ry(-core->observer->altitude, core->target.src_q, core->target.src_q);

    eraC2s((double*)pos, &az, &al);
    quat_set_identity(core->target.dst_q);
    quat_rz(az, core->target.dst_q, core->target.dst_q);
    quat_ry(-al, core->target.dst_q, core->target.dst_q);

    core->target.duration = duration;
    core->target.t = 0.0;
    core->fast_mode = true;
    return 0;
}

static json_value *core_point_and_lock(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    double v[4], duration = 1.0;
    obj_t* target_obj;
    args_get(args, "target", 1, "p", "obj", &target_obj);
    args_get(args, "speed", 2, "f", NULL, &duration);

    obj_set_attr(&core->obj, "lock", "p", target_obj);

    obj_get_pos_observed(core->target.lock, core->observer, v);
    do_core_lookat(v, duration);
    core->target.move_to_lock = true;
    return NULL;
}

static json_value *core_lookat(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    // XXX find a better way to create a rot quaternion from a direction?
    double duration = 1.0, pos[3];

    args_get(args, "target", 1, "v3", "azalt", pos);
    args_get(args, "speed", 2, "f", NULL, &duration);

    do_core_lookat(pos, duration);
    return NULL;
}

static json_value *core_zoomto(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    double duration = 1.0, fov = 0.0;

    args_get(args, "fov", 1, "f", NULL, &fov);
    args_get(args, "speed", 2, "f", NULL, &duration);

    projection_t proj;
    core_get_proj(&proj);
    if (fov > proj.max_fov)
        fov = proj.max_fov;

    // Direct lookat.
    if (duration == 0.0) {
        core->fov = fov;
        return NULL;
    }

    typeof(core->fov_animation)* anim = &core->fov_animation;
    if (anim->t < 1 && anim->t > 0) {
        // We request a new animation while another one is still on going
        if (fov == anim->dst_fov) {
            // Same animation is going on, just finish it
            return NULL;
        }
        // We are looking for a new set of zoom parameters so that:
        // - we preserve the current zoom level
        // - the remaining animation time is equal to the new duration
        double t2 = (anim->t * anim->duration) / (anim->t * anim->duration +
                                                  duration);
        assert(t2 >= 0 && t2 <= 1);
        double st2 = smoothstep(0, 1, t2);
        double src2 = (core->fov - fov * st2) / (1.0 - st2);
        anim->src_fov = src2;
        anim->dst_fov = fov;
        anim->duration = anim->t * anim->duration + duration;
        anim->t = t2;
        return NULL;
    }

    anim->src_fov = core->fov;
    anim->dst_fov = fov;
    anim->duration = duration;
    anim->t = 0.0;

    return NULL;
}

// Return a static string representation of a an object type id.
// XXX: deprecated.  Use otypes_get_str instead.
EMSCRIPTEN_KEEPALIVE
const char *type_to_str(const char type[4])
{
    const char *explanation;
    explanation = otype_get_str(type);
    if (explanation) return explanation;

    // Most of yhose values should eventually be removed, since we are now
    // using Simbad types everywhere.
    static const char *TABLE[][2] = {
        // XXX: some are from NGC2000 and could be removed.
        {"Gx  ", "Galaxy"},
        {"OC  ", "Open star cluster"},
        {"Gb  ", "Globular star cluster"},
        {"Nb  ", "Bright emission or reflection nebula"},
        {"Pl  ", "Planetary nebula"},
        {"C+N ", "Cluster associated with nebulosity"},
        {"Ast ", "Asterism or group of a few stars"},
        {"Kt  ",  "Knot or nebulous region in an external galaxy"},
        {"*** ", "Triple star"},
        {"D*  ",  "Double star"},
        {"*   ",   "Star"},
        {"PD  ",  "Photographic plate defect"},

        {"PLA ", "Planet"},
        {"MOO ", "Moon"},
        {"SUN ", "Sun"},

        // From OpenNGC
        {"*Ass", "Association of stars"},
        {"OCl ", "Open Cluster"},
        {"GCl ", "Globular Cluster"},
        {"Cl+N", "Star cluster + Nebula"},
        {"G   ", "Galaxy"},
        {"GPai", "Galaxy Pair"},
        {"GTrp", "Galaxy Triplet"},
        {"GGro", "Group of galaxies"},
        {"PN  ", "Planetary Nebula"},
        {"HII ", "HII Ionized region"},
        {"EmN ", "Emission Nebula"},
        {"Neb ", "Nebula"},
        {"RfN ", "Reflection Nebula"},
        {"SNR ", "Supernova remnant"},
        {"Nova", "Nova star"},
        {"NonE", "Nonexistent object"},
    };
    int i;
    char t[4] = "    ";
    memcpy(t, type, strlen(type));
    for (i = 0; i < ARRAY_SIZE(TABLE); i++) {
        if (strncmp(t, TABLE[i][0], 4) == 0)
            return TABLE[i][1];
    }
    return "?";
}

static obj_klass_t core_klass = {
    .id = "core",
    .size = sizeof(core_t),
    .flags = OBJ_IN_JSON_TREE,
    .get = core_get,
    .get_by_oid = core_get_by_oid,
    .get_by_nsid = core_get_by_nsid,
    .list = core_list,
    .add_data_source = core_add_data_source,
    .attributes = (attribute_t[]) {
        PROPERTY("utcoffset", "d", MEMBER(core_t, utc_offset),
                 .on_changed = core_on_utcoffset_changed),
        PROPERTY("fov", "f", MEMBER(core_t, fov),
                 .on_changed = core_on_fov_changed),
        PROPERTY("projection", "d", MEMBER(core_t, proj)),
        PROPERTY("selection", "p", MEMBER(core_t, selection),
                 .hint = "obj"),
        PROPERTY("lock", "p", MEMBER(core_t, target.lock),
                 .hint = "obj"),
        PROPERTY("hovered", "p", MEMBER(core_t, hovered),
                 .hint = "obj"),
        PROPERTY("hints_mag_offset", "f", MEMBER(core_t, hints_mag_offset),
                 .sub = "hints"),
        PROPERTY("progressbars", "json", .fn = core_fn_progressbars),
        PROPERTY("fps", "f", MEMBER(core_t, prof.fps)),
        PROPERTY("clicks", "d", MEMBER(core_t, clicks)),
        PROPERTY("ignore_clicks", "b", MEMBER(core_t, ignore_clicks)),
        PROPERTY("zoom", "f", MEMBER(core_t, zoom)),
        PROPERTY("test", "b", MEMBER(core_t, test)),
        FUNCTION("lookat", .fn = core_lookat),
        FUNCTION("point_and_lock", .fn = core_point_and_lock),
        FUNCTION("zoomto", .fn = core_zoomto),
        {}
    }
};
OBJ_REGISTER(core_klass)

/******* TESTS **********************************************************/

#if COMPILE_TESTS

// Some tests of vec/mat functions.
static void test_vec(void)
{
    double mat[4][4];
    double e = 0.0000001;
    mat4_set_identity(mat);
    mat4_rx(M_PI / 2, mat, mat);
    assert(vec3_dist2(mat[0], (double[]){1,  0, 0}) < e);
    assert(vec3_dist2(mat[1], (double[]){0,  0, 1}) < e);
    assert(vec3_dist2(mat[2], (double[]){0, -1, 0}) < e);

    mat4_invert(mat, mat);
    assert(vec3_dist2(mat[0], (double[]){1, 0, 0}) < e);
    assert(vec3_dist2(mat[1], (double[]){0, 0, -1}) < e);
    assert(vec3_dist2(mat[2], (double[]){0, 1, 0}) < e);
}

static void test_core(void)
{
    double v;
    core_init(100, 100, 1.0);
    core->observer->hm = 10.0;
    obj_t *obs = obj_get(NULL, "core.observer", 0);
    assert(obs);
    obj_get_attr(obs, "elevation", "f", &v); assert(v == 10.0);
    obj_set_attr(obs, "longitude", "f", 1.0);
    assert(core->observer->elong == 1.0);
    obj_get_attr(obs, "longitude", "f", &v); assert(v == 1.0);
    obj_get_attr(obs, "utc", "f", &v);
}

static void test_basic(void)
{
    obj_t *obj;
    // Test search bright star by HIP:
    obj = obj_get(NULL, "HIP 677", 0);
    assert(obj);
    obj_release(obj);
    // Test obj_get with '|'
    obj = obj_get(NULL, "No result|Sun", 0);
    assert(obj);
    obj_release(obj);
}

static void test_set_city(void)
{
    double lat;
    core_init(100, 100, 1.0);
    obj_t *city;
    obj_t *obs = &core->observer->obj;

    // Make sure that after we set the city, the position has been updated.
    city = obj_get(NULL, "CITY GB London", 0);
    obj_get_attr(city, "latitude", "f", &lat);
    assert(fabs(lat * DR2D - 51.50853) < 0.01);
    obj_set_attr(obs, "city", "p", obj_get(NULL, "CITY GB London", 0));
    obj_get_attr(obs, "latitude", "f", &lat);
    assert(fabs(lat * DR2D - 51.50853) < 0.01);
    obj_get_attr(obs, "city", "p", &city);
    assert(city == core->observer->city);
}

TEST_REGISTER(NULL, test_core, TEST_AUTO);
TEST_REGISTER(NULL, test_vec, TEST_AUTO);
TEST_REGISTER(NULL, test_basic, TEST_AUTO);
TEST_REGISTER(NULL, test_set_city, TEST_AUTO);

#endif
