/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial license.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "navigation.h"
#include "swe.h"

/*
 * Animate between two times in such a way to minimize the visual movements
 */
static double smart_time_mix(double src_tt, double dst_tt, double t)
{
    double dt, y4, y, d, f;
    int sign = 1;

    dt = dst_tt - src_tt;
    if (dt < 0) {
        sign = -1;
        dt = -dt;
    }

    y4 = floor(dt / (4 * ERFA_DJY));
    dt -= y4 * (4 * ERFA_DJY);
    y = floor(dt / ERFA_DJY);
    dt -= y * ERFA_DJY;
    d = floor(dt);
    f = dt - d;

    y4 = round(mix(0, y4, t));
    y = round(mix(0, y, t));
    d = round(mix(0, d, t));
    f = mix(0, f, t);

    return src_tt + sign * (y4 * 4 * ERFA_DJY + y * ERFA_DJY + d + f);
}



// Update the core time animation.
void core_update_time(double dt)
{
    typeof(core->time_animation) *anim = &core->time_animation;
    double t, tt;

    // Normal time increase.
    if (!anim->src_time && core->time_speed) {
        tt = core->observer->tt + dt * core->time_speed / 86400;
        obj_set_attr(&core->observer->obj, "tt", tt);
        observer_update(core->observer, true);
        return;
    }

    // Time animation.
    if (anim->src_time) {
        t = smoothstep(anim->src_time, anim->dst_time, core->clock);
        switch (anim->mode) {
        case 0:
            tt = mix(anim->src_tt, anim->dst_tt, t);
            break;
        case 1:
            tt = smart_time_mix(anim->src_tt, anim->dst_tt, t);
            break;
        default:
            assert(false);
            return;
        }
        obj_set_attr(&core->observer->obj, "tt", tt);
        if (t >= 1.0) {
            anim->src_time = 0.0;
            anim->dst_time = 0.0;
            anim->dst_utc = NAN;
            module_changed((obj_t*)core, "time_animation_target");
        }
        observer_update(core->observer, true);
        return;
    }
}

void core_update_direction(double dt)
{
    double v[4] = {1, 0, 0, 0}, q[4], t, az, al, vv[4];
    typeof(core->target) *anim = &core->target;

    if (anim->src_time) {
        t = smoothstep(anim->src_time, anim->dst_time, core->clock);
        if (anim->lock && anim->move_to_lock) {
            // We are moving toward a potentially moving target, adjust the
            // destination
            obj_get_pos(anim->lock, core->observer, FRAME_MOUNT, vv);
            eraC2s((double*)vv, &az, &al);
            quat_set_identity(anim->dst_q);
            quat_rz(az, anim->dst_q, anim->dst_q);
            quat_ry(-al, anim->dst_q, anim->dst_q);
        }
        if (!anim->lock || anim->move_to_lock) {
            quat_slerp(anim->src_q, anim->dst_q, t, q);
            quat_mul_vec3(q, v, v);
            eraC2s(v, &core->observer->yaw, &core->observer->pitch);
        }
        if (t >= 1.0) {
            anim->src_time = 0.0;
            anim->dst_time = 0.0;
            anim->move_to_lock = false;
        }
        // Notify the changes.
        module_changed(&core->observer->obj, "pitch");
        module_changed(&core->observer->obj, "yaw");
        observer_update(core->observer, true);
    }

    if (anim->lock && !anim->move_to_lock) {
        obj_get_pos(anim->lock, core->observer, FRAME_MOUNT, v);
        eraC2s(v, &core->observer->yaw, &core->observer->pitch);
        // Notify the changes.
        module_changed(&core->observer->obj, "pitch");
        module_changed(&core->observer->obj, "yaw");
        observer_update(core->observer, true);
    }
}

// Update the observer mount quaternion.
void core_update_mount(double dt)
{
    observer_t *obs = core->observer;
    int frame = core->mount_frame;
    double quat[4], mat[3][3];
    const double speed = 4;
    const double FLIP_Y_AXIS_MAT[3][3] = {{1, 0, 0},
                                          {0, -1, 0},
                                          {0, 0, 1}};

    switch (frame) {
    case FRAME_OBSERVED:
        quat_set_identity(quat);
        break;
    // XXX: not clear why we cannot use the same code for ICRF and Ecliptic.
    case FRAME_ICRF:
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(1, 0, 0), mat[0]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, -1, 0), mat[1]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, 0, 1), mat[2]);
        mat3_to_quat(mat, quat);
        quat_normalize(quat, quat);
        break;
    case FRAME_ECLIPTIC:
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(1, 0, 0), mat[0]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, 1, 0), mat[1]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, 0, 1), mat[2]);
        mat3_mul(FLIP_Y_AXIS_MAT, mat, mat); // Could we avoid this?
        mat3_to_quat(mat, quat);
        quat_normalize(quat, quat);
        break;
    default:
        assert(false);
    }

    if (vec4_equal(quat, obs->mount_quat)) return;
    quat_rotate_towards(obs->mount_quat, quat, dt * speed, obs->mount_quat);
    observer_update(core->observer, true);
}

// Weak so that we can easily replace the navigation algorithm.
__attribute__((weak))
void core_update_observer(double dt)
{
    core_update_time(dt);
    core_update_direction(dt);
    core_update_mount(dt);
}
