/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

typedef struct sensors {
    obj_t       obj;
    bool        active;
    bool        started;
    double      acc[3];
    double      mag[3];
} sensors_t;

static int sensors_update(obj_t *obj, const observer_t *obs, double dt);

static obj_klass_t sensors_klass = {
    .id             = "sensors",
    .size           = sizeof(sensors_t),
    .flags          = OBJ_MODULE,
    .update         = sensors_update,
    .render_order   = 0,
    .attributes = (attribute_t[]) {
        PROPERTY("active", "b", MEMBER(sensors_t, active)),
        {}
    },
};

OBJ_REGISTER(sensors_klass)

static int sensors_update(obj_t *obj, const observer_t *obs_, double dt)
{
    observer_t *obs = (observer_t*)obs_;
    sensors_t *sensors = (sensors_t*)obj;
    double avg, acc[3], mag[3], v[3], roll, pitch, yaw;
    int r;

    if (!sensors->active && !sensors->started) return 0;

    r = sys_device_sensors(sensors->active, acc, mag);
    if (r) {
        return 0;
    }
    if (!sensors->active) {
        sensors->started = false;
        return 0;
    }

    avg = mix(0.01, 0.1, min(core->fov / (130 * DD2R), 1.0));
    if (!sensors->started) avg = 1.0;
    sensors->started = true;
    vec3_mix(sensors->acc, acc, avg, sensors->acc);
    vec3_mix(sensors->mag, mag, avg, sensors->mag);
    vec3_copy(sensors->acc, v);

    roll = atan2(-v[0], v[1]);
    pitch = atan2(-v[2], sqrt(v[0] * v[0] + v[1] * v[1]));

    vec3_copy(sensors->mag, v);
    vec2_rotate(-roll, v, v);
    vec2_rotate(pitch, &v[1], &v[1]);
    yaw = atan2(-v[0], -v[2]);

    obj_set_attr((obj_t*)obs, "roll", "f", roll);
    obj_set_attr((obj_t*)obs, "altitude", "f", pitch);
    obj_set_attr((obj_t*)obs, "azimuth", "f", yaw);
    obs->dirty = true;

    return 0;
}
