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

EMSCRIPTEN_KEEPALIVE
void core_update_fov(double dt)
{
    double t;
    double save_fov = core->fov;

    if (core->fov_animation.duration) {
        core->fov_animation.t += dt / core->fov_animation.duration;
        t = smoothstep(0.0, 1.0, core->fov_animation.t);
        // Make sure we finish on an exact value.
        if (core->fov_animation.t >= 1.0) t = 1.0;
        if (core->fov_animation.dst_fov) {
            core->fov = mix(core->fov_animation.src_fov,
                            core->fov_animation.dst_fov, t);
        }
        if (core->fov_animation.t >= 1.0) {
            core->fov_animation.duration = 0.0;
            core->fov_animation.t = 0.0;
            core->fov_animation.dst_fov = 0.0;
        }
    }

    projection_t proj;

    const double ZOOM_FACTOR = 0.05;
    // Continuous zoom.
    core_get_proj(&proj);
    if (core->zoom) {
        core->fov *= pow(1. + ZOOM_FACTOR * (-core->zoom), dt/(1./60));
        if (core->fov > proj.max_fov)
            core->fov = proj.max_fov;
    }

    core->fov = clamp(core->fov, CORE_MIN_FOV, proj.max_fov);

    if (core->fov != save_fov)
        module_changed((obj_t*)core, "fov");
}

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
    if (!anim->duration && core->time_speed) {
        tt = core->observer->tt + dt * core->time_speed / 86400;
        obj_set_attr(&core->observer->obj, "tt", tt);
        observer_update(core->observer, true);
        return;
    }

    // Time animation.
    if (anim->duration) {
        anim->t += dt / anim->duration;
        t = smoothstep(0.0, 1.0, anim->t);
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
            anim->duration = 0.0;
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

    if (core->target.duration) {
        core->target.t += dt / core->target.duration;
        t = smoothstep(0.0, 1.0, core->target.t);
        // Make sure we finish on an exact value.
        if (core->target.t >= 1.0) t = 1.0;
        if (core->target.lock && core->target.move_to_lock) {
            // We are moving toward a potentially moving target, adjust the
            // destination
            obj_get_pos(core->target.lock, core->observer, FRAME_MOUNT, vv);
            eraC2s((double*)vv, &az, &al);
            quat_set_identity(core->target.dst_q);
            quat_rz(az, core->target.dst_q, core->target.dst_q);
            quat_ry(-al, core->target.dst_q, core->target.dst_q);
        }
        if (!core->target.lock || core->target.move_to_lock) {
            quat_slerp(core->target.src_q, core->target.dst_q, t, q);
            quat_mul_vec3(q, v, v);
            eraC2s(v, &core->observer->yaw, &core->observer->pitch);
        }
        if (core->target.t >= 1.0) {
            core->target.duration = 0.0;
            core->target.t = 0.0;
            core->target.move_to_lock = false;
        }
        // Notify the changes.
        module_changed(&core->observer->obj, "pitch");
        module_changed(&core->observer->obj, "yaw");
        observer_update(core->observer, true);
    }

    if (core->target.lock && !core->target.move_to_lock) {
        obj_get_pos(core->target.lock, core->observer, FRAME_MOUNT, v);
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

    if (frame == FRAME_OBSERVED) {
        quat_set_identity(quat);
    } else {
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(1, 0, 0), mat[0]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, -1, 0), mat[1]);
        convert_frame(obs, FRAME_OBSERVED, frame, true, VEC(0, 0, 1), mat[2]);
        mat3_to_quat(mat, quat);
        quat_normalize(quat, quat);
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
