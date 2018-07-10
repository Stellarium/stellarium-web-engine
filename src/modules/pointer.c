/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Render a pointer around the selected object.
// This is convenient to put this as a module, because it has to be rendered
// Before the UI.

typedef struct pointer {
    obj_t           obj;
} pointer_t;

static int pointer_render(const obj_t *obj, const painter_t *painter);

static obj_klass_t pointer_klass = {
    .id = "pointer",
    .size = sizeof(pointer_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = pointer_render,
    .render_order = 199, // Just before the ui.
};

OBJ_REGISTER(pointer_klass)

static int pointer_render(const obj_t *obj, const painter_t *painter)
{
    int i;
    double pos[4], pos2d[2];
    double mag, size, luminance, angle, radius = NAN;
    double color[4] = {0.34, 0.59, 1.0, 0.9};
    obj_t *selection = core->selection;
    if (!selection) return 0;
    obj_get_pos_observed(selection, painter->obs, pos);
    mat4_mul_vec3(painter->obs->ro2v, pos, pos);
    if (!project(painter->proj, PROJ_TO_NDC_SPACE, 2, pos, pos2d))
        return 0;

    // Empirical formula to compute the pointer size.
    mag = core_get_observed_mag(selection->vmag);
    core_get_point_for_mag(mag, &size, &luminance);
    if (obj_has_attr(selection, "radius")) {
        obj_get_attr(selection, "radius", "f", &radius);
        size = max(size, radius);
    }
    angle = fmod(0.5 * sys_get_unix_time(), 2 * M_PI);

    if (isnan(radius)) {
        size = 4000.0 * size / core->fov;
        size = max(size, 32);
        symbols_paint(painter, "POIN", pos2d, size, color, angle);
    } else {
        // Draw four strokes around the object.
        // XXX: a bit ugly code.
        for (i = 0; i < 4; i++) {
            double p[2] = {1, 0};
            double r;
            double scaling = painter->proj->scaling[0] /
                             painter->proj->scaling[1];
            r = size / core->fov * 2 + 0.02;
            r = max(r, 0.04);
            r += 0.01 * (sin(angle * 4) + 1);
            vec2_rotate(angle + i * 90 * DD2R, p, p);
            p[1] *= scaling;
            vec2_addk(pos2d, p, r, p);
            symbols_paint(painter, "POIN2", p, 16, color,
                          angle + i * 90 * DD2R);
        }
    }
    return 0;
}
