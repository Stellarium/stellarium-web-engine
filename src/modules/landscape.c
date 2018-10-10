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
    char            *uri;
    fader_t         visible;
    double          color[4];
    hips_t          *hips;
    texture_t       *fog;
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

static int landscape_update(obj_t *obj, const observer_t *obs, double dt)
{
    landscape_t *ls = (landscape_t*)obj;
    const char *data;
    char path[1024];
    int code;

    if (!(ls->parsed & LS_DESCRIPTION)) {
        sprintf(path, "%s/%s", ls->uri, "description.en.html");
        data = asset_get_data(path, NULL, &code);
        if (!code) return 0;
        ls->parsed |= LS_DESCRIPTION;
        if (!data) return 0;
        ls->description = strdup(data);
        obj_changed((obj_t*)ls, "description");
    }

    return fader_update(&ls->visible, dt);
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double az, al;
    az = v[0] * 360 * DD2R;
    al = (v[1] - 0.5) * 180 * DD2R;
    eraS2c(az, al, out);
    out[3] = 0.0; // At infinity.
}

// XXX: this should be a function of core, with an observer argument or
// something.
static double get_global_brightness(void)
{
    // I use the algo from stellarium, even though I don't really understand
    // it.
    obj_t *sun;
    double sun_pos[4];
    double sin_sun_angle;
    double brightness = 0.0;
    sun = obj_get_by_oid(&core->obj, oid_create("HORI", 10), 0);
    obj_get_pos_observed(sun, core->observer, sun_pos);
    vec3_normalize(sun_pos, sun_pos);
    sin_sun_angle = sin(min(M_PI/ 2, asin(sun_pos[2]) + 8. * DD2R));
    if(sin_sun_angle > -0.1 / 1.5 )
        brightness += 1.5 * (sin_sun_angle + 0.1 / 1.5);
    return min(brightness, 1.0);
}

static int landscape_render(const obj_t *obj, const painter_t *painter_)
{
    /*
     *
     *     +--------------------+ alt = 0
     *     |                    |
     *     |                    |
     *     |                    |
     *     +--------------------+ alt = -90
     *    az = 0              az = 360
     */
    landscape_t *ls = (landscape_t*)obj;
    painter_t painter = *painter_;
    projection_t proj_spherical = {
        .name       = "spherical",
        .backward   = spherical_project,
    };
    double brightness;
    double FULL_UV[][2] = {{0.0, 1.0}, {1.0, 1.0},
                           {0.0, 0.0}, {1.0, 0.0}};
    double HALF_UV[][2] = {{0.0, 0.5}, {1.0, 0.5},
                           {0.0, 0.0}, {1.0, 0.0}};
    // Hack matrice to fix the hips survey orientation.
    double rg2h[4][4] = {
        {1,  0,  0,  0},
        {0, -1,  0,  0},
        {0,  0,  1,  0},
        {0,  0,  0,  1},
    };
    int div = (painter.flags & PAINTER_FAST_MODE) ? 16 : 32;

    painter.color[3] *= ls->visible.value;
    if (painter.color[3] == 0.0) return 0;
    if (!ls->fog) {
        ls->fog = texture_from_url("asset://textures/fog.png", 0);
        assert(ls->fog);
    }

    brightness = get_global_brightness();

    painter.color[3] *= 0.5;
    paint_quad(&painter, FRAME_OBSERVED, ls->fog, NULL,
               FULL_UV, &proj_spherical, div);
    painter.color[3] /= 0.5;

    // Adjust the alpha to make the landscape transparent when we look down.
    painter.color[3] *= mix(1.0, 0.25,
                        smoothstep(0, -45, painter.obs->altitude * DR2D));

    if (ls->hips && hips_is_ready(ls->hips)) {
        vec3_mul(brightness, painter.color, painter.color);
        painter.transform = &rg2h;
        hips_render(ls->hips, &painter, 2 * M_PI);
    } else {
        vec4_copy(ls->color, painter.color);
        vec4_set(painter.color, 0, 0, 0, ls->visible.value);
        vec3_mul(brightness, painter.color, painter.color);
        paint_quad(&painter, FRAME_OBSERVED, NULL, NULL,
                   HALF_UV, &proj_spherical, div);
    }

    return 0;
}

