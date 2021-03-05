/* Stellarium Web Engine - Copyright (c) 2021 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Module that allows to drag selection rectangles on the sky.
 */

// Enable by default only if we compile to js.
#ifndef DRAG_SELECTION_MODULE_ENABLED
#   ifdef __EMSCRIPTEN__
#       define DRAG_SELECTION_MODULE_ENABLED 1
#   else
#       define DRAG_SELECTION_MODULE_ENABLED 0
#   endif
#endif

#if DRAG_SELECTION_MODULE_ENABLED

#include "swe.h"

typedef struct drag_selection {
    obj_t           obj;
    gesture_t       gest_pan;
    bool            active;
    double          start_pos[2];
    double          pos[2];
} drag_selection_t;

static int on_pan(const gesture_t *gest, void *user)
{
    drag_selection_t *module = user;
    double x1, y1, x2, y2;

    switch (gest->state) {
    case GESTURE_BEGIN:
        vec2_copy(gest->pos, module->start_pos);
        module->active = true;
        break;
    case GESTURE_END:
        module->active = false;
        if (core->on_rect) {
            x1 = min(module->start_pos[0], gest->pos[0]);
            y1 = min(module->start_pos[1], gest->pos[1]);
            x2 = max(module->start_pos[0], gest->pos[0]);
            y2 = max(module->start_pos[1], gest->pos[1]);
            core->on_rect(x1, y1, x2, y2);
        }
        break;
    }
    vec2_copy(gest->pos, module->pos);
    return 0;
}

static int drag_selection_init(obj_t *obj, json_value *args)
{
    drag_selection_t *module = (void*)obj;
    module->gest_pan = (gesture_t) {
        .type = GESTURE_PAN,
        .callback = on_pan,
    };
    return 0;
}

static int drag_selection_on_mouse(obj_t *obj, int id, int state,
                                   double x, double y, int buttons)
{
    drag_selection_t *module = (void*)obj;
    if (!core->on_rect) return 0;
    if (buttons != 2) return 0;
    gesture_t *gs[] = {&module->gest_pan};
    gesture_on_mouse(1, gs, id, state, x, y, module);
    return 0;
}

static int drag_selection_render(const obj_t *obj, const painter_t *painter)
{
    double size[2];
    drag_selection_t *module = (void*)obj;
    if (!module->active) return 0;

    vec2_sub(module->pos, module->start_pos, size);
    paint_2d_rect(painter, NULL, module->start_pos, size);
    return 0;
}

/*
 * Meta class declarations.
 */
static obj_klass_t drag_selection_klass = {
    .id             = "drag_selection",
    .size           = sizeof(drag_selection_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = drag_selection_init,
    .on_mouse       = drag_selection_on_mouse,
    .render         = drag_selection_render,
    .render_order   = 50,
};
OBJ_REGISTER(drag_selection_klass);

#endif
