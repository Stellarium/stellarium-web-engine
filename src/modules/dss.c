/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// HIPS survey support.

typedef struct dss {
    obj_t       obj;
    fader_t     visible;
    hips_t      *hips;
} dss_t;

static int dss_init(obj_t *obj, json_value *args);
static int dss_update(obj_t *obj, const observer_t *obs, double dt);
static int dss_render(const obj_t *obj, const painter_t *painter);
static obj_klass_t dss_klass = {
    .id = "dss",
    .size = sizeof(dss_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = dss_init,
    .update = dss_update,
    .render = dss_render,
    .render_order = 6,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(dss_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(dss_klass)

static int dss_init(obj_t *obj, json_value *args)
{
    dss_t *dss = (void*)obj;
    fader_init(&dss->visible, true); // Visible by default.
    dss->hips = hips_create("https://alaskybis.unistra.fr/DSS/DSSColor", 0);
    return 0;
}

static int dss_render(const obj_t *obj, const painter_t *painter)
{
    double visibility;
    dss_t *dss = (dss_t*)obj;
    painter_t painter2 = *painter;

    if (dss->visible.value == 0.0) return 0;
    // Fade the survey between 20° and 10° fov.
    visibility = smoothstep(20 * DD2R, 10 * DD2R, core->fov);
    painter2.color[3] *= dss->visible.value * visibility;
    // Adjust for eye adaptation.
    // Should this be done in the painter?
    painter2.color[3] /= pow(2.5, 0.5 * core->vmag_shift);
    if (painter2.color[3] == 0.0) return 0;
    return hips_render(dss->hips, &painter2, 2 * M_PI);
}

static int dss_update(obj_t *obj, const observer_t *obs, double dt)
{
    dss_t *dss = (dss_t*)obj;
    return fader_update(&dss->visible, dt);
}