static void landscape_on_active_changed(obj_t *obj, const attribute_t *attr)
{
    landscape_t *ls = (void*)obj, *other;

    // Deactivate the others.
    if (ls->active) {
        OBJ_ITER(ls->obj.parent, other, "landscape") {
            if (other == ls) continue;
            obj_set_attr((obj_t*)other, "active", "b", false);
        }
    }
    ls->visible.target = ls->active;
    // Set the current attribute of the landscape manager object.
    if (ls->active)
        obj_set_attr(ls->obj.parent, "current", "p", ls);
}

static landscape_t *add_from_uri(landscapes_t *lss, const char *uri,
                                 const char *id)
{
    landscape_t *ls;
    ls = (void*)obj_create("landscape", id, (obj_t*)lss, NULL);
    ls->uri = strdup(uri);
    ls->hips = hips_create(uri, 0, NULL);
    ls->info.name = strdup(id);
    hips_set_label(ls->hips, "Landscape");
    hips_set_frame(ls->hips, FRAME_OBSERVED);
    return ls;
}

static int landscapes_init(obj_t *obj, json_value *args)
{
    landscapes_t *lss = (landscapes_t*)obj;
    fader_init(&lss->visible, true);
    return 0;
}

static int landscapes_update(obj_t *obj, const observer_t *obs, double dt)
{
    landscapes_t *lss = (landscapes_t*)obj;
    obj_t *ls;
    OBJ_ITER((obj_t*)lss, ls, "landscape") {
        obj_update(ls, obs, dt);
    }
    return fader_update(&lss->visible, dt);
}

static int landscapes_render(const obj_t *obj, const painter_t *painter_)
{
    landscapes_t *lss = (landscapes_t*)obj;
    obj_t *ls;
    painter_t painter = *painter_;

    painter.color[3] *= lss->visible.value;
    OBJ_ITER(obj, ls, "landscape") {
        obj_render(ls, &painter);
    }
    return 0;
}

static void landscapes_gui(obj_t *obj, int location)
{
    landscape_t *ls;
    if (!DEFINED(SWE_GUI)) return;
    if (gui_tab("Landscapes")) {
        OBJ_ITER(obj, ls, "landscape") {
            gui_item(&(gui_item_t){
                    .label = ls->obj.id,
                    .obj = (obj_t*)ls,
                    .attr = "active"});
        }
        gui_tab_end();
    }
}

static int landscapes_add_data_source(
        obj_t *obj, const char *url, const char *type)
{
    const char *key;
    landscapes_t *lss = (landscapes_t*)obj;
    landscape_t *ls;

    if (!type || strcmp(type, "landscape") != 0) return 1;
    key = strrchr(url, '/') + 1;
    // Skip if we already have it.
    if (obj_get((obj_t*)lss, key, 0)) return 0;
    ls = add_from_uri(lss, url, key);
    // If this is the first landscape, use it immediatly.
    if (ls == (void*)lss->obj.children) {
        obj_set_attr((obj_t*)ls, "active", "b", true);
        ls->visible.value = 1.0;
    }
    return 0;
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
        PROPERTY("name", "s", MEMBER(landscape_t, info.name)),
        PROPERTY("visible", "b", MEMBER(landscape_t, visible.target)),
        PROPERTY("color", "v4", MEMBER(landscape_t, color), .hint = "color"),
        PROPERTY("active", "b", MEMBER(landscape_t, active),
                 .on_changed = landscape_on_active_changed),
        PROPERTY("description", "s", MEMBER(landscape_t, description)),
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
        PROPERTY("visible", "b", MEMBER(landscapes_t, visible.target)),
        PROPERTY("current", "p", MEMBER(landscapes_t, current),
                 .hint = "obj"),
        {}
    },
};
OBJ_REGISTER(landscapes_klass)
