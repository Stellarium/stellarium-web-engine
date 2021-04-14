/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

typedef struct milkyway {
    obj_t           obj;
    fader_t         visible;
    hips_t          *hips;
} milkyway_t;


static int milkyway_init(obj_t *obj, json_value *args)
{
    milkyway_t *mw = (void*)obj;
    fader_init(&mw->visible, true);
    return 0;
}

static int milkyway_update(obj_t *obj, double dt)
{
    milkyway_t *mw = (milkyway_t*)obj;
    if (!mw->hips) return 0;
    return fader_update(&mw->visible, dt);
}

static int milkyway_render(const obj_t *obj, const painter_t *painter_)
{
    double lum, c;
    int split_order = 2;
    milkyway_t *mw = (milkyway_t*)obj;
    painter_t painter = *painter_;
    double visibility;

    if (!mw->hips) return 0;
    if (mw->visible.value == 0.0) return 0;

    // For small FOV we use the DSS texture
    visibility = smoothstep(10 * DD2R, 20 * DD2R, core->fov);
    painter.color[3] *= mw->visible.value * visibility;

    // Ad-hock formula for tone mapping.
    lum = 0.002;
    c = tonemapper_map(&core->tonemapper, lum) * 10;

    // Modulate luminance from atmosphere average brightness so that
    // milky way becomes less visible with a full moon
    c *= min(0.004 / max(0.000001, core->lwsky_average), 1.0);
    c = max(c, 0) * 0.7;
    painter.color[3] *= c;

    if (painter.color[3] < 1./255)
        return 0;

    hips_render(mw->hips, &painter, NULL, split_order);
    return 0;
}

static int milkyway_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    milkyway_t *mw = (milkyway_t*)obj;
    if (mw->hips) return -1;
    mw->hips = hips_create(url, 0, NULL);
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t milkyway_klass = {
    .id = "milkyway",
    .size = sizeof(milkyway_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = milkyway_init,
    .update = milkyway_update,
    .render = milkyway_render,
    .add_data_source = milkyway_add_data_source,
    .render_order = 5,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(milkyway_t, visible.target)),
        {}
    },
};

OBJ_REGISTER(milkyway_klass)
