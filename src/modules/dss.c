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

static int dss_init(obj_t *obj, json_value *args)
{
    dss_t *dss = (void*)obj;
    fader_init(&dss->visible, true); // Visible by default.
    return 0;
}

static int dss_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(dss_render, 0);
    //double visibility;
    dss_t *dss = (dss_t*)obj;
    painter_t painter2 = *painter;
    double lum, c, sep;
    int render_order, split_order;

    if (dss->visible.value == 0.0) return 0;
    if (!dss->hips) return 0;

    // Adjust for eye adaptation.
    // DSS is darker than dark night sky (cd/m2)
    lum = 0.0002;
    lum *= core->telescope.light_grasp;
    c = tonemapper_map(&core->tonemapper, lum);
    c = clamp(c, 0, 1);
    painter2.color[3] *= c;

    // Don't even try to display if the brightness is too low
    if (painter2.color[3] < 3.0 / 255) return 0;

    /*
     * Compute split order.
     * When we are around the poles we need a higher resolution (up to order
     * 11).  We also limit the split so that we don't split a single quad
     * too much, or the rendering would be too slow.
     *
     * Note that this could be done in hips.c, but for the moment we only
     * need to do this for the DSS survey.
     */
    sep = min(eraSepp(painter->viewport_caps[FRAME_ICRF], VEC(0, 0, +1)),
              eraSepp(painter->viewport_caps[FRAME_ICRF], VEC(0, 0, -1)));
    split_order = mix(11, 4, sep / (M_PI / 2));
    render_order = hips_get_render_order(dss->hips, painter, 2 * M_PI);
    split_order = min(split_order, render_order + 4);

    hips_render(dss->hips, &painter2, 2 * M_PI, split_order);
    return 0;
}

static int dss_update(obj_t *obj, const observer_t *obs, double dt)
{
    dss_t *dss = (dss_t*)obj;
    return fader_update(&dss->visible, dt);
}

static int dss_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    dss_t *dss = (dss_t*)obj;
    const char *title, *release_date_str;
    double release_date = 0;

    if (dss->hips) return 1;
    if (!type || !args || strcmp(type, "hips")) return 1;
    title = json_get_attr_s(args, "obs_title");
    if (!title || strcasecmp(title, "DSS colored") != 0) return 1;
    release_date_str = json_get_attr_s(args, "hips_release_date");
    if (release_date_str)
        release_date = hips_parse_date(release_date_str);
    dss->hips = hips_create(url, release_date, NULL);
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t dss_klass = {
    .id = "dss",
    .size = sizeof(dss_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = dss_init,
    .update = dss_update,
    .render = dss_render,
    .render_order = 6,
    .add_data_source = dss_add_data_source,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(dss_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(dss_klass)
