/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

/*
 * Declaration of a sky object of type 'coordinates' so that we can select
 * generic position by coordinates.
 */

typedef struct coordinates {
    obj_t obj;
    double po[3]; // In Ra/Dec J2000, Observer Centric.
} coordinates_t;

static int coordinates_init(obj_t *obj, json_value *args)
{
    strncpy(obj->type, "Coo", 4);
    return 0;
}

static int coordinates_get_info(const obj_t *obj, const observer_t *obs,
                                int info, void *out)
{
    coordinates_t *coo = (void*)obj;
    double pvo[2][4] = {};
    switch (info) {
    case INFO_PVO:
        convert_frame(obs, FRAME_JNOW, FRAME_ICRF, true, coo->po, pvo[0]);
        memcpy(out, pvo, sizeof(pvo));
        return 0;
    }
    return 1;
}

static void coordinates_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user,
             const char *cat, const char *str))
{
    coordinates_t *coo = (void*)obj;
    char buf_ra[32], buf_de[32], buf[128];
    double ra, de;
    eraC2s(coo->po, &ra, &de);
    format_angle(buf_ra, ra, 'h', 1, NULL);
    format_angle(buf_de, de, 'd', 1, NULL);
    snprintf(buf, sizeof(buf), "%s / %s", buf_ra, buf_de);
    f(obj, user, "RA/DE", buf);
}

/*
 * Meta class declarations.
 */
static obj_klass_t coordinates_klass = {
    .id                     = "coordinates",
    .size                   = sizeof(coordinates_t),
    .init                   = coordinates_init,
    .get_info               = coordinates_get_info,
    .get_designations       = coordinates_get_designations,
    .attributes = (attribute_t[]) {
        PROPERTY(pos, TYPE_V3, MEMBER(coordinates_t, po)),
        {}
    },
};
OBJ_REGISTER(coordinates_klass)
