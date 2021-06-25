/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

/*
 * Enum of all the data files we need to parse.
 */
enum {
    LS_DESCRIPTION    = 1 << 1,
};

/*
 * Type: landscape_t
 * Represent an individual landscape.
 */
typedef struct landscape {
    obj_t           obj;
    char            *key; // The key passed to add_data_source.
    char            *uri;
    fader_t         visible;
    double          color[4];
    hips_t          *hips;
    obj_t           *shape; // For zero horizon landscape.
    bool            active;
    struct  {
        char        *name;
    } info;
    int             parsed; // union of LS_ enum for each parsed file.
    char            *description;  // html description if any.
} landscape_t;

/*
 * Type: landscapes_t
 * The module, that maintains the list of landscapes.
 */
typedef struct landscapes {
    obj_t           obj;
    fader_t         visible;
    fader_t         fog_visible;
    landscape_t     *current; // The current landscape.
    int             loading_code; // Return code of the initial list loading.
} landscapes_t;


static int landscape_init(obj_t *obj, json_value *args)
{
    landscape_t *ls = (void*)obj;
    fader_init(&ls->visible, false);
    vec4_set(ls->color, 1.0, 1.0, 1.0, 1.0);
    return 0;
}

static int landscape_update(obj_t *obj, double dt)
{
    landscape_t *ls = (landscape_t*)obj;
    const char *data;
    char path[1024];
    int code;

    if (!(ls->parsed & LS_DESCRIPTION)) {
        snprintf(path, sizeof(path), "%s/%s", ls->uri, "description.en.utf8");
        data = asset_get_data(path, NULL, &code);
        if (!code) return 0;
        ls->parsed |= LS_DESCRIPTION;
        if (!data) return 0;
        ls->description = strdup(data);
        module_changed((obj_t*)ls, "description");
    }

    return fader_update(&ls->visible, dt);
}

// XXX: this should be a function of core, with an observer argument or
// something.
static double get_global_brightness(void)
{
    // I use the algo from stellarium, even though I don't really understand
    // it.
    obj_t *sun, *moon;
    double pos[4];
    double sin_angle;
    double brightness = 0.0;
    double moon_phase;
    sun = core_get_planet(PLANET_SUN);
    obj_get_pos(sun, core->observer, FRAME_OBSERVED, pos);
    vec3_normalize(pos, pos);
    sin_angle = sin(min(M_PI/ 2, asin(pos[2]) + 8. * DD2R));
    if (sin_angle > -0.1 / 1.5 )
        brightness += 1.5 * (sin_angle + 0.1 / 1.5);

    moon = core_get_planet(PLANET_MOON);
    obj_get_pos(moon, core->observer, FRAME_OBSERVED, pos);
    obj_get_info(moon, core->observer, INFO_PHASE, &moon_phase);
    vec3_normalize(pos, pos);
    sin_angle = sin(min(M_PI/ 2, asin(pos[2]) + 8. * DD2R));
    if (sin_angle > -0.1 / 1.5 )
        brightness += moon_phase * 0.2 * (sin_angle + 0.1 / 1.5);

    return min(brightness * 1.2, 1.0);
}

/*
 * Render the fog using a healpix projection and opengl shader.
 */
static void render_fog(const painter_t *painter_, double alpha)
{
    int pix, order = 1, split = 2;
    double theta, phi;
    painter_t painter = *painter_;
    uv_map_t map;

    painter.color[3] *= alpha;
    if (painter.color[3] == 0.0) return;

    painter.flags |= PAINTER_FOG_SHADER;
    // Note: we could try to optimize further by doing a breath first
    // iteration to skip level 0 tiles.
    for (pix = 0; pix < 12 * 4; pix++) {
        healpix_pix2ang(1 << order, pix, &theta, &phi);
        // Skip tiles that will be totally transparent anyway.
        if (fabs(theta - M_PI / 2) > 20 * DD2R) continue;
        if (painter_is_healpix_clipped(&painter, FRAME_OBSERVED, order, pix))
            continue;
        uv_map_init_healpix(&map, order, pix, true, true);
        paint_quad(&painter, FRAME_OBSERVED, &map, split);
    }
}

