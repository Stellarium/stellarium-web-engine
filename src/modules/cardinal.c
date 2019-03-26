/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#define D 0.7071067811865476 // Sqrt(2)/2

static struct {
    const char *text;
    double pos[3];
} POINTS[] = {
    {"N" , { 1,  0,  0}},
    {"E" , { 0,  1,  0}},
    {"S" , {-1,  0,  0}},
    {"W" , { 0, -1,  0}},

    {"NE", { D,  D,  0}},
    {"SE", {-D,  D,  0}},
    {"SW", {-D, -D,  0}},
    {"NW", { D, -D,  0}},
};


/*
 * Type: cardinal_t
 * Cardinal module.
 */
typedef struct cardinal {
    obj_t obj;
} cardinal_t;


static int cardinal_render(const obj_t *obj, const painter_t *painter)
{
    int i;
    double size = 24;
    obj_t *sun;
    double pos[4];
    double sin_angle;
    double brightness = 0.0;
    double color[4];

    // Compute global brightness to adjust opacity/color of labels
    sun = obj_get_by_oid(&core->obj, oid_create("HORI", 10), 0);
    obj_get_pos_observed(sun, core->observer, pos);
    vec3_normalize(pos, pos);
    sin_angle = sin(min(M_PI/ 2, asin(pos[2]) + 8. * DD2R));
    if (sin_angle > -0.1 / 1.5 )
        brightness += 1.5 * (sin_angle + 0.1 / 1.5);
    brightness = min(brightness, 1.0);

    color[0] = 0.6 + 0.4 * brightness;
    color[1] = 0.5 - 0.3 * brightness;
    color[2] = 0.5 - 0.3 * brightness;
    color[3] = 0.6 + 0.4 * brightness;

    for (i = 0; i < ARRAY_SIZE(POINTS); i++) {
        if (painter_is_point_clipped_fast(painter, FRAME_OBSERVED,
                POINTS[i].pos, true))
            continue;
        labels_add_3d(sys_translate("gui", POINTS[i].text), FRAME_OBSERVED,
                      POINTS[i].pos, true, 0, size, color, 0,
                      ALIGN_CENTER | ALIGN_MIDDLE, TEXT_BOLD, 0, obj->oid);
    }
    return 0;
}


/*
 * Meta class declaration.
 */

static obj_klass_t cardinal_klass = {
    .id = "cardinals",
    .size = sizeof(obj_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = cardinal_render,
    .render_order = 50,
};
OBJ_REGISTER(cardinal_klass)
