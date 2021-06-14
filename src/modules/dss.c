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
    double visibility;
    dss_t *dss = (dss_t*)obj;
    painter_t painter2 = *painter;
    double lum, c, sep, ratio;
    int render_order, split_order;

    if (dss->visible.value == 0.0) return 0;
    if (!dss->hips) return 0;

    // For large FOV we use the milky way texture
    visibility = smoothstep(20 * DD2R, 10 * DD2R, core->fov);
    painter2.color[3] *= dss->visible.value;

    // Adjust for eye adaptation.
    // 0.02 cd/m2 luminance should more or less match the brightest
    // non-saturated pixels brightness inside the DSS images.
    lum = 0.02;
    lum *= core->telescope.light_grasp;
    lum /= pow(core->telescope.magnification, 2);
    c = tonemapper_map(&core->tonemapper, lum);
    // Ad-hoc integration of the Bortle scale
    c *= 1.0 / (6.0 / 8.0) * (9.0 - core->bortle_index) / 8.0;
    c = max(0, c);
    c *= visibility;
    c = min(c, 1.2);

    // limit mag = 4 -> no dss
    // limit mag > 14 -> 100% dss
    if (core->display_limit_mag < 14.0) {
        ratio = (14.0 - core->display_limit_mag) / 10;
        ratio = clamp(ratio, 0.0, 1.0);
        c *= 1.0 - ratio;
    }

    vec4_mul(c, painter2.color, painter2.color);

    // Don't even try to display if the brightness is too low
    if (painter2.color[3] < 3.0 / 255) return 0;

    /*
     * Compute split order.
     * When we are around the poles we need a higher resolution (up to order
     * 11).  We also limit the split so that we don't split a single quad
     * too much, or the rendering would be too slow.
     *
     * Note 1: this could be done in hips.c, but for the moment we only
     * need to do this for the DSS survey.
     * Note 2: instead of this heuristic we should compute the exact healpix
     * distortion at a given position.
     */
    sep = min(eraSepp(painter->clip_info[FRAME_ICRF].bounding_cap,
                      VEC(0, 0, +1)),
              eraSepp(painter->clip_info[FRAME_ICRF].bounding_cap,
                      VEC(0, 0, -1)));
    split_order = mix(12, 4, clamp(sep / (40 * DD2R), 0, 1));

    render_order = hips_get_render_order(dss->hips, painter);
    split_order = min(split_order, render_order + 3);

    hips_render(dss->hips, &painter2, NULL, split_order);
    return 0;
}

static int dss_update(obj_t *obj, double dt)
{
    dss_t *dss = (dss_t*)obj;
    return fader_update(&dss->visible, dt);
}

static int dss_add_data_source(obj_t *obj, const char *url, const char *key)
{
    dss_t *dss = (dss_t*)obj;
    hips_delete(dss->hips);
    dss->hips = hips_create(url, 0, NULL);
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
