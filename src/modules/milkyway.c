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
    texture_t       *tex;
} milkyway_t;

static int milkyway_init(obj_t *obj, json_value *args);
static int milkyway_update(obj_t *obj, const observer_t *obs, double dt);
static int milkyway_render(const obj_t *obj, const painter_t *painter);

static obj_klass_t milkyway_klass = {
    .id = "milkyway",
    .size = sizeof(milkyway_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = milkyway_init,
    .update = milkyway_update,
    .render = milkyway_render,
    .render_order = 5,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(milkyway_t, visible.target)),
        {}
    },
};

static int milkyway_init(obj_t *obj, json_value *args)
{
    milkyway_t *mw = (void*)obj;
    fader_init(&mw->visible, true);
    return 0;
}

static int milkyway_update(obj_t *obj, const observer_t *obs, double dt)
{
    milkyway_t *mw = (milkyway_t*)obj;
    return fader_update(&mw->visible, dt);
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double ra, de;
    ra = v[0] * 360 * DD2R - 90 * DD2R;
    de = (v[1] - 0.5) * 180 * DD2R;
    eraS2c(-ra, -de, out);
    out[3] = 0; // At infinity.
}

static int milkyway_render(const obj_t *obj, const painter_t *painter)
{
    /*
     * For the moment we use stellarium texture.  I guess we should get
     * a healpix or toast texture.
     *
     * Stellarium texture is in polar coordinate, with a 90° shift:
     *
     *                  Celestial North Pole
     *                          v
     *     +--------------------+--------------------+ de = +90°
     *     |                    |                    |
     *     |                    |                    |
     *     |                    |                    |
     *     +--------------------+--------------------+
     *     |                    |                    |
     *     |                    |                    |
     *     |                    |                    |
     *     +--------------------+----------+---------+ de = -90°
     *  ra:90°                 270°        0°       90°
     */
    milkyway_t *mw = (milkyway_t*)obj;
    painter_t painter2;
    projection_t proj_spherical = {
        .name       = "spherical",
        .backward   = spherical_project,
    };
    double UV[][2] = {{0.0, 1.0}, {0.0, 0.0},
                      {1.0, 1.0}, {1.0, 0.0}};
    double alpha = 0.3;
    const int div = 32;
    if (mw->visible.value == 0.0) return 0;

    if (!mw->tex) {
        mw->tex = texture_from_url("asset://textures/milkyway.jpg", 0);
        assert(mw->tex);
    }
    painter2 = *painter;
    vec3_mul(alpha, painter2.color, painter2.color);

    // Adjust for eye adaptation.
    // Should this be done in the painter?
    painter2.color[3] /= pow(2.5, 0.5 * core->vmag_shift);

    paint_quad(&painter2, FRAME_ICRS, mw->tex, NULL, UV, &proj_spherical, div);
    return 0;
}

OBJ_REGISTER(milkyway_klass)
