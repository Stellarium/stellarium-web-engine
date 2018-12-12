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

static int selection_get_choices(
        int (*f)(const char* name, uint64_t oid, void *user),
        void *user)
{
    char cat_up[8];
    char buff[128];
    const char *cat, *value;
    int nb = 0, r;
    uint64_t oid;
    IDENTIFIERS_ITER(0, NULL, &oid, NULL, &cat, &value,
                     NULL, NULL, NULL, NULL) {
        if      (oid_is_catalog(oid, "CITY")) continue;
        if      (str_equ(cat, "NAME"))  r = f(value, oid, user);
        else if (str_equ(cat, "BAYER")) r = f(value, oid, user);
        else {
            str_to_upper(cat, cat_up);
            sprintf(buff, "%s %s", cat_up, value);
            r = f(buff, oid, user);
        }
        nb++;
        if (r) break;
    }
    return nb;
}

static void core_on_fov_changed(obj_t *obj, const attribute_t *attr)
{
    // Maybe put this into projection class?
    const double PROJ_MAX_FOVS[] = {
        [PROJ_MERCATOR]         = 360 * DD2R,
        [PROJ_STEREOGRAPHIC]    = 220 * DD2R,
        [PROJ_PERSPECTIVE]      = 360 * DD2R,
    };
    // For the moment there is not point going further than 0.5°.
    core->fov = clamp(core->fov, 0.0001 * DD2R, PROJ_MAX_FOVS[core->proj]);
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
        if (strcmp(module->id, id) == 0) return module;
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
        if (strcmp(module->id, id) == 0) return module;
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
        nb += obj_list(module, obs, max_mag, hint, user, f);
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
        r = obj_add_data_source(module, url, type, args);
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

static int core_update(void);
static int core_update_direction(double dt);

// Convert mouse position to observed coordinates.
static void mouse_to_observed(double x, double y, double p[3])
{
    double rv2o[4][4];
    projection_t proj;
    double pos[4] = {x, y};

    mat4_invert(core->observer->ro2v, rv2o);
    projection_init(&proj, core->proj, core->fovx,
                    core->win_size[0], core->win_size[1]);
    // Convert to NDC coordinates.
    // Could be done in the projector?
    pos[0] = pos[0] / core->win_size[0] * 2 - 1;
    pos[1] = -1 * (pos[1] / core->win_size[1] * 2 - 1);
    project(&proj, PROJ_BACKWARD, 4, pos, pos);
    mat4_mul_vec3(rv2o, pos, pos);
    vec3_copy(pos, p);
}

static int core_pan(const gesture_t *g, void *user)
{
    double sal, saz, dal, daz;
    double pos[3];
    static double start_pos[3];

    mouse_to_observed(g->pos[0], g->pos[1], pos);
    if (g->state == GESTURE_BEGIN)
        vec3_copy(pos, start_pos);

    eraC2s(start_pos, &saz, &sal);
    eraC2s(pos, &daz, &dal);
    core->observer->azimuth += (saz - daz);
    core->observer->altitude += (sal - dal);
    core->observer->altitude = clamp(core->observer->altitude,
                                     -M_PI / 2, +M_PI / 2);
    core->fast_mode = true;
    obj_set_attr(&core->obj, "lock", "p", NULL);
    observer_update(core->observer, true);
    // Notify the changes.
    obj_changed(&core->observer->obj, "altitude");
    obj_changed(&core->observer->obj, "azimuth");
    return 0;
}

static obj_t *get_obj_at(double x, double y, double max_dist)
{
    double pos[2] = {x, y};
    uint64_t oid, hint;
    if (!areas_lookup(core->areas, pos, max_dist, &oid, &hint))
        return NULL;
    if (!oid) return NULL;
    return obj_get_by_oid(NULL, oid, hint);
}

static int core_click(const gesture_t *g, void *user)
{
    obj_t *obj;
    if (!core->ignore_clicks) {
        obj = get_obj_at(g->pos[0], g->pos[1], 18);
        obj_set_attr(&core->obj, "selection", "p", obj);
        obj_release(obj);
    }
    core->clicks++;
    obj_changed((obj_t*)core, "clicks");
    return 0;
}

static int core_hover(const gesture_t *g, void *user)
{
    obj_t *obj;
    obj = get_obj_at(g->pos[0], g->pos[1], 18);
    obj_set_attr(&core->obj, "hovered", "p", obj);
    obj_release(obj);
    return 0;
}

static int core_pinch(const gesture_t *gest, void *user)
{
    static double start_fov = 0;
    if (gest->state == GESTURE_BEGIN) {
        start_fov = core->fov;
    }
    core->fov = start_fov / gest->pinch;
    obj_changed((obj_t*)core, "fov");
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void core_add_default_sources(void)
{
    #define BASE_URL "https://data.stellarium.org/"
    asset_set_alias(BASE_URL "landscapes", "asset://landscapes");
    obj_add_data_source(NULL, BASE_URL "landscapes", NULL, NULL);

    // Skyculture.  We load the western culture immediately so we don't have
    // to wait to parse the online directory index.json file.
    asset_set_alias(BASE_URL "skycultures", "asset://skycultures");
    obj_add_data_source(NULL, BASE_URL "skycultures/western", "skyculture",
                        NULL);
    obj_add_data_source(NULL, BASE_URL "skycultures", NULL, NULL);

    // HiPS surveys.
    obj_add_data_source(NULL, BASE_URL "surveys", "hipslist", NULL);
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
    core->utc_offset = sys_get_utc_offset() / 60.0;

    core->proj = PROJ_STEREOGRAPHIC;

    core->star_relative_scale = 2;
    core->star_linear_scale = 2;
    core->telescope_auto = true;
    core->lwa_coef = 1.0;

    core_update();
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
void core_init(void)
{
    char cache_dir[1024];
    obj_klass_t *module;

    if (core) {
        // Already initialized.
        core_set_default();
        return;
    }
    texture_set_load_callback(NULL, texture_load_function);
    sprintf(cache_dir, "%s/%s", sys_get_user_dir(), ".cache");
    request_init(cache_dir);
    identifiers_init();

    font_init(asset_get_data("asset://font/DejaVuSans-small.ttf", NULL, NULL));
    core = (core_t*)obj_create("core", "core", NULL, NULL);
    core->win_pixels_scale = 1.0;
    obj_add_sub(&core->obj, "hints");
    core->hints_mag_max = NAN;

    core->observer = (observer_t*)obj_create("observer", "observer",
                                             (obj_t*)core, NULL);

    // Set initial word adaptation luminance to 5 kcd/m², full sunlight.
    core->lwa = core->lwa_target = 5000;
    core->tonemapper = tonemapper_create(86, 35, false);

    tonemapper_set_adaptation_luminance(core->tonemapper, core->lwa);

    core->gest_pan = (gesture_t) {
        .type = GESTURE_PAN,
        .callback = core_pan,
    };
    core->gest_click = (gesture_t) {
        .type = GESTURE_CLICK,
        .callback = core_click,
    };
    core->gest_hover = (gesture_t) {
        .type = GESTURE_HOVER,
        .callback = core_hover,
    };
    core->gest_pinch = (gesture_t) {
        .type = GESTURE_PINCH,
        .callback = core_pinch,
    };

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
}

static int core_update_direction(double dt)
{
    double v[4] = {1, 0, 0, 0}, q[4], t, az, al, vv[4];

    if (core->target.speed) {
        core->target.t += dt / core->target.speed;
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
        if (core->target.dst_fov) {
            core->fov = mix(core->target.src_fov, core->target.dst_fov, t);
            obj_changed(&core->obj, "fov");
        }
        if (core->target.t >= 1.0) {
            core->target.speed = 0.0;
            core->target.t = 0.0;
            core->target.dst_fov = 0.0;
            core->target.move_to_lock = false;
        }
        // Notify the changes.
        obj_changed(&core->observer->obj, "altitude");
        obj_changed(&core->observer->obj, "azimuth");
    }

    if (core->target.lock && !core->target.move_to_lock) {
        obj_get_pos_observed(core->target.lock, core->observer, v);
        eraC2s(v, &core->observer->azimuth, &core->observer->altitude);
    }

    return 1;
}

static int core_update(void)
{
    bool atm_visible;
    double aspect = core->win_size[0] / core->win_size[1];
    obj_t *atm;

    atm = core_get_module("atmosphere");
    assert(atm);
    obj_get_attr(atm, "visible", "b", &atm_visible);
    projection_compute_fovs(core->proj, core->fov, aspect,
                            &core->fovx, &core->fovy);
    observer_update(core->observer, true);
    // Update telescope according to the fov.
    if (core->telescope_auto)
        telescope_auto(&core->telescope, core->fov);
    progressbar_update();

    // Update eye adaptation.
    core->lwa = exp(log(core->lwa) +
                (log(core->lwa_target * core->lwa_coef) -
                 log(core->lwa)) * 0.1);
    tonemapper_set_adaptation_luminance(core->tonemapper, core->lwa);
    core->lwa_target = 0.001; // Reset for next frame.

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
 * Compute the magnitude a stars need to have to have a given radius on screen.
 *
 * Parameters:
 *   target     - The target radius (pixel).
 */
static double compute_vmag_for_radius(double target)
{
    // Compute by dichotomy.
    const int max_iter = 32;
    double m = 0, m1 = -128.0, m2 = 128.0;
    double r, l;
    int i;
    for (i = 0; i < max_iter; i++) {
        m = (m1 + m2) / 2;
        core_get_point_for_mag(m, &r, &l);
        if (r && fabs(r - target) < 0.1) return m;
        *((r > target) ? &m1 : &m2) = m;
    }
    if (i >= max_iter) LOG_W("Too many iterations! (%f)", m);
    return m;
}


EMSCRIPTEN_KEEPALIVE
int core_render(double win_w, double win_h, double pixel_scale)
{
    obj_t *module;
    projection_t proj;
    double t;
    double dt = 1.0 / 16.0;
    int r;
    bool updated = false, cst_visible;
    double max_vmag;


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
    core->win_size[0] = win_w;
    core->win_size[1] = win_h;
    core->win_pixels_scale = pixel_scale;
    labels_reset();
    r = core_update();
    if (r) updated = true;

    projection_init(&proj, core->proj, core->fovx, win_w, win_h);

    const double ZOOM_FACTOR = 1.05;
    const double MOVE_SPEED  = 1 * DD2R;

    if (core->inputs.keys[KEY_RIGHT])
        core->observer->azimuth += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_LEFT])
        core->observer->azimuth -= MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_UP])
        core->observer->altitude += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_DOWN])
        core->observer->altitude -= MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_PAGE_UP])
        core->fov /= ZOOM_FACTOR;
    if (core->inputs.keys[KEY_PAGE_DOWN])
        core->fov *= ZOOM_FACTOR;

    // Continuous zoom.
    if (core->zoom) {
        core->fov *= pow(ZOOM_FACTOR, -core->zoom);
        if (core->fov > proj.max_fov)
            core->fov = proj.max_fov;
        obj_changed((obj_t*)core, "fov");
    }

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
        .mag_max = max_vmag,
        .hint_mag_max = (!isnan(core->hints_mag_max)) ? core->hints_mag_max :
                        compute_vmag_for_radius(1),
        .label_mag_max = compute_vmag_for_radius(2),
        .points_smoothness = 0.75,
        .color = {1.0, 1.0, 1.0, 1.0},
        .contrast = 1.0,
        .lines_width = 1.0,
        .flags = (core->fast_mode ? PAINTER_FAST_MODE : 0) |
            (is_below_horizon_hidden() ? PAINTER_HIDE_BELOW_HORIZON : 0) |
            (cst_visible ? PAINTER_SHOW_BAYER_LABELS : 0),
    };

    r = core_update_direction(dt);
    if (r) updated = true;

    DL_SORT(core->obj.children, modules_sort_cmp);
    DL_FOREACH(core->obj.children, module) {
        if (module->klass->update) {
            r = module->klass->update(module, core->observer, dt);
            if (r < 0) LOG_E("Error updating module '%s'", module->id);
            if (r == 1) updated = true;
        }
    }

    paint_prepare(&painter, win_w, win_h, pixel_scale);
    DL_FOREACH(core->obj.children, module) {
        obj_render(module, &painter);
    }

    paint_finish(&painter);
    if (core->fast_mode) updated = true;
    core->fast_mode = false;
    return updated ? 1 : 0;
}

