/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
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


// Normalized distance from the screen center at which dragging the sky
// starts to rotate it around the view direction instead of sliding it, and
// distance at which tangential movements fully rotate it.  1.0 corresponds
// to the screen edges.
static const double EDGE_ROTATE_START = 0.6;
static const double EDGE_ROTATE_FULL  = 1.0;

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

// Compute the shortest arc rotation quaternion from unit vector a to b.
static void quat_from_vectors(const double a[3], const double b[3],
                              double q[4])
{
    double c[3];
    vec3_cross(a, b, c);
    q[0] = 1.0 + vec3_dot(a, b);
    if (q[0] < 1e-12) { // Opposite vectors: ignore.
        quat_set_identity(q);
        return;
    }
    vec3_copy(c, q + 1);
    quat_normalize(q, q);
}

/*
 * Apply a rotation, expressed in the mount frame, to the view.
 * The rotation maps the sky direction that a screen pixel used to show to
 * the one it shows afterward.
 */
static void rotate_view(const double rot[4])
{
    double q[4];
    // Never let an invalid rotation corrupt the view (for example if the
    // window size is not known yet).
    if (!isfinite(rot[0] + rot[1] + rot[2] + rot[3])) return;
    quat_mul(rot, core->observer->view_q, q);
    observer_set_view_q(core->observer, q);
    observer_update(core->observer, true);
}

/*
 * Drag the sky so that the point under screen position p0 moves under p1.
 *
 * The point always stays under the cursor, but the movement is a mix of a
 * slide and a rotation around the screen center: near the center we only
 * slide the sky, and toward the screen edges the movement tangential to the
 * center rotates the sky around the view direction instead.
 */
static void drag_view(const projection_t *proj,
                      const double p0[2], const double p1[2])
{
    double center[2], r[2], u0[3], u1[3], c[3], a0[3], a1[3], n[3], s[3];
    double angle, w, q_roll[4], q_slide[4], q[4];

    vec2_mul(0.5, proj->window_size, center);
    screen_to_mount(core->observer, proj, center, c);
    screen_to_mount(core->observer, proj, p0, u0);
    screen_to_mount(core->observer, proj, p1, u1);

    // Rotation weight, from the distance of the cursor to the screen
    // center, normalized so that the screen edges are at 1.
    r[0] = (p1[0] - center[0]) / center[0];
    r[1] = (p1[1] - center[1]) / center[1];
    w = smoothstep(EDGE_ROTATE_START, EDGE_ROTATE_FULL, vec2_norm(r));

    // Angle swept by the cursor around the view direction.
    vec3_addk(u0, c, -vec3_dot(u0, c), a0);
    vec3_addk(u1, c, -vec3_dot(u1, c), a1);
    vec3_cross(a0, a1, n);
    angle = atan2(vec3_dot(n, c), vec3_dot(a0, a1));

    // Rotate around the view direction by the weighted angle, then slide
    // the rest of the way.
    quat_from_axis(q_roll, -w * angle, c[0], c[1], c[2]);
    quat_mul_vec3(q_roll, u1, s);
    quat_from_vectors(s, u0, q_slide);
    quat_mul(q_slide, q_roll, q);
    rotate_view(q);
}

// Compute an orthonormal frame from two unit vectors.
static void frame_from_vectors(const double a[3], const double b[3],
                               double m[3][3])
{
    vec3_add(a, b, m[0]);
    vec3_normalize(m[0], m[0]);
    vec3_sub(b, a, m[1]);
    vec3_addk(m[1], m[0], -vec3_dot(m[1], m[0]), m[1]);
    vec3_normalize(m[1], m[1]);
    vec3_cross(m[0], m[1], m[2]);
}

static int on_pan(const gesture_t *gest, void *user)
{
    static double last_pos[2];
    projection_t proj;

    obj_set_attr(&core->obj, "lock", NULL);
    if (gest->state != GESTURE_BEGIN) {
        core_get_proj(&proj);
        drag_view(&proj, last_pos, gest->pos);
    }
    vec2_copy(gest->pos, last_pos);
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
    static double last_touches[2][2];
    double a0[3], b0[3], a1[3], b1[3], f0[3][3], f1[3][3], rot[3][3], q[4];
    projection_t proj;

    if (gest->state == GESTURE_BEGIN) {
        start_fov = core->fov;
        memcpy(last_touches, gest->touches, sizeof(last_touches));
        return 0;
    }

    // Sky positions under the two touches before the move.
    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, last_touches[0], a0);
    screen_to_mount(core->observer, &proj, last_touches[1], b0);

    core->fov = clamp(start_fov / gest->pinch,
                      CORE_MIN_FOV, proj.klass->max_ui_fov);
    module_changed((obj_t*)core, "fov");

    // Rotate the view so that the sky follows the two touches: this both
    // drags and twists the sky.
    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, gest->touches[0], a1);
    screen_to_mount(core->observer, &proj, gest->touches[1], b1);
    frame_from_vectors(a0, b0, f0);
    frame_from_vectors(a1, b1, f1);
    mat3_transpose(f1, f1);
    mat3_mul(f0, f1, rot);
    mat3_to_quat(rot, q);
    rotate_view(q);

    memcpy(last_touches, gest->touches, sizeof(last_touches));
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
    double fov, pos_start[3], pos_end[3], q[4];
    projection_t proj;

    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, VEC(x, y), pos_start);
    obj_get_attr(&core->obj, "fov", &fov);
    fov /= k;
    fov = clamp(fov, CORE_MIN_FOV, proj.klass->max_ui_fov);
    obj_set_attr(&core->obj, "fov", fov);
    core_get_proj(&proj);
    screen_to_mount(core->observer, &proj, VEC(x, y), pos_end);

    // Rotate the view to keep the mouse point at the same position.
    quat_from_vectors(pos_end, pos_start, q);
    rotate_view(q);
    return 0;
}

static int movements_update(obj_t *obj, double dt)
{
    const double ZOOM_FACTOR = 1.05;
    const double MOVE_SPEED  = 1 * DD2R;

    const double a = MOVE_SPEED * core->fov;
    const bool *keys = core->inputs.keys;
    double q[4];

    // Rotate the view around its own up and side axes.
    if (keys[KEY_RIGHT] || keys[KEY_LEFT] || keys[KEY_UP] || keys[KEY_DOWN]) {
        vec4_copy(core->observer->view_q, q);
        if (keys[KEY_RIGHT]) quat_rz(+a, q, q);
        if (keys[KEY_LEFT])  quat_rz(-a, q, q);
        if (keys[KEY_UP])    quat_ry(-a, q, q);
        if (keys[KEY_DOWN])  quat_ry(+a, q, q);
        observer_set_view_q(core->observer, q);
    }
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
