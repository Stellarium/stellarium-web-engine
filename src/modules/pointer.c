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


static int pointer_render(const obj_t *obj, const painter_t *painter_)
{
    int i;
    double pos[4], pos2d[2];
    double mag, size, luminance, radius = NAN;
    double white[4] = {1, 1, 1, 1};
    const double T = 2.0;    // Animation period.
    double r, transf[4][4];
    obj_t *selection = core->selection;
    painter_t painter = *painter_;
    vec4_set(painter.color, 0.34, 0.59, 1.0, 0.9);
    if (!selection) return 0;

    // If the selection has a custom rendering method, we use it.
    if (selection->klass->render_pointer) {
        if (selection->klass->render_pointer(selection, &painter) == 0)
            return 0;
    }

    obj_get_pos_observed(selection, painter.obs, pos);
    mat4_mul_vec3(painter.obs->ro2v, pos, pos);
    if (!project(painter.proj, PROJ_TO_WINDOW_SPACE, 2, pos, pos2d))
        return 0;

    // Empirical formula to compute the pointer size.
    mag = core_get_observed_mag(selection->vmag);
    core_get_point_for_mag(mag, &size, &luminance);
    if (obj_has_attr(selection, "radius")) {
        obj_get_attr(selection, "radius", "f", &radius);
        size = max(size, radius);
    }

    if (isnan(radius)) {
        size = 4000.0 * size / core->fov;
        size = max(size, 32);
        size *= 1 + 0.05 * (sin(sys_get_unix_time() / T * 2 * M_PI) + 1);
        symbols_paint(&painter, SYMBOL_POINTER, pos2d, VEC(size, size),
                      white, 0);
    } else {
        // Draw four strokes around the object.
        for (i = 0; i < 4; i++) {
            r = size / core->fov * core->win_size[0] + 5;
            r = max(r, 8);
            r += 0.8 * (sin(sys_get_unix_time() / T * 2 * M_PI) + 1);
            mat4_set_identity(transf);
            mat4_itranslate(transf, pos2d[0], pos2d[1], 0);
            mat4_rz(i * 90 * DD2R, transf, transf);
            mat4_itranslate(transf, r, 0, 0);
            mat4_iscale(transf, 8, 1, 1);
            paint_2d_line(&painter, transf, VEC(0, 0), VEC(1, 0));
        }
    }
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t pointer_klass = {
    .id = "pointer",
    .size = sizeof(pointer_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = pointer_render,
    .render_order = 199, // Just before the ui.
};

OBJ_REGISTER(pointer_klass)
