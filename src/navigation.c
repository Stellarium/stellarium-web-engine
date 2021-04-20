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
    double ro2m[3][3] = MAT3_IDENTITY;

    switch (frame) {
    case FRAME_OBSERVED:
        mat3_set_identity(ro2m);
        break;
    case FRAME_ICRF:
        mat3_copy(obs->rh2i, ro2m);
        break;
    default:
        assert(false);
    }

    if (memcmp(ro2m, obs->ro2m, sizeof(ro2m)) == 0) return;
    mat3_copy(ro2m, obs->ro2m);
    observer_update(core->observer, true);
}

void core_update_fov(double dt)
{
    double t;
    double save_fov = core->fov;
    projection_t proj;
    const double ZOOM_FACTOR = 0.05;
    typeof(core->fov_animation)* anim = &core->fov_animation;

    if (anim->src_time) {
        t = smoothstep(anim->src_time, anim->dst_time, core->clock);
        if (anim->dst_fov)
            core->fov = mix(anim->src_fov, anim->dst_fov, t);
        if (t >= 1.0)
            memset(anim, 0, sizeof(*anim));
    }

    // Continuous zoom.
    core_get_proj(&proj);
    if (core->zoom) {
        core->fov *= pow(1. + ZOOM_FACTOR * (-core->zoom), dt/(1./60));
        if (core->fov > proj.klass->max_fov)
            core->fov = proj.klass->max_fov;
    }

    core->fov = clamp(core->fov, CORE_MIN_FOV, proj.klass->max_fov);
    if (core->fov != save_fov)
        module_changed((obj_t*)core, "fov");
}


// Weak so that we can easily replace the navigation algorithm.
__attribute__((weak))
void core_update_observer(double dt)
{
    core_update_fov(dt);
    core_update_time(dt);
    core_update_direction(dt);
    core_update_mount(dt);
}
