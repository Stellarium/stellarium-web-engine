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
 * Type: landscape_t
 * Represent an individual landscape.
 */
typedef struct landscape {
    obj_t           obj;
    fader_t         visible;
    double          color[4];
    hips_t          *hips;
    texture_t       *fog;
} landscape_t;

/*
 * Type: landscapes_t
 * The module, that maintains the list of landscapes.
 */
typedef struct landscapes {
    obj_t           obj;
    fader_t         visible;
    landscape_t     *current; // The current landscape.
} landscapes_t;


static int landscape_init(obj_t *obj, json_value *args)
{
    landscape_t *ls = (void*)obj;
    fader_init(&ls->visible, true);
    // XXX: this should be taken from the hipslist.
    // XXX: I use guereins2 as a temporary survey, since the original
    // is only in png, while this one is using wepb.
    ls->hips = hips_create("https://data.stellarium.org/surveys/guereins2", 0);
    hips_set_label(ls->hips, "Landscape");
    hips_set_frame(ls->hips, FRAME_OBSERVED);
    vec4_set(ls->color, 1.0, 1.0, 1.0, 1.0);
    return 0;
}

static obj_t *landscape_add_res(obj_t *obj, json_value *val,
                                const char *base_path)
{
    const char *type, *url;
    landscape_t *landscape = (landscape_t*)obj;
    type = json_get_attr_s(val, "type");
    if (!type || strcmp(type, "landscape") != 0)
        return NULL;
    url = json_get_attr_s(val, "url");
    if (!url) {
        LOG_E("landscape needs a url attribute");
        return NULL;
    }
    landscape->hips = hips_create(url, 0);
    landscape->color[3] = 1.0;
    return NULL; // Should render the hips but it's not an object...
}

static int landscape_update(obj_t *obj, const observer_t *obs, double dt)
{
    landscape_t *landscape = (landscape_t*)obj;
    return fader_update(&landscape->visible, dt);
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
    sun = obj_get(&core->obj, "SUN", 0);
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

    paint_quad(&painter, FRAME_OBSERVED, ls->fog, NULL,
               FULL_UV, &proj_spherical, div);

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

static int landscapes_init(obj_t *obj, json_value *args)
{
    landscapes_t *lss = (landscapes_t*)obj;
    fader_init(&lss->visible, true);
    obj_create("landscape", "guereins", (obj_t*)lss, NULL);
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
    .add_res = landscape_add_res,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(landscape_t, visible.target)),
        PROPERTY("color", "v4", MEMBER(landscape_t, color), .hint = "color"),
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
    .render_order   = 40,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(landscapes_t, visible.target)),
        PROPERTY("current", "p", MEMBER(landscapes_t, current),
                 .hint = "obj"),
        {}
    },
};
OBJ_REGISTER(landscapes_klass)
