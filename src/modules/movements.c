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
    gesture_t       gest_pinch;
} movements_t;


// Convert screen position to mount coordinates.
static void screen_to_mount(
        const observer_t *obs, const projection_t *proj,
        const double screen_pos[2], double p[3])
{
    double pos[4] = {screen_pos[0], screen_pos[1]};
    unproject(proj, pos, pos);
    vec3_normalize(pos, pos);
    convert_frame(obs, FRAME_VIEW, FRAME_MOUNT, true, pos, p);
}


static int on_pan(const gesture_t *gest, void *user)
{
    double sal, saz, dal, daz;
    double pos[3];
    static double start_pos[3];
    projection_t proj;

    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, gest->pos, pos);
    if (gest->state == GESTURE_BEGIN)
        vec3_copy(pos, start_pos);

    eraC2s(start_pos, &saz, &sal);
    eraC2s(pos, &daz, &dal);
    core->observer->yaw += (saz - daz);
    core->observer->pitch += (sal - dal);
    core->observer->pitch = clamp(core->observer->pitch, -M_PI / 2, +M_PI / 2);

    obj_set_attr(&core->obj, "lock", NULL);
    observer_update(core->observer, true);
    // Notify the changes.
    module_changed(&core->observer->obj, "pitch");
    module_changed(&core->observer->obj, "yaw");
    return 0;
}

static int on_click(const gesture_t *gest, void *user)
{
    obj_t *obj;
    bool r = false;
    if (core->on_click)
        r = core->on_click(gest->pos[0], gest->pos[1]);
    // Default behavior: select an object.
    if (!r) {
        obj = core_get_obj_at(gest->pos[0], gest->pos[1], 18);
        obj_set_attr(&core->obj, "selection", obj);
        obj_release(obj);
    }
    core->clicks++;
    module_changed((obj_t*)core, "clicks");
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
                              double x, double y, int buttons)
{
    movements_t *movs = (void*)obj;
    if (buttons != 1) return 0;
    id = get_touch_index(id + 1);
    if (id == -1) return 0;
    if (state == -1) state = core->inputs.touches[id].down[0];
    if (state == 0) core->inputs.touches[id].id = 0; // Remove.
    core->inputs.touches[id].pos[0] = x;
    core->inputs.touches[id].pos[1] = y;
    core->inputs.touches[id].down[0] = state == 1;
    if (core->gui_want_capture_mouse) return 0;
    gesture_t *gs[] = {&movs->gest_pan, &movs->gest_pinch, &movs->gest_click};
    gesture_on_mouse(3, gs, id, state, x, y, movs);
    return 0;
}

static int movements_on_zoom(obj_t *obj, double k, double x, double y)
{
    double fov, pos_start[3], pos_end[3];
    double sal, saz, dal, daz;
    projection_t proj;

    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, VEC(x, y), pos_start);
    obj_get_attr(&core->obj, "fov", &fov);
    fov /= k;
    fov = clamp(fov, CORE_MIN_FOV, proj.klass->max_ui_fov);
    obj_set_attr(&core->obj, "fov", fov);
    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, VEC(x, y), pos_end);

    // Adjust lat/az to keep the mouse point at the same position.
    eraC2s(pos_start, &saz, &sal);
    eraC2s(pos_end, &daz, &dal);
    core->observer->yaw += (saz - daz);
    core->observer->pitch += (sal - dal);
    core->observer->pitch = clamp(core->observer->pitch, -M_PI / 2, +M_PI / 2);

    // Notify the changes.
    module_changed(&core->observer->obj, "pitch");
    module_changed(&core->observer->obj, "yaw");
    return 0;
}

static int movements_update(obj_t *obj, double dt)
{
    const double ZOOM_FACTOR = 1.05;
    const double MOVE_SPEED  = 1 * DD2R;

    if (core->inputs.keys[KEY_RIGHT])
        core->observer->yaw += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_LEFT])
        core->observer->yaw -= MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_UP])
        core->observer->pitch += MOVE_SPEED * core->fov;
    if (core->inputs.keys[KEY_DOWN])
        core->observer->pitch -= MOVE_SPEED * core->fov;
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
    .on_zoom        = movements_on_zoom,
    .update         = movements_update,
    .render_order   = -1,
};
OBJ_REGISTER(movements_klass);
