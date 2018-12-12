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
    double pos[3];
    double size = 24;
    double color[4] = {0.8, 0.2, 0.1, 1.0};
    for (i = 0; i < ARRAY_SIZE(POINTS); i++) {
        mat3_mul_vec3(core->observer->ro2v, POINTS[i].pos, pos);
        if (    !project(painter->proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_WINDOW_SPACE,
                2, pos, pos))
            continue;
        labels_add(POINTS[i].text, pos, 0, size, color, 0,
                   ANCHOR_CENTER | ANCHOR_FIXED, 0);
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
