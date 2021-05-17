/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "algos/utctt.h"
#include "navigation.h"
#include "render.h"

core_t *core;   // The global core object.

#define exp10(x) exp((x) * log(10.f))

static void core_on_fov_changed(obj_t *obj, const attribute_t *attr)
{
    // For the moment there is not point going further than 0.5°.
    projection_t proj;
    core_get_proj(&proj);
    core->fov = clamp(core->fov, CORE_MIN_FOV, proj.klass->max_fov);
}

static void add_progressbar(void *user, const char *id, const char *label,
                            int v, int total,
                            int error, const char *error_msg)
{
    json_value *array = user, *val;
    val = json_object_new(0);
    json_object_push(val, "id", json_string_new(id));
    json_object_push(val, "label", json_string_new(label));
    json_object_push(val, "total", json_integer_new(total));
    json_object_push(val, "value", json_integer_new(v));
    if (error) {
        json_object_push(val, "error", json_integer_new(error));
        json_object_push(val, "error_msg", json_string_new(error_msg));
    }
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

EMSCRIPTEN_KEEPALIVE
obj_t *core_get_module(const char *id)
{
    int len;
    const char *end;
    obj_t *m, *ret = (obj_t*)core;

    // Make the first 'core' optional.
    if (strcmp(id, "core") == 0) return (obj_t*)core;
    if (strncmp(id, "core.", 5) == 0) id += 5;

    // Remove this once all the client code has been updated.
    if (strcmp(id, "observer") == 0) {
        LOG_D("Getting observer as a module is deprecated");
        LOG_D("Use attribute API instead");
        return &core->observer->obj;
    }

    while (*id) {
        end = strchr(id, '.') ?: id + strlen(id);
        len = end - id;
        DL_FOREACH(ret->children, m) {
            if (!m->id) continue;
            if (strncmp(m->id, id, len) == 0 && m->id[len] == '\0') {
                ret = m;
                id += len;
                if (*id == '.') id++;
                break;
            }
        }
        if (!m) return NULL;
    }
    return ret;
}

static int modules_sort_cmp(void *a, void *b)
{
    obj_t *at, *bt;
    at = a;
    bt = b;
    return cmp(module_get_render_order(at), module_get_render_order(bt));
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
    double mat[4][4] = MAT4_IDENTITY;
    projection_compute_fovs(core->proj, core->fov, aspect, &fovx, &fovy);
    projection_init(proj, core->proj, fovy,
                    core->win_size[0], core->win_size[1]);

    if (core->flip_view_vertical) {
        proj->flags |= PROJ_FLIP_VERTICAL;
        mat4_iscale(mat, 1, -1, 1);
    }
    if (core->flip_view_horizontal) {
        proj->flags |= PROJ_FLIP_HORIZONTAL;
        mat4_iscale(mat, -1, 1, 1);
    }
    mat4_mul(mat, proj->mat, proj->mat);
}

obj_t *core_get_obj_at(double x, double y, double max_dist)
{
    double pos[2] = {x, y};
    obj_t *obj;

    // First test the labels, and then the global shape area.
    obj = labels_get_obj_at(pos, 0);
    if (obj) return obj;
    obj = areas_lookup(core->areas, pos, max_dist);
    if (obj) return obj;
    return NULL;
}

// Set the default init values.
static void core_set_default(void)
{
    double tsl;
    observer_t *obs = core->observer;

    // Reset to Taipei.
    obj_set_attr(&obs->obj, "latitude", 25.066667 * DD2R);
    obj_set_attr(&obs->obj, "longitude", 121.516667 * DD2R);
    obj_set_attr(&obs->obj, "elevation", 0.0);
    obs->tt = utc2tt(unix_to_mjd(sys_get_unix_time()));

    // We approximate the pressure from the altitude and the sea level
    // temperature in K (See Astrophysical Quantities, C.W.Allen, 3rd edition,
    // section 52).
    tsl = 15 + 273.15;  // Let say we have a see level temp of 15 deg C.
    obs->pressure = 1013.25 * exp(-obs->hm / (29.3 * tsl));
    fader_init(&core->refraction, true);

    core->fov = 50 * DD2R;
    core->proj = PROJ_STEREOGRAPHIC;
    core->lwmax = 5000;
    core->clock = sys_get_unix_time();

    // Adjust those values to make the sky look good.
    core->star_linear_scale = 0.8;
    core->star_scale_screen_factor = 0.5;
    core->star_relative_scale = 1.1;
    core->bortle_index = 3;

    core->lwmax_min = 0.052;
    core->max_point_radius = 50.0;
    core->min_point_radius = 0.6;
    core->skip_point_radius = 0.25;
    core->show_hints_radius = 0.4;
    core->lwsky_average = 0.0001;  // Updated by atmosphere rendering
    core->exposure_scale = 2;
    core->tonemapper_p = 2.2;     // Setup using atmosphere as reference

    tonemapper_update(&core->tonemapper, core->tonemapper_p, 1, 1, core->lwmax);

    core->telescope_auto = true;
    core->mount_frame = FRAME_OBSERVED;

    core->time_animation.dst_utc = NAN;

    observer_update(core->observer, false);
}

static void on_progressbar(const char *id)
{
    module_changed((obj_t*)core, "progressbars");
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
    obj_t *m;

    // Why do we even need those attributes?
    assert(!isnan(win_w) && !isnan(win_h) && !isnan(pixel_scale));

    if (core) {
        // Already initialized.
        core_set_default();
        return;
    }
    texture_set_load_callback(NULL, texture_load_function);
    snprintf(cache_dir, sizeof(cache_dir), "%s/%s",
             sys_get_user_dir(), ".cache");
    request_init(cache_dir);

    core = (core_t*)obj_create("core", NULL);
    core->obj.id = "core";
    core->win_size[0] = win_w;
    core->win_size[1] = win_h;
    core->win_pixels_scale = pixel_scale;
    core->display_limit_mag = 99;

    core->observer = (observer_t*)obj_create("observer", NULL);
    core->observer->obj.id = "observer";

    for (module = obj_get_all_klasses(); module; module = module->next) {
        if (!(module->flags & OBJ_MODULE)) continue;
        m = module_add_new(&core->obj, module->id, NULL);
        m->id = module->id;
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
}

/*
 * Get a projection scaling factor in the Y direction.
 */
static double proj_get_scaling_y(const projection_t *proj)
{
    return 1 / fabs(proj->mat[1][1]);
}

EMSCRIPTEN_KEEPALIVE
void core_set_view_offset(double center_y_offset)
{
    double pix_angular_size, scaling_y;
    projection_t proj;

    core_get_proj(&proj);
    assert(proj.window_size[1]);
    scaling_y = proj_get_scaling_y(&proj);
    pix_angular_size = 1.0 * scaling_y / proj.window_size[1] * 2;
    core->observer->view_offset_alt = -center_y_offset * pix_angular_size;
}

// Smoothly update the observer pressure for refraction effect.
static void update_refraction(double dt, bool on)
{
    double tsl;
    observer_t *obs = core->observer;
    core->refraction.target = on;
    fader_update(&core->refraction, dt);
    tsl = 15 + 273.15;  // Let say we have a see level temp of 15 deg C.
    obs->pressure = 1013.25 * exp(-obs->hm / (29.3 * tsl));
    obs->pressure *= core->refraction.value;
}

EMSCRIPTEN_KEEPALIVE
int core_update(void)
{
    bool atm_visible;
    double lwmax, now, dt;
    int r;
    obj_t *atm, *module;
    task_t *task, *task_tmp;

    now = sys_get_unix_time();
    dt = now - core->clock;
    dt = max(dt, 0.001); // Prevent bug in case the clock goes backward.
    core->clock = now;

    atm = core_get_module("atmosphere");
    assert(atm);
    obj_get_attr(atm, "visible", &atm_visible);
    update_refraction(dt, atm_visible);
    observer_update(core->observer, true);
    // Update telescope according to the fov.
    if (core->telescope_auto)
        telescope_auto(&core->telescope, core->fov);
    progressbar_update();

    // Update eye adaptation.
    if (core->fast_adaptation && core->lwmax > core->tonemapper.lwmax) {
        lwmax = core->lwmax;
    } else {
        lwmax = exp(logf(core->tonemapper.lwmax) +
                    (logf(core->lwmax) - logf(core->tonemapper.lwmax)) *
                    min(0.16 * dt / 0.01666, 0.5));
    }

    tonemapper_update(&core->tonemapper, core->tonemapper_p, -1,
                      core->exposure_scale, lwmax);
    core->lwmax = core->lwmax_min; // Reset for next frame.

    // Adjust star linear scale in function of screen pixel size
    // It ranges from 0.7 for a small screen to 1.5 for large screens
    double screen_s = min(core->win_size[0], core->win_size[1]);
    double fact = screen_s / 600;
    core->star_scale_screen_factor = min(max(0.7, fact), 1.5);

    // Defined in navigation.c
    core_update_observer(dt);

    DL_FOREACH_SAFE(core->tasks, task, task_tmp) {
        if (task->fun(task, dt) != 0) {
            DL_DELETE(core->tasks, task);
            free(task);
        }
    }

    DL_SORT(core->obj.children, modules_sort_cmp);
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->update) {
            r = module->klass->update(module, dt);
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
    double direction[4];
    ls = core_get_module("landscapes");
    obj_get_attr(ls, "visible", &visible);
    if (!visible) return false;

    // If we look down, it means the landscape is semi transparent, and so
    // we can't clip.
    // XXX: we should let the lanscape module notify the core that it hides
    // the stars instead.
    convert_frame(core->observer, FRAME_VIEW, FRAME_OBSERVED, true,
                  VEC(0, 0, -1), direction);
    return direction[2] >= 0;
}

// Get point for mag without any radius lower limit.
static void core_get_point_for_mag_(
        double mag, double *radius, double *luminance)
{
    double ld;

    // Poor man's extinction function taking the Bortle index into account
    double s_linear = (core->star_linear_scale + 3.0 / 11.0 -
                       core->bortle_index / 11.0) *
                       core->star_scale_screen_factor;
    double s_relative = core->star_relative_scale;
    // Compute apparent luminance, i.e. the luminance percieved by the eye
    // when looking in the telescope eyepiece.
    double lum_apparent = core_mag_to_lum_apparent(mag, 0);
    // Apply eye adaptation.
    ld = tonemapper_map(&core->tonemapper, lum_apparent);
    if (ld < 0) ld = 0; // Prevent math error.
    // Compute r, using both manual adjustement factors.
    *radius = s_linear * pow(ld, s_relative / 2.0);
    if (luminance) *luminance = clamp(ld, 0, 1);
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
bool core_get_point_for_mag(double mag, double *radius, double *luminance)
{
    double ld, r;
    double r_min = core->min_point_radius;
    const double r_skip = core->skip_point_radius;

    // Fix aliasing on low res screen
    if (r_min * core->win_pixels_scale < 1.0)
        r_min = 1.0;

    // Get radius and luminance without any constraint on the radius.
    core_get_point_for_mag_(mag, &r, &ld);

    // If the radius is really too small, we don't render the star.
    if (r < r_skip) {
        *radius = 0;
        if (luminance) *luminance = 0;
        return false;
    }

    // If the radius is too small, we adjust the luminance.
    if (r > 0 && r < r_min) {
        ld *= pow((r - r_skip) / (r_min - r_skip), 2);
        r = r_min;
    }

    ld = pow(ld, 1 / 2.2); // Gama correction.
    // Saturate radius after a certain point.
    // XXX: make it smooth.
    r = min(r, core->max_point_radius);
    *radius = r;
    if (luminance) *luminance = clamp(ld, 0, 1);
    return true;
}

double core_get_hints_mag_offset(const double win_pos[2])
{
    const double center[2] = {core->win_size[0] / 2, core->win_size[1] / 2};
    const double var = 60;
    double x;
    // Gaussian distribution at center of screen with max value
    // core->center_hints_mag_offset and hard-codded variance.
    if (core->center_hints_mag_offset == 0)
        return 0;
    x = vec2_dist(win_pos, center);
    return core->center_hints_mag_offset * exp(-x / (2 * var));
}

/*
 * Function: compute_vmag_for_radius
 * Compute the vmag for a given screen radius.
 *
 * Parameters:
 *   target_r   - Radius in windows pixel unit.
 *
 * Return:
 *   A vmag such as core_get_point_for_vmag_ returns the target radius.
 */
static double compute_vmag_for_radius(double target_r)
{
    // Compute by dichotomy.
    const int max_iter = 32;
    double m = 0.0, m1 = -192.0, m2 = 64.0;
    double r;
    const double delta = 0.001;
    int i;

    /*
     * If the lowest magnitude is already too high for the target, then
     * just return it, since the dichotomy algo will fail anyway.  It would be
     * nice to use a better algo to avoid that at some point.
     */
    core_get_point_for_mag_(m1, &r, NULL);
    if (r < target_r) return m1;

    for (i = 0; i < max_iter; i++) {
        m = (m1 + m2) / 2;
        core_get_point_for_mag_(m, &r, NULL);
        if (fabs(r - target_r) < delta) return m;
        *(r > target_r ? &m1 : &m2) = m;
    }
    if (i >= max_iter) {
        LOG_D("Too many iterations! target_r: %f, mag:%f -> r:%f",
              target_r, m, r);
    }
    return m;
}

// Debug function to check for errors in the projections.
static void render_proj_markers(const painter_t *painter_)
{
    painter_t painter = *painter_;
    double lon, lat;
    double p[4], p_win[4];
    double r[4][4] = MAT4_IDENTITY;
    painter.color[1] = 0;
    mat4_rx(M_PI / 2, r, r);
    mat4_rz(M_PI / 2, r, r);
    for (lon = 0; lon < 360; lon += 10)
    for (lat = -90; lat < 90; lat += 10) {
        painter.color[0] = lon / 360.;
        eraS2c(lon * DD2R, lat * DD2R, p);
        p[3] = 0;
        mat4_mul_vec4(r, p, p);
        project_to_win(painter.proj, p, p_win);
        paint_2d_ellipse(&painter, NULL, 0, p_win, VEC(2, 2), NULL);

        unproject(painter.proj, p_win, p);
        project_to_win(painter.proj, p, p_win);
        paint_2d_ellipse(&painter, NULL, 0, p_win, VEC(4, 4), NULL);
    }
}


EMSCRIPTEN_KEEPALIVE
int core_render(double win_w, double win_h, double pixel_scale)
{
    obj_t *module;
    projection_t proj;
    double max_vmag, hints_vmag;

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
    max_vmag = compute_vmag_for_radius(core->skip_point_radius);
    hints_vmag = compute_vmag_for_radius(core->show_hints_radius);

    fps_tick(&core->fps, sys_get_unix_time());
    module_changed(&core->obj, "fps");

    if (!core->rend)
        core->rend = render_create();
    labels_reset();

    painter_t painter = {
        .rend = core->rend,
        .obs = core->observer,
        .fb_size = {win_w * pixel_scale, win_h * pixel_scale},
        .pixel_scale = pixel_scale,
        .proj = &proj,
        .stars_limit_mag = max_vmag,
        .hints_limit_mag = hints_vmag,
        .hard_limit_mag = core->display_limit_mag,
        .points_halo = 7.0,
        .color = {1.0, 1.0, 1.0, 1.0},
        .contrast = 1.0,
        .lines.width = 1.0,
        .lines.glow = 0.2,
        .flags = (is_below_horizon_hidden() ? PAINTER_HIDE_BELOW_HORIZON : 0),
    };
    painter_update_clip_info(&painter);
    paint_prepare(&painter, win_w, win_h, pixel_scale);

    DL_FOREACH(core->obj.children, module) {
        obj_render(module, &painter);
    }

    // Render the viewport cap for debugging.
    if ((0)) {
        paint_cap(&painter, FRAME_ICRF,
                  painter.clip_info[FRAME_ICRF].bounding_cap);
    }

    if ((0)) {
        render_proj_markers(&painter);
    }

    // Flush all rendering pipeline
    paint_finish(&painter);

    assert(bck.obs.tt == core->observer->tt);
    assert(bck.obs.yaw == core->observer->yaw);
    assert(bck.obs.pitch == core->observer->pitch);
    assert(bck.fov == core->fov);

    // Do post render (e.g. for GUI)
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->post_render)
            module->klass->post_render(module, &painter);
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
void core_on_mouse(int id, int state, double x, double y, int buttons)
{
    obj_t *module;
    int r;
    DL_FOREACH(core->obj.children, module) {
        if (!module->klass->on_mouse) continue;
        r = module->klass->on_mouse(module, id, state, x, y, buttons);
        if (r == 0) return;
    }
}

EMSCRIPTEN_KEEPALIVE
void core_on_pinch(int state, double x, double y, double scale,
                   int points_count)
{
    obj_t *module;
    int r;
    DL_FOREACH(core->obj.children, module) {
        if (!module->klass->on_pinch) continue;
        r = module->klass->on_pinch(module, state, x, y, scale, points_count);
        if (r == 0) return;
    }
}

EMSCRIPTEN_KEEPALIVE
void core_on_key(int key, int action)
{
    static char *SC[][3] = {
        {"A", "core.atmosphere"},
        {"G", "core.landscapes"},
        {"F", "core.landscapes", "fog_visible"},
        {"C", "core.constellations", "lines_visible"},
        {"C", "core.constellations", "labels_visible"},
        {"R", "core.constellations", "images_visible"},
        {"Z", "core.lines.azimuthal"},
        {"E", "core.lines.equatorial_jnow"},
        {"M", "core.lines.meridian"},
        {"N", "core.dsos"},
        {"D", "core.dss"},
        {"S", "core.stars"},
        {"T", "core", "test"},
    };
    int i;
    bool v;
    obj_t *module;
    const char *attr;
    char buf[128];

    core->inputs.keys[key] = (action != KEY_ACTION_UP);

    if (core->gui_want_capture_mouse) return;
    if (action != KEY_ACTION_DOWN) return;

    for (i = 0; i < ARRAY_SIZE(SC); i++) {
        if (SC[i][0][0] == key) {
            attr = SC[i][2] ?: "visible";
            module = core_get_module(SC[i][1]);
            obj_get_attr(module, attr, &v);
            obj_set_attr(module, attr, !v);
        }
    }
    if (key == ' ' && core->selection) {
        LOG_D("lock to %s", obj_get_name(core->selection, buf, sizeof(buf)));
        obj_set_attr(&core->obj, "lock", core->selection);
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
    obj_t *module;
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->on_zoom) {
            if (module->klass->on_zoom(module, k, x, y) == 0)
                return;
        };
    }
}

double core_mag_to_illuminance(double vmag)
{
    /*
     * Compute illuminance (in lux = lum/m² = cd.sr/m²)
     *
     * L = 10.7646e4 * 10^(-0.4 * S)  | S: vmag/arcsec², L: luminance (cd/m²)
     * E = L * A                      | E: lux (= cd.sr/m²), A: sr, L: cd/m²
     * => E = 10.7646e4 / R2AS^2 * 10^(-0.4 * m)
     *
     * Same formula at https://en.wikipedia.org/wiki/Illuminance
     */
    return 10.7646e4 / (ERFA_DR2AS * ERFA_DR2AS) * exp10(-0.4 * vmag);
}

double core_mag_to_surf_brightness(double mag, double surf)
{
    // From https://en.wikipedia.org/wiki/Surface_brightness
    // S = m + 2.5 * log10(A)       | S: Surface Brightness (vmag/arcsec²)
    //                              | A: visual area of source (arcsec²)
    //                              | m: source magnitude integrated over A
    return mag + 2.5 * log10(surf * (ERFA_DR2AS * ERFA_DR2AS));
}

double core_illuminance_to_lum_apparent(double illum, double surf)
{
    const telescope_t *tel = &core->telescope;
    /*
     * Apply optic from telescope light grasp (Gl).
     *
     * E' = E * Gl
     */
    illum *= tel->light_grasp;

    // Scale the object apparent area to transform it into apparent
    // area in the eyepiece.
    surf *= tel->magnification * tel->magnification;

    // Assumes point objects (unresolved stars) has a radius of 2.5 arcmin,
    // roughly corresponding to human eye Point Spread Function.
    const double pr = 2.5 / 60 * DD2R;
    const double min_point_area = M_PI * pr * pr;

    surf = max(surf, min_point_area);

    /*
     * Compute luminance
     *
     * L = E / A   |  in sr, E in lux, L in cd/m²
     */
    return illum / surf;
}

double core_surf_brightness_to_lum_apparent(double surf_brightness)
{
    const telescope_t *tel = &core->telescope;
    double lum = 10.7646e4 * exp10(-0.4 * surf_brightness);
    return lum * tel->light_grasp / (tel->magnification * tel->magnification);
}

double core_mag_to_lum_apparent(double mag, double surf)
{
    double illum = core_mag_to_illuminance(mag);
    return core_illuminance_to_lum_apparent(illum, surf);
}


/*
 * Function: core_get_apparent_angle_for_point
 * Get angular radius of a round object from its pixel radius on screen.
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
    const double win_h = proj->window_size[1];
    const double scaling_y = proj_get_scaling_y(proj);
    return r * scaling_y / win_h * 2;
}

/*
 * Function: core_get_point_for_apparent_angle
 * Get the pixel radius of a circle with a given apparent angle
 *
 * This is the inverse of <core_get_apparent_angle_for_point>
 *
 * Parameters:
 *   proj   - The projection used.
 *   angle  - Apparent angle (rad).
 *
 * Return:
 *   Point radius in window pixel.
 */
double core_get_point_for_apparent_angle(const projection_t *proj,
                                         double angle)
{
    const double win_h = proj->window_size[1];
    const double scaling_y = proj_get_scaling_y(proj);
    return angle / scaling_y * win_h / 2;
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
    // E = 10.8e4 / R2AS^2 * 10^(-0.4 * m)
    // m: source magnitude integrated over A
    // E: lux (= cd.sr/m²), A: sr, L: cd/m²
    lf = 10.8e4 / (ERFA_DR2AS * ERFA_DR2AS) * pow(10, -0.4 * vmag);

    // L = E / A
    lum = lf / (M_PI * r * r);

    // Radius as seen from the observer eye in the eye piece
    r2 = r * core->telescope.magnification;

    // Make sure the observed radius can't be smaller as point source radius
    // assumed to be 2.5 arcmin (see formula in above function)
    r2 = max(r2, 2.5 / 60 * DD2R);

    // The following 3 lines are 100% ad-hoc formulas adjusted so that:
    // - the moon should render all but bright stars invisible
    // - mars should hide most stars and DSS when zoomed

    // Modulate final luminance by the ratio of area covered by the object
    // in eyepiece field of view (which is always assumed to be 60 deg FOV).
    lum *= pow(r2 / (60 * DD2R), 1.2);
    lum = pow(lum, 0.33);
    lum /= 300;

    lum *= smoothstep(core->fov * 0.75, 0, max(0, sep - r));
    core_report_luminance_in_fov(lum * 7.0, false);
}

void core_report_luminance_in_fov(double lum, bool fast_adaptation)
{
    if (lum > core->lwmax) {
        core->fast_adaptation = fast_adaptation;
        core->lwmax = lum;
    }
}

EMSCRIPTEN_KEEPALIVE
void core_lookat(const double *pos, double duration)
{
    double az, al, now;
    typeof(core->target) *anim = &core->target;

    // Direct lookat.
    if (duration == 0.0) {
        eraC2s(pos, &core->observer->yaw, &core->observer->pitch);
        memset(anim, 0, sizeof(*anim));
        return;
    }

    now = sys_get_unix_time();
    quat_set_identity(anim->src_q);
    quat_rz(core->observer->yaw, anim->src_q, anim->src_q);
    quat_ry(-core->observer->pitch, anim->src_q, anim->src_q);

    eraC2s((double*)pos, &az, &al);
    quat_set_identity(anim->dst_q);
    quat_rz(az, anim->dst_q, anim->dst_q);
    quat_ry(-al, anim->dst_q, anim->dst_q);

    anim->src_time = now;
    anim->dst_time = now + duration;
}

EMSCRIPTEN_KEEPALIVE
void core_point_and_lock(obj_t *target, double duration)
{
    double v[4];
    if (core->target.lock == target) return;
    observer_update(core->observer, true);
    obj_set_attr(&core->obj, "lock", target);
    obj_get_pos(core->target.lock, core->observer, FRAME_OBSERVED, v);
    core_lookat(v, duration);
    core->target.move_to_lock = true;
}

EMSCRIPTEN_KEEPALIVE
void core_zoomto(double fov, double duration)
{
    double s, now;
    projection_t proj;

    now = sys_get_unix_time();
    core_get_proj(&proj);
    if (fov > proj.klass->max_ui_fov)
        fov = proj.klass->max_ui_fov;

    // Direct lookat.
    if (duration == 0.0) {
        core->fov = fov;
        return;
    }

    typeof(core->fov_animation)* anim = &core->fov_animation;
    if (now > anim->src_time && now <= anim->dst_time) {
        // We request a new animation while another one is still on going
        if (fov == anim->dst_fov) {
            // Same animation is going on, just finish it
            return;
        }
        // We are looking for a new set of zoom parameters so that:
        // - we preserve the current zoom level
        // - the remaining animation time is equal to the new duration
        s = smoothstep(anim->src_time, now + duration, now);
        anim->dst_time = now + duration;
        anim->dst_fov = fov;
        anim->src_fov = (core->fov - s * fov) / (1 - s);
        return;
    }

    anim->src_fov = core->fov;
    anim->dst_fov = fov;
    anim->src_time = now;
    anim->dst_time = now + duration;
}

EMSCRIPTEN_KEEPALIVE
void core_set_time(double utc, double duration)
{
    double tt, speed, now;
    typeof(core->time_animation) *anim = &core->time_animation;

    tt = utc2tt(utc);
    if (duration == 0.0) {
        obj_set_attr(&core->observer->obj, "tt", tt);
        memset(anim, 0, sizeof(*anim));
        return;
    }
    now = sys_get_unix_time();
    anim->src_tt = core->observer->tt;
    anim->dst_tt = tt;
    anim->dst_utc = utc;
    anim->src_time = now;
    anim->dst_time = now + duration;

    // Determine the animation mode (normal or 'smart').  If the animation
    // moves at more than a few days per seconds, use the 'smart' mode.
    speed = fabs(anim->dst_tt - anim->src_tt) / duration;
    anim->mode = speed > 5 ? 1 : 0;

    module_changed((obj_t*)core, "time_animation_target");
}

// Return a static string representation of a an object type id.
EMSCRIPTEN_KEEPALIVE
const char *otype_to_str(const char *otype)
{
    const char *res = otype_get_str(otype);
    return res ? res : "";
}

void core_add_task(int (*fun)(task_t *task, double dt), void *user)
{
    task_t *task = calloc(1, sizeof(*task));
    task->fun = fun;
    task->user = user;
    DL_APPEND(core->tasks, task);
}

static void on_designation(const obj_t *obj, void *user, const char *dsgn)
{
    const char *query = USER_GET(user, 0);
    obj_t **result = USER_GET(user, 1);
    if (*result) return;
    if (strcasecmp(query, dsgn) == 0) {
        *result = obj_retain(obj);
    }
}

static int on_search(void *user, obj_t *obj)
{
    obj_t **result = USER_GET(user, 1);
    if (*result) return 1;
    obj_get_designations(obj, user, on_designation);
    return (*result) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
obj_t *core_search(const char *query)
{
    obj_t *module, *ret = NULL;
    DL_FOREACH(core->obj.children, module) {
        module_list_objs(module, NAN, 0, NULL, USER_PASS(query, &ret),
                         on_search);
    }
    return ret;
}

static obj_klass_t core_klass = {
    .id = "core",
    .size = sizeof(core_t),
    .flags = OBJ_IN_JSON_TREE,
    .attributes = (attribute_t[]) {
        PROPERTY(observer, TYPE_OBJ, MEMBER(core_t, observer)),
        PROPERTY(fov, TYPE_ANGLE, MEMBER(core_t, fov),
                 .on_changed = core_on_fov_changed),
        PROPERTY(projection, TYPE_INT, MEMBER(core_t, proj)),
        PROPERTY(selection, TYPE_OBJ, MEMBER(core_t, selection)),
        PROPERTY(lock, TYPE_OBJ, MEMBER(core_t, target.lock)),
        PROPERTY(progressbars, TYPE_JSON, .fn = core_fn_progressbars),
        PROPERTY(fps, TYPE_INT, MEMBER(core_t, fps.avg)),
        PROPERTY(clicks, TYPE_INT, MEMBER(core_t, clicks)),
        PROPERTY(zoom, TYPE_FLOAT, MEMBER(core_t, zoom)),
        PROPERTY(test, TYPE_BOOL, MEMBER(core_t, test)),
        PROPERTY(exposure_scale, TYPE_FLOAT, MEMBER(core_t, exposure_scale)),
        PROPERTY(star_linear_scale, TYPE_FLOAT,
                 MEMBER(core_t, star_linear_scale)),
        PROPERTY(star_relative_scale, TYPE_FLOAT,
                 MEMBER(core_t, star_relative_scale)),
        PROPERTY(bortle_index, TYPE_INT, MEMBER(core_t, bortle_index)),
        PROPERTY(tonemapper_p, TYPE_FLOAT, MEMBER(core_t, tonemapper_p)),
        PROPERTY(display_limit_mag, TYPE_FLOAT,
                 MEMBER(core_t, display_limit_mag)),
        PROPERTY(center_hints_mag_offset, TYPE_FLOAT,
                 MEMBER(core_t, center_hints_mag_offset)),
        PROPERTY(flip_view_vertical, TYPE_BOOL,
                 MEMBER(core_t, flip_view_vertical)),
        PROPERTY(flip_view_horizontal, TYPE_BOOL,
                 MEMBER(core_t, flip_view_horizontal)),
        PROPERTY(mount_frame, TYPE_ENUM, MEMBER(core_t, mount_frame)),
        PROPERTY(on_click, TYPE_FUNC, MEMBER(core_t, on_click)),
        PROPERTY(on_rect, TYPE_FUNC, MEMBER(core_t, on_rect)),
        PROPERTY(time_animation_target, TYPE_MJD,
                 MEMBER(core_t, time_animation.dst_utc)),
        PROPERTY(time_speed, TYPE_FLOAT, MEMBER(core_t, time_speed)),
        PROPERTY(y_offset, TYPE_FLOAT, MEMBER(core_t, y_offset)),
        PROPERTY(hide_selection_label, TYPE_BOOL,
                 MEMBER(core_t, hide_selection_label)),
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
    obj_t *obs = &core->observer->obj;
    obj_get_attr(obs, "elevation", &v); assert(v == 10.0);
    obj_set_attr(obs, "longitude", 1.0);
    assert(core->observer->elong == 1.0);
    obj_get_attr(obs, "longitude", &v); assert(v == 1.0);
    obj_get_attr(obs, "utc", &v);
}

static void test_basic(void)
{
    obj_t *obj;
    // Test obj_get.
    obj = core_search("NAME Sun");
    assert(obj);
    obj_release(obj);
}

static void test_info(void)
{
    obj_t *obj;
    double vmag;
    obj = core_get_planet(599); // Jupiter.
    assert(obj);
    obj_get_info(obj, core->observer, INFO_VMAG, &vmag);
}

TEST_REGISTER(NULL, test_core, TEST_AUTO);
TEST_REGISTER(NULL, test_vec, TEST_AUTO);
TEST_REGISTER(NULL, test_basic, TEST_AUTO);
TEST_REGISTER(NULL, test_info, TEST_AUTO);

#endif
