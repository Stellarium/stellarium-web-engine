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

static int observer_init(obj_t *obj, json_value *args);
static obj_t *observer_clone(const obj_t *obj);
static void observer_on_changed(obj_t *obj, const attribute_t *attr);
static void observer_on_timeattr_changed(obj_t *obj, const attribute_t *attr);
static void observer_on_city_changed(obj_t *obj, const attribute_t *attr);
static int city_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user);
static json_value *observer_get_azalt(obj_t *obj, const attribute_t *attr,
                                      const json_value *args);
static obj_klass_t observer_klass = {
    .id = "observer",
    .size = sizeof(observer_t),
    .flags = OBJ_IN_JSON_TREE,
    .init = observer_init,
    .clone = observer_clone,
    .attributes = (attribute_t[]) {
        PROPERTY("longitude", "f", MEMBER(observer_t, elong),
                 .hint = "d_angle",
                 .on_changed = observer_on_changed),
        PROPERTY("latitude", "f", MEMBER(observer_t, phi),
                 .hint = "d_angle",
                 .on_changed = observer_on_changed),
        PROPERTY("elevation", "f", MEMBER(observer_t, hm),
                 .on_changed = observer_on_changed),
        PROPERTY("refraction", "b", MEMBER(observer_t, refraction),
                 .on_changed = observer_on_changed),
        PROPERTY("ut1", "f", MEMBER(observer_t, ut1),
                 .hint = "mjd", .on_changed = observer_on_timeattr_changed),
        PROPERTY("utc", "f", MEMBER(observer_t, utc),
                 .hint = "mjd", .on_changed = observer_on_timeattr_changed),
        PROPERTY("city", "p", MEMBER(observer_t, city),
                 .hint = "obj", .choices = city_get_choices,
                 .on_changed = observer_on_city_changed),
        PROPERTY("altitude", "f", MEMBER(observer_t, altitude),
                .hint = "d_angle"),
        PROPERTY("azimuth", "f", MEMBER(observer_t, azimuth),
                .hint = "h_angle"),
        PROPERTY("azalt", "v3", .hint = "azalt", .fn = observer_get_azalt),
        {}
    },
};
OBJ_REGISTER(observer_klass)

static obj_t *core_get(const obj_t *obj, const char *id, int flags);
static obj_t *core_get_by_nsid(const obj_t *obj, uint64_t nsid);
static int core_list(const obj_t *obj, double max_mag, void *user,
                     int (*f)(const char *id, void *user));
static json_value *core_lookat(obj_t *obj, const attribute_t *attr,
                               const json_value *args);
static int selection_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user);
static int proj_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user);
static void core_on_fov_changed(obj_t *obj, const attribute_t *attr);
static void core_on_utcoffset_changed(obj_t *obj, const attribute_t *attr);
static json_value *core_fn_progressbars(obj_t *obj, const attribute_t *attr,
                                        const json_value *args);

static obj_klass_t core_klass = {
    .id = "core",
    .size = sizeof(core_t),
    .flags = OBJ_IN_JSON_TREE,
    .get = core_get,
    .get_by_nsid = core_get_by_nsid,
    .list = core_list,
    .attributes = (attribute_t[]) {
        PROPERTY("utcoffset", "d", MEMBER(core_t, utc_offset),
                 .on_changed = core_on_utcoffset_changed),
        PROPERTY("fov", "f", MEMBER(core_t, fov),
                 .on_changed = core_on_fov_changed),
        PROPERTY("projection", "d", MEMBER(core_t, proj),
                 .choices = proj_get_choices),
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
        FUNCTION("lookat", .fn = core_lookat),
        {}
    }
};

OBJ_REGISTER(core_klass)

typedef union
{
    bool    b;
    int     d;
    double  f;
    void   *p;
} any_t;