static int landscape_render(const obj_t *obj, const painter_t *painter_)
{
    landscape_t *ls = (landscape_t*)obj;
    landscapes_t *lss = (landscapes_t*)obj->parent;
    painter_t painter = *painter_;
    double alpha, alt, az, direction[3];
    double brightness;
    const int split_order = 3;
    // Hack matrix to fix the hips survey orientation.
    const double rg2h[4][4] = {
        {1,  0,  0,  0},
        {0, -1,  0,  0},
        {0,  0,  1,  0},
        {0,  0,  0,  1},
    };

    painter.color[3] *= ls->visible.value;
    if (painter.color[3] == 0.0) return 0;

    // Don't hide below horizon when we draw the horizon!
    painter.flags &= ~PAINTER_HIDE_BELOW_HORIZON;

    render_fog(&painter, lss->fog_visible.value);

    painter.color[3] *= lss->visible.value;
    brightness = get_global_brightness();

    // Adjust the alpha to make the landscape transparent when we look down
    // and when we zoom in.
    convert_frame(core->observer, FRAME_VIEW, FRAME_OBSERVED, true,
                  VEC(0, 0, -1), direction);
    eraC2s(direction, &az, &alt);
    alpha = smoothstep(1, 20, core->fov * DR2D);
    alpha = mix(alpha, alpha / 2, smoothstep(0, -45, alt * DR2D));
    painter.color[3] *= alpha;
    if (painter.color[3] == 0.0) return 0;

    if (ls->hips && hips_is_ready(ls->hips)) {
        vec3_mul(brightness, painter.color, painter.color);
        hips_render(ls->hips, &painter, rg2h, split_order);
    }
    if (ls->shape) {
        obj_render(ls->shape, &painter);
    }
    return 0;
}

static void landscape_on_active_changed(obj_t *obj, const attribute_t *attr)
{
    landscape_t *ls = (void*)obj, *other;

    // Deactivate the others.
    if (ls->active) {
        MODULE_ITER(ls->obj.parent, other, "landscape") {
            if (other == ls) continue;
            obj_set_attr((obj_t*)other, "active", false);
        }
    }
    ls->visible.target = ls->active;
    // Set the current attribute of the landscape manager object.
    if (ls->active) {
        obj_set_attr(ls->obj.parent, "current", ls);
        module_changed(ls->obj.parent, "current_id");
    }
}

static landscape_t *add_from_uri(landscapes_t *lss, const char *uri,
                                 const char *key)
{
    landscape_t *ls;

    ls = (void*)module_add_new(&lss->obj, "landscape", NULL);
    ls->key = strdup(key);
    ls->obj.id = ls->key;
    ls->uri = strdup(uri);
    if (strcmp(key, "zero") != 0) {
        ls->hips = hips_create(uri, 0, NULL);
        hips_set_label(ls->hips, "Landscape");
        hips_set_frame(ls->hips, FRAME_OBSERVED);
        ls->info.name = strdup(key);
    } else {
        // Zero horizon shape.
        ls->shape = module_add_new(&ls->obj, "circle", NULL);
        obj_set_attr(ls->shape, "pos", VEC(0, 0, -1, 0));
        obj_set_attr(ls->shape, "frame", FRAME_OBSERVED);
        obj_set_attr(ls->shape, "size", VEC(M_PI, M_PI));
        obj_set_attr(ls->shape, "color", VEC(0.1, 0.15, 0.1, 1.0));
        obj_set_attr(ls->shape, "border_color", VEC(0.2, 0.4, 0.1, 1.0));
        ls->info.name = strdup("Zero Horizon");
    }
    return ls;
}

static int landscapes_init(obj_t *obj, json_value *args)
{
    landscapes_t *lss = (landscapes_t*)obj;
    fader_init(&lss->visible, true);
    fader_init(&lss->fog_visible, true);
    return 0;
}