static int get_touch_index(int id)
{
    int i;
    assert(id != 0);
    for (i = 0; i < ARRAY_SIZE(core->inputs.touches); i++) {
        if (core->inputs.touches[i].id == id) return i;
    }
    // Create new touch.
    for (i = 0; i < ARRAY_SIZE(core->inputs.touches); i++) {
        if (!core->inputs.touches[i].id) {
            core->inputs.touches[i].id = id;
            return i;
        }
    }
    return -1; // No more space.
}

EMSCRIPTEN_KEEPALIVE
void core_on_mouse(int id, int state, double x, double y)
{
    id = get_touch_index(id + 1);
    if (id == -1) return;
    if (state == -1) state = core->inputs.touches[id].down[0];
    if (state == 0) core->inputs.touches[id].id = 0; // Remove.
    core->inputs.touches[id].pos[0] = x;
    core->inputs.touches[id].pos[1] = y;
    core->inputs.touches[id].down[0] = state == 1;
    if (core->gui_want_capture_mouse) return;
    gesture_t *gs[] = {&core->gest_pan, &core->gest_pinch,
                       &core->gest_click, &core->gest_hover};
    gesture_on_mouse(4, gs, id, state, x, y);
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
        LOG_D("lock to %s", obj_get_name(core->selection));
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

    mouse_to_observed(x, y, pos_start);
    obj_get_attr(&core->obj, "fov", "f", &fov);
    fov /= k;
    obj_set_attr(&core->obj, "fov", "f", fov);
    mouse_to_observed(x, y, pos_end);

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
 *   radius    - Output radius (rad).  Note: the fov is already taken into
 *               account in the output.
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
    const double r_min = 0.5;

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
    ld = tonemapper_map_log10(core->tonemapper, log_lw);
    if (ld < 0) ld = 0; // Prevent math error.

    // Extra scale if the telescope magnification is not enough to reach the
    // current zoom level.
    s = (foveye / core->fov) / tel->magnification;
    // Compute r, using both manual adjustement factors.
    r = s_linear * pow(ld, s_relative / 2.0) * s;

    // If the radius is really too small, we don't render the star.
    if (r < r_min / 4) {
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
    r = min(r, 8.0);
    *radius = r;
    if (luminance) *luminance = clamp(ld, 0, 1);
}


double core_get_apparent_angle_for_point(const painter_t *painter, double r)
{
    const double win_w = painter->fb_size[0] / painter->pixel_scale;
    return r * painter->proj->scaling[0] / win_w * 2;
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
    core->lwa_target = max(core->lwa_target, lum);
    if (fast_adaptation && core->lwa_target > core->lwa) {
        core->lwa = core->lwa_target;
        tonemapper_set_adaptation_luminance(core->tonemapper, core->lwa);
    }
}

static int do_core_lookat(double* pos, double speed, double fov) {
    double az, al;

    // Direct lookat.
    if (speed == 0.0) {
        eraC2s(pos, &core->observer->azimuth, &core->observer->altitude);
        if (fov) core->fov = fov;
        return 0;
    }

    quat_set_identity(core->target.src_q);
    quat_rz(core->observer->azimuth, core->target.src_q, core->target.src_q);
    quat_ry(-core->observer->altitude, core->target.src_q, core->target.src_q);

    eraC2s((double*)pos, &az, &al);
    quat_set_identity(core->target.dst_q);
    quat_rz(az, core->target.dst_q, core->target.dst_q);
    quat_ry(-al, core->target.dst_q, core->target.dst_q);

    core->target.src_fov = core->fov;
    core->target.dst_fov = fov;
    core->target.speed = speed;
    core->target.t = 0.0;
    core->fast_mode = true;
    return 0;
}

static json_value *core_point_and_lock(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    double v[4], speed = 1.0, fov = 0.0;
    obj_t* target_obj;
    args_get(args, "target", 1, "p", "obj", &target_obj);
    args_get(args, "speed", 2, "f", NULL, &speed);
    args_get(args, "fov", 3, "f", NULL, &fov);

    obj_set_attr(&core->obj, "lock", "p", target_obj);

    obj_get_pos_observed(core->target.lock, core->observer, v);
    do_core_lookat(v, speed, fov ? fov : core->fov);
    core->target.move_to_lock = true;
    return NULL;
}

static json_value *core_lookat(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    // XXX find a better way to create a rot quaternion from a direction?
    double speed = 1.0, pos[3], fov = 0.0;

    args_get(args, "target", 1, "v3", "azalt", pos);
    args_get(args, "speed", 2, "f", NULL, &speed);
    args_get(args, "fov", 3, "f", NULL, &fov);

    do_core_lookat(pos, speed, fov);
    return NULL;
}

// Return a static string representation of a an object type id.
EMSCRIPTEN_KEEPALIVE
const char *type_to_str(const char type[4])
{
    const char *explanation;
    if (otypes_lookup(type, NULL, &explanation, NULL) == 0)
        return explanation;

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
                 .hint = "obj", .choices = selection_get_choices),
        PROPERTY("lock", "p", MEMBER(core_t, target.lock),
                 .hint = "obj"),
        PROPERTY("hovered", "p", MEMBER(core_t, hovered),
                 .hint = "obj"),
        PROPERTY("max_mag", "f", MEMBER(core_t, hints_mag_max),
                 .sub = "hints"),
        PROPERTY("progressbars", "json", .fn = core_fn_progressbars),
        PROPERTY("fps", "f", MEMBER(core_t, prof.fps)),
        PROPERTY("clicks", "d", MEMBER(core_t, clicks)),
        PROPERTY("ignore_clicks", "b", MEMBER(core_t, ignore_clicks)),
        PROPERTY("zoom", "f", MEMBER(core_t, zoom)),
        PROPERTY("test", "b", MEMBER(core_t, test)),
        FUNCTION("lookat", .fn = core_lookat),
        FUNCTION("point_and_lock", .fn = core_point_and_lock),
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
    core_init();
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
    core_init();
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
