/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Module that handles the movements from mouse and keyboard inputs
 * Should this be renamed to navigation?
 */

#include "swe.h"

typedef struct movements {
    obj_t           obj;
    gesture_t       gest_pan;
    gesture_t       gest_click;
    gesture_t       gest_hover;
    gesture_t       gest_pinch;
} movements_t;


// Convert screen position to observed coordinates.
static void screen_to_observed(
        const observer_t *obs, const projection_t *proj,
        const double screen_pos[2], double p[3])
{
    double pos[4] = {screen_pos[0], screen_pos[1]};

    // Convert to NDC coordinates.
    // Could be done in the projector?
    pos[0] = pos[0] / proj->window_size[0] * 2 - 1;
    pos[1] = -1 * (pos[1] / proj->window_size[1] * 2 - 1);
    project(proj, PROJ_BACKWARD, 4, pos, pos);
    convert_frame(obs, FRAME_VIEW, FRAME_OBSERVED, true, pos, p);
}


static int on_pan(const gesture_t *gest, void *user)
{
    double sal, saz, dal, daz;
    double pos[3];
    static double start_pos[3];
    projection_t proj;

    projection_init(&proj, core->proj, core->fov,
                    core->win_size[0], core->win_size[1]);
    screen_to_observed(core->observer, &proj, gest->pos, pos);
    if (gest->state == GESTURE_BEGIN)
        vec3_copy(pos, start_pos);

    eraC2s(start_pos, &saz, &sal);
    eraC2s(pos, &daz, &dal);
    core->observer->azimuth += (saz - daz);
    core->observer->altitude += (sal - dal);
    core->observer->altitude = clamp(core->observer->altitude,
                                     -M_PI / 2, +M_PI / 2);
    core->fast_mode = true;
    obj_set_attr(&core->obj, "lock", NULL);
    observer_update(core->observer, true);
    // Notify the changes.
    module_changed(&core->observer->obj, "altitude");
    module_changed(&core->observer->obj, "azimuth");
    return 0;
    return 0;
}

static int on_click(const gesture_t *gest, void *user)
{
    obj_t *obj;
    if (!core->ignore_clicks) {
        obj = core_get_obj_at(gest->pos[0], gest->pos[1], 18);
        obj_set_attr(&core->obj, "selection", obj);
        obj_release(obj);
    }
    core->clicks++;
    module_changed((obj_t*)core, "clicks");
    return 0;
}

static int on_hover(const gesture_t *gest, void *user)
{
    obj_t *obj;
    obj = core_get_obj_at(gest->pos[0], gest->pos[1], 18);
    obj_set_attr(&core->obj, "hovered", obj);
    obj_release(obj);
    return 0;
}

static int on_pinch(const gesture_t *gest, void *user)
{
    static double start_fov = 0;
    if (gest->state == GESTURE_BEGIN) {
        start_fov = core->fov;
    }
    core->fov = start_fov / gest->pinch;
    module_changed((obj_t*)core, "fov");
    return 0;
}

static int movements_init(obj_t *obj, json_value *args)
{
    movements_t *movs = (void*)obj;
    movs->gest_pan = (gesture_t) {
        .type = GESTURE_PAN,
        .callback = on_pan,
    };
    movs->gest_click = (gesture_t) {
        .type = GESTURE_CLICK,
        .callback = on_click,
    };
    movs->gest_hover = (gesture_t) {
        .type = GESTURE_HOVER,
        .callback = on_hover,
    };
    movs->gest_pinch = (gesture_t) {
        .type = GESTURE_PINCH,
        .callback = on_pinch,
    };
    return 0;
}

static int get_touch_index(int id)
{
    int i;
    assert(id != 0);
    for (i = 0; i < ARRAY_SIZE(core->inputs.touches); i++) {
        if (core->inputs.touches[i].id == id) return i;
    }
    // Create new touch.
    for (i = 0; i < ARRAY_SIZE(core->inputs.touches); i++) {
        if (!core->inputs.touches[i].id) {
            core->inputs.touches[i].id = id;
            return i;
        }
    }
    return -1; // No more space.
}

static int movements_on_mouse(obj_t *obj, int id, int state,
                              double x, double y)
{
    movements_t *movs = (void*)obj;
    id = get_touch_index(id + 1);
    if (id == -1) return 0;
    if (state == -1) state = core->inputs.touches[id].down[0];
    if (state == 0) core->inputs.touches[id].id = 0; // Remove.
    core->inputs.touches[id].pos[0] = x;
    core->inputs.touches[id].pos[1] = y;
    core->inputs.touches[id].down[0] = state == 1;
    if (core->gui_want_capture_mouse) return 0;
    gesture_t *gs[] = {&movs->gest_pan, &movs->gest_pinch,
                       &movs->gest_click, &movs->gest_hover};
    gesture_on_mouse(4, gs, id, state, x, y);
    return 0;
}

static int movements_update(obj_t *obj, double dt)
{
    const double ZOOM_FACTOR = 1.05;
    const double MOVE_SPEED  = 1 * DD2R;

    if (core->inputs.keys[KEY_RIGHT])
        core->observer->azimuth += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_LEFT])
        core->observer->azimuth -= MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_UP])
        core->observer->altitude += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_DOWN])
        core->observer->altitude -= MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_PAGE_UP])
        core->fov /= ZOOM_FACTOR;
    if (core->inputs.keys[KEY_PAGE_DOWN])
        core->fov *= ZOOM_FACTOR;
    return 0;
}

/*
 * Meta class declarations.
 */
static obj_klass_t movements_klass = {
    .id             = "movements",
    .size           = sizeof(movements_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = movements_init,
    .on_mouse       = movements_on_mouse,
    .update         = movements_update,
    .render_order   = -1,
};
OBJ_REGISTER(movements_klass);