static int landscapes_update(obj_t *obj, double dt)
{
    landscapes_t *lss = (landscapes_t*)obj;
    obj_t *ls;
    MODULE_ITER((obj_t*)lss, ls, "landscape") {
        landscape_update(ls, dt);
    }
    fader_update(&lss->visible, dt);
    fader_update(&lss->fog_visible, dt);
    return 0;
}

static int landscapes_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *ls;
    MODULE_ITER(obj, ls, "landscape") {
        obj_render(ls, painter);
    }
    return 0;
}

static void landscapes_gui(obj_t *obj, int location)
{
    landscape_t *ls;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Landscapes")) {
        MODULE_ITER(obj, ls, "landscape") {
            gui_item(&(gui_item_t){
                    .label = ls->key,
                    .obj = (obj_t*)ls,
                    .attr = "active"});
        }
        gui_tab_end();
    }
}

static int landscapes_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    landscapes_t *lss = (landscapes_t*)obj;
    landscape_t *ls;

    // Skip if we already have it.
    MODULE_ITER(lss, ls, NULL) {
        if (strcmp(ls->key, key) == 0) return 0;
    }

    ls = add_from_uri(lss, url, key);
    // If this is the first landscape, use it immediatly.
    if (ls == (void*)lss->obj.children) {
        obj_set_attr((obj_t*)ls, "active", true);
        ls->visible.value = 1.0;
    }
    return 0;
}

// Set/Get the current landscape by id.
static json_value *landscapes_current_id_fn(
        obj_t *obj, const attribute_t *attr, const json_value *args)
{
    char id[128];
    landscapes_t *lss = (void*)obj;
    landscape_t *ls;
    if (args && args->u.array.length) {
        args_get(args, TYPE_STRING, id);
        MODULE_ITER(lss, ls, "landscape") {
            if (strcmp(ls->key, id) == 0) {
                obj_set_attr((obj_t*)ls, "active", true);
                break;
            }
        }
    }
    if (!lss->current) return args_value_new(TYPE_STRING, "");
    return args_value_new(TYPE_STRING, lss->current->key);
}

/*
 * Meta class declarations.
 */

static obj_klass_t landscape_klass = {
    .id = "landscape",
    .size = sizeof(landscape_t),
    .flags = OBJ_IN_JSON_TREE,
    .init = landscape_init,
    .update = landscape_update,
    .render = landscape_render,
    .render_order = 40,
    .attributes = (attribute_t[]) {
        PROPERTY(name, TYPE_STRING_PTR, MEMBER(landscape_t, info.name)),
        PROPERTY(visible, TYPE_BOOL, MEMBER(landscape_t, visible.target)),
        PROPERTY(color, TYPE_COLOR, MEMBER(landscape_t, color)),
        PROPERTY(active, TYPE_BOOL, MEMBER(landscape_t, active),
                 .on_changed = landscape_on_active_changed),
        PROPERTY(description, TYPE_STRING_PTR,
                 MEMBER(landscape_t, description)),
        PROPERTY(url, TYPE_STRING_PTR, MEMBER(landscape_t, uri)),
        {}
    },
};
OBJ_REGISTER(landscape_klass)

static obj_klass_t landscapes_klass = {
    .id             = "landscapes",
    .size           = sizeof(landscapes_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = landscapes_init,
    .update         = landscapes_update,
    .render         = landscapes_render,
    .gui            = landscapes_gui,
    .add_data_source    = landscapes_add_data_source,
    .render_order   = 40,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(landscapes_t, visible.target)),
        PROPERTY(fog_visible, TYPE_BOOL,
                 MEMBER(landscapes_t, fog_visible.target)),
        PROPERTY(current, TYPE_OBJ, MEMBER(landscapes_t, current)),
        PROPERTY(current_id, TYPE_STRING, .fn = landscapes_current_id_fn),
        {}
    },
};
OBJ_REGISTER(landscapes_klass)