static int selection_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user)
{
    char cat_up[8];
    char buff[128];
    const char *id, *cat, *value;
    int nb = 0, r;
    IDENTIFIERS_ITER(NULL, NULL, &id, &cat, &value, NULL, NULL) {
        if      (str_startswith(id, "CITY ")) continue;
        if      (str_equ(cat, "NAME"))  r = f(value, id, user);
        else if (str_equ(cat, "BAYER")) r = f(value, id, user);
        else {
            str_to_upper(cat, cat_up);
            sprintf(buff, "%s %s", cat_up, value);
            r = f(buff, id, user);
        }
        nb++;
        if (r) break;
    }
    return nb;
}

static int proj_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user)
{
    char id_buf[8];
    int nb = 0;
    #define F(p, n) do { \
        nb++; \
        if (f) {sprintf(id_buf, "%d", p); f(n, id_buf, user);} \
    } while (0)
    F(PROJ_PERSPECTIVE, "Perspective");
    F(PROJ_STEREOGRAPHIC, "Stereographic");
    F(PROJ_MERCATOR, "Mercator");
    #undef F
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
    DL_FOREACH(core->obj.children, module) {
        if (strcmp(module->id, id) == 0) return module;
        ret = obj_get(module, id, flags);
        if (ret) return ret;
    }
    id = identifiers_search(id);
    if (id) {
        DL_FOREACH(core->obj.children, module) {
            ret = obj_get(module, id, flags);
            if (ret) return ret;
        }
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

static int core_list(const obj_t *obj, double max_mag, void *user,
                     int (*f)(const char *id, void *user))
{
    // XXX: won't stop if the callback return != 0.
    obj_t *module;
    int nb = 0;
    DL_FOREACH(core->obj.children, module) {
        nb += obj_list(module, max_mag, user, f);
    }
    return nb;
}

static int city_get_choices(
        int (*f)(const char* name, const char *id, void *user),
        void *user)
{
    // XXX: we are iterating all the registered names, this is too slow.
    // We should have a way to index the objects by catalog in identifiers.
    const char *id, *cat, *value, *search_str;
    IDENTIFIERS_ITER(NULL, "NAME", &id, &cat, &value, &search_str, NULL) {
        if (str_startswith(id, "CITY "))
            if (f(value, id, user)) return 0;
    }
    return 0;
}

static int observer_init(obj_t *obj, json_value *args)
{
    observer_recompute_hash((observer_t*)obj);
    return 0;
}

static obj_t *observer_clone(const obj_t *obj)
{
    observer_t *ret;
    ret = (observer_t*)obj_create("observer", "observer", obj->parent, NULL);
    // Copy all except obj attributes.
    memcpy(((char*)ret) + sizeof(obj_t), ((char*)obj) + sizeof(obj_t),
           sizeof(*ret) - sizeof(obj_t));
    return &ret->obj;
}

static void observer_on_changed(obj_t *obj, const attribute_t *attr)
{
    observer_t *obs = (observer_t*)obj;
    obs->dirty = true;
    obs->force_full_update = true;
    observer_recompute_hash(obs);
}

static void observer_on_timeattr_changed(obj_t *obj, const attribute_t *attr)
{
    // Make sure that the TT is synced.
    observer_t *obs = (observer_t*)obj;
    double ut11, ut12, tt1, tt2, dt, dut1 = 0;
    if (strcmp(attr->name, "utc") == 0) {
        dt = deltat(obs->utc);
        eraUtcut1(DJM0, obs->utc, dut1, &ut11, &ut12);
        eraUt1tt(ut11, ut12, dt, &tt1, &tt2);
        obs->ut1 = ut11 - DJM0 + ut12;
        obs->tt = tt1 - DJM0 + tt2;
    }
    if (strcmp(attr->name, "ut1") == 0) {
        dt = deltat(obs->ut1);
        eraUt1tt(DJM0, obs->ut1, dt, &tt1, &tt2);
        obs->tt = tt1 - DJM0 + tt2;
    }
    obs->dirty = true;
    observer_recompute_hash(obs);
}

// Expose azalt vector to js.
static json_value *observer_get_azalt(obj_t *obj, const attribute_t *attr,
                                 const json_value *args)
{
    observer_t *obs = (observer_t*)obj;
    double v[3];
    eraS2c(obs->azimuth, obs->altitude, v);
    return args_value("v3", "azalt", v);
}

static void observer_on_city_changed(obj_t *obj, const attribute_t *attr)
{
    observer_t *obs = (observer_t*)obj;
    double lat, lon, el;
    if (!obs->city) return;
    obj_get_attr(obs->city, "latitude", "f", &lat);
    obj_get_attr(obs->city, "longitude", "f", &lon);
    obj_get_attr(obs->city, "elevation", "f", &el);
    obj_set_attr(obj, "latitude", "f", lat);
    obj_set_attr(obj, "longitude", "f", lon);
    obj_set_attr(obj, "elevation", "f", el);
}

static int modules_sort_cmp(void *a, void *b)
{
    obj_t *at, *bt;
    at = a;
    bt = b;
    return cmp(obj_get_render_order(at), obj_get_render_order(bt));
}

static void core_windows_to_ndc(const double p[2], double out[2])
{
    out[0] = p[0] / core->win_size[0] * 2 - 1;
    out[1] = -1 * (p[1] / core->win_size[1] * 2 - 1);
}

static int core_update(void);
static int core_update_direction(double dt);

// Convert mouse position to observed coordinates.
static void mouse_to_observed(double x, double y, double p[3])
{
    double rv2o[4][4];
    projection_t proj;
    double pos[4] = {x, y};
    double ratio = core->win_size[0] / core->win_size[1];

    mat4_invert(core->observer->ro2v, rv2o);
    projection_init(&proj, core->proj, core->fov, ratio);

    core_windows_to_ndc(pos, pos);
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
    core->observer->dirty = true;
    core->fast_mode = true;
    obj_set_attr(&core->obj, "lock", "p", NULL);
    observer_update(core->observer, true);
    // Notify the changes.
    obj_changed(&core->observer->obj, "azalt");
    obj_changed(&core->observer->obj, "az");
    obj_changed(&core->observer->obj, "alt");
    return 0;
}

static obj_t *get_obj_at(double x, double y, double max_dist)
{
    point_t *p = NULL, *best = NULL;
    double pos[2] = {x, y};
    double dist, best_dist = max_dist;
    while ( (p = (point_t*)utarray_next(core->rend_points, p)) ) {
        dist = vec2_dist(p->pos, pos) - p->size;
        if (dist < best_dist) {
            best_dist = dist;
            best = p;
        }
    }
    if (!best) return NULL;
    return obj_get(NULL, best->id, 0);
}

static int core_click(const gesture_t *g, void *user)
{
    obj_t *obj;
    obj = get_obj_at(g->pos[0], g->pos[1], 18);
    obj_set_attr(&core->obj, "selection", "p", obj);
    return 0;
}

static int core_hover(const gesture_t *g, void *user)
{
    obj_t *obj;
    obj = get_obj_at(g->pos[0], g->pos[1], 18);
    obj_set_attr(&core->obj, "hovered", "p", obj);
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

// Set the default init values.
static void core_set_default(void)
{
    double tsl;
    observer_t *obs = core->observer;

    obj_set_attr(&obs->obj, "city", "p", obj_get(NULL, "CITY TW TAIPEI", 0));
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
    obs->force_full_update = true;
    core->contrast = 0.9;
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

// Add all the bayer names into the identifiers table.
static void add_bayer_identifiers(void)
{
    int hd, bayer, n, i;
    char cons[4], id[32], txt[32], show[32];
    const char *greek[3];
    const char *g_abbr, *g_full;

    for (i = 0; bayer_iter(i, &hd, cons, &bayer, &n, greek); i++) {
        sprintf(id, "HD %d", hd);
        g_abbr = greek[1];
        g_full = greek[2];
        sprintf(txt,  "%s%.*d %s", g_abbr, n ? 1 : 0, n, cons);
        sprintf(show, "%s%.*d %s", g_full, n ? 1 : 0, n, cons);
        identifiers_add(id, "BAYER", txt, NULL, show);
    }
}

EMSCRIPTEN_KEEPALIVE
void core_init(void)
{
    obj_klass_t *module;

    if (core) {
        // Already initialized.
        core_set_default();
        return;
    }
    texture_set_load_callback(NULL, texture_load_function);
    request_init(".cache"); // XXX: use a proper cache directory!
    identifiers_init();
    add_bayer_identifiers();

    font_init(asset_get_data("asset://font/DejaVuSans-small.ttf", NULL, NULL));
    core = (core_t*)obj_create("core", "core", NULL, NULL);
    obj_add_sub(&core->obj, "hints");
    core->hints_mag_max = NAN;

    core->observer = (observer_t*)obj_create("observer", "observer",
                                             (obj_t*)core, NULL);

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

    static UT_icd point_icd = {sizeof(point_t), NULL, NULL, NULL};
    utarray_new(core->rend_points, &point_icd);
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
    double v[3] = {1, 0, 0}, q[4], t;

    if (core->target.speed) {
        core->target.t += dt / core->target.speed;
        t = smoothstep(0.0, 1.0, core->target.t);
        // Make sure we finish on an exact value.
        if (core->target.t >= 1.0) t = 1.0;
        quat_slerp(core->target.src_q, core->target.dst_q, t, q);
        quat_mul_vec3(q, v, v);
        eraC2s(v, &core->observer->azimuth, &core->observer->altitude);
        if (core->target.dst_fov) {
            core->fov = mix(core->target.src_fov, core->target.dst_fov, t);
            obj_changed(&core->obj, "fov");
        }
        core->observer->dirty = true;
        if (core->target.t >= 1.0) {
            core->target.speed = 0.0;
            core->target.t = 0.0;
            core->target.dst_fov = 0.0;
        }
        // Notify the changes.
        obj_changed(&core->observer->obj, "azalt");
        obj_changed(&core->observer->obj, "az");
        obj_changed(&core->observer->obj, "alt");
    }

    if (core->target.lock && !core->target.t) {
        obj_update(core->target.lock, core->observer, 0);
        eraS2c(core->target.lock->pos.az, core->target.lock->pos.alt, v);
        obj_call(&core->obj, "lookat", "v3f", v, 0.0);
    }

    return 1;
}

static int core_update(void)
{
    bool r;
    observer_update(core->observer, true);
    // XXX: Ad-hoc formula!
    r = move_toward(&core->vmag_shift,
            max(0, -3 - core->max_vmag_in_fov) + core->manual_vmag_shift,
            0, 64.0, 1.0 / 60);
    core->max_vmag_in_fov = INFINITY;
    return r ? 1 : 0;
}

// Test whether the landscape hides the view below the horizon.
static bool is_below_horizon_hidden(void)
{
    obj_t *ls;
    bool visible;
    double color[4];
    ls = obj_get(&core->obj, "landscape", 0);
    obj_get_attr(ls, "visible", "b", &visible);
    if (!visible) return false;
    obj_get_attr(ls, "color", "v4", color);
    if (color[3] < 1.0) return false;
    return true;
}

static double get_radius_for_mag(double mag);
// Compute the maximum *observed* magnitude that can be rendered on screen.
static double get_max_observed_mag(double min_radius)
{
    // Compute by dichotomy.
    const int max_iter = 128;
    const double eps = 0.001 * DD2R;
    double m1 = 0.0, m2 = 128.0;
    double v, ret;
    int i;

    for (i = 0; i < max_iter; i++) {
        ret = (m1 + m2) / 2;
        v = get_radius_for_mag(ret);
        if (fabs(v - min_radius) < eps) break;
        *((v < min_radius) ? &m2 : &m1) = ret;
    }
    if (i >= max_iter) LOG_W("Too many iterations! (%f)", ret);
    return ret;
}

// Helper function to compute the magnitude limits when we set up the painter.
// If <value> is not NAN, just return it, otherwise compute the best value
// To obtain <observed_mag>.
static double get_absolute_mag(double value, double observed_mag)
{
    // Compute by dichotomy.
    double min_v = -128.0, max_v = 128.0, ret, v;
    const double eps = 0.001;
    const int max_iter = 128;
    int i;

    if (!isnan(value)) return value;
    for (i = 0; i < max_iter; i++) {
        ret = mix(min_v, max_v, 0.5);
        v = core_get_observed_mag(ret);
        if (fabs(v - observed_mag) < eps) break;
        *((v < observed_mag) ? &min_v : &max_v) = ret;
    }
    if (i >= max_iter) LOG_W("Too many iterations!");
    return ret;
}

EMSCRIPTEN_KEEPALIVE
int core_render(int w, int h)
{
    obj_t *module;
    projection_t proj;
    double t;
    double dt = 1.0 / 16.0;
    int r;
    bool updated = false;
    double max_mag;

    // Constants that define the magnitude needed to see objects, hints,
    // and labels.  Default values.
    const double max_hint_mag = 7.0;
    const double max_label_mag = 4.0;

    // XXX: Compute r_min and max_mag from the screen resolution.
    // I add a small security offset to max_mag to prevent bugs when we
    // zoom out.
    const float r_min = 0.01 * DD2R;
    max_mag = get_max_observed_mag(r_min) + 0.5;

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
    core->win_size[0] = w;
    core->win_size[1] = h;
    labels_reset();
    r = core_update();
    if (r) updated = true;

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

    projection_init(&proj, core->proj, core->fov, (float)w / h);

    vec2_copy(core->view_shift, proj.offset);
    painter_t painter = {
        .rend = core->rend,
        .obs = core->observer,
        .transform = &mat4_identity,
        .fb_size = {w, h},
        .proj = &proj,
        .mag_max = get_absolute_mag(NAN, max_mag),
        .hint_mag_max = get_absolute_mag(core->hints_mag_max, max_hint_mag),
        .label_mag_max = get_absolute_mag(NAN, max_label_mag),
        .points_smoothness = 0.75,
        .color = {1.0, 1.0, 1.0, 1.0},
        .flags = (core->fast_mode ? PAINTER_FAST_MODE : 0) |
            (is_below_horizon_hidden() ? PAINTER_HIDE_BELOW_HORIZON : 0),
    };
    // Limit the number of labels when we zoom out, so that we don't make
    // the screen filled with text.
    // XXX: in fact this should depend on the labels size relative to the
    // screen size.
    // painter.label_mag_max = smoothstep(720, 60, core->fov * DR2D) * 4.0;

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

    paint_prepare(&painter, w, h);
    DL_FOREACH(core->obj.children, module) {
        obj_render(module, &painter);
    }

    paint_finish(&painter);
    if (core->fast_mode) updated = true;
    core->fast_mode = false;
    if (DEBUG && !DEFINED(__EMSCRIPTEN__)) progressbar_print_all();
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
        {"G", "core.landscape"},
        {"C", "core.constellations.lines"},
        {"R", "core.constellations.images"},
        {"Z", "core.lines.azimuthal"},
        {"E", "core.lines.equatorial"},
        {"M", "core.lines.meridian"},
        {"N", "core.dsos"},
        {"D", "core.dss"},
        {"S", "core.stars"},
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
    core->observer->dirty = true;
    core->fast_mode = true;
    // Notify the changes.
    obj_changed(&core->observer->obj, "azalt");
    obj_changed(&core->observer->obj, "az");
    obj_changed(&core->observer->obj, "alt");
}

/*
 * Function: compute_observed_mag
 * Compute the observed magnitude by simulating the optic.
 *
 * Parameters:
 *   vmag       - The source input magnitude.
 *   fov        - Viewer fov (rad).
 *
 * Returns:
 *   The observed magnitude.
 */
static double compute_observed_mag(double vmag, double fov)
{
    // I use upercases variable names to stay consistent with the notations
    // used in:
    // http://www.rocketmime.com/astronomy/Telescope/MagnitudeGain.html
    double Dep = 6.3; // Diameter of the eye pupil, for an average person.
    double M;    // Magnification.
    double Do;   // Objective diameter.
    double Gmag; // Light grasp magnitude.

    // When fov is larger than 60deg, we are not in telescope mode anymore,
    // so we change the point size linearly.
    if (fov < 60 * DD2R) {
        // XXX: Objective diameter chosen so that at 60 fov, Gmag = 0.
        M = 60 * DD2R / fov;
        Do = M * Dep;
        Gmag = 5 * log10(Do) - 4;
        vmag -= Gmag;
    } else {
        // Non telescope mode: vmag changes proportionally to the
        // magnification.  I chose the value 0.4 after some tests.
        M = 60 * DD2R / fov;
        vmag /= 1.0 - 0.4 * (1.0 - M);
    }

    return vmag;
}

// Compute the observed magnitude of an object after adjustment for the
// optic and eye adaptation.
double core_get_observed_mag(double vmag)
{
    vmag = compute_observed_mag(vmag, core->fov);
    // Shift due to exposure.
    vmag += core->vmag_shift;
    return vmag;
}

/*
 * Function: get_radius_for_mag
 * Compute the rendered radius for a given magnitude.
 */
static double get_radius_for_mag(double mag)
{
    const double r0 = 0.6 * DD2R;       // radius at mag = 0.
    const double mi = 8.0;         // Max visible mag.
    double r;

    r = -r0 / mi * mag + r0;
    if (r <= 0.0) return 0.0;
    r *= pow(smoothstep(0.0, r0, r), core->contrast);
    return r;
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
 *   radius    - Output radius.  Note: the fov is already taken into account
 *               in the output.
 *   luminance - Output luminance.  Ignored if set to NULL.
 */
void core_get_point_for_mag(double mag, double *radius, double *luminance)
{
    double l;                           // Luminance.
    double r;                           // Radius in rad.
    // XXX: this value should depend on the screen resolution.
    const float r_min = 0.1 * DD2R; // Below this radius we lower luminance.

    r = get_radius_for_mag(mag);

    if (r == 0) {
        *radius = 0;
        if (luminance) *luminance = 0;
        return;
    }

    l = 1.0;        // Full luminance.
    if (r > 0 && r < r_min) {
        l *= r / r_min;
        r = r_min;
    }
    r /= (60 * DD2R / core->fov);
    *radius = r;
    if (luminance) *luminance = l;
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
    if (sep - r > core->fov) return; // Not visible at all.
    // Here we use an heuristic so that objects that are small or not
    // centered don't affect the eye adaptation too much.
    if (r < 360 * DD2R) { // Not for atmosphere.
        vmag = compute_observed_mag(vmag, core->fov);
        vmag *= smoothstep(0, core->fov / 32.0, r);
        vmag *= smoothstep(core->fov * 0.75, 0, max(0, sep - r));
    }
    core->max_vmag_in_fov = min(core->max_vmag_in_fov, vmag);
}


static json_value *core_lookat(obj_t *obj, const attribute_t *attr,
                               const json_value *args)
{
    // XXX find a better way to create a rot quaternion from a direction?
    double az, al, speed = 1.0, pos[3], fov = 0.0;

    args_get(args, "target", 1, "v3", "azalt", pos);
    args_get(args, "speed", 2, "f", NULL, &speed);
    args_get(args, "fov", 3, "f", NULL, &fov);

    // Direct lookat.
    if (speed == 0.0) {
        eraC2s(pos, &core->observer->azimuth, &core->observer->altitude);
        core->observer->dirty = true;
        core->observer->force_full_update = true;
        if (fov) core->fov = fov;
        observer_recompute_hash(core->observer);
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
    // Test obj_get with '|'
    obj = obj_get(NULL, "No result|Sun", 0);
    assert(obj);
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
