/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

enum {
    GEO_DISABLED,
    GEO_SEARCHING,
    GEO_FOUND,
    GEO_UNSUPPORTED,
};

typedef struct geolocation {
    obj_t       obj;
    bool        active;
    int         state;
} geolocation_t;

static void set_state(geolocation_t *geo, int state)
{
    if (state == geo->state) return;
    geo->state = state;
    obj_changed((obj_t*)geo, "state");
}

static int geolocation_init(obj_t *obj, json_value *args)
{
    geolocation_t *geo = (geolocation_t*)obj;
    // Check if geolocation is supported.
    if (sys_get_position(NULL, NULL, NULL, NULL) < 0) {
        set_state(geo, GEO_UNSUPPORTED);
        return 0;
    }
    // For the moment active by default.
    geo->active = true;
    return 0;
}

static int geolocation_update(obj_t *obj, const observer_t *obs_, double dt)
{
    geolocation_t *geo = (geolocation_t*)obj;
    observer_t *obs = (observer_t*)obs_;
    double lat, lon, alt, accuracy;
    int r;

    if (geo->state == GEO_UNSUPPORTED) return 0;
    if (!geo->active) {
        set_state(geo, GEO_DISABLED);
        return 0;
    }
    r = sys_get_position(&lat, &lon, &alt, &accuracy);
    switch (r) {
    case 0:
        set_state(geo, GEO_FOUND);
        obj_set_attr((obj_t*)obs, "longitude", "f", lon);
        obj_set_attr((obj_t*)obs, "latitude", "f", lat);
        obj_set_attr((obj_t*)obs, "elevation", "f", alt);
        break;
    case 1:
        set_state(geo, GEO_SEARCHING);
        break;
    case -1:
        set_state(geo, GEO_UNSUPPORTED);
        break;
    }

    return 0;
}

static obj_klass_t geolocation_klass = {
    .id             = "geolocation",
    .size           = sizeof(geolocation_t),
    .flags          = OBJ_MODULE,
    .init           = geolocation_init,
    .update         = geolocation_update,
    .render_order   = 0,
    .attributes = (attribute_t[]) {
        PROPERTY("active", "b", MEMBER(geolocation_t, active)),
        PROPERTY("state", "d", MEMBER(geolocation_t, state)),
        {}
    },
};
OBJ_REGISTER(geolocation_klass)
