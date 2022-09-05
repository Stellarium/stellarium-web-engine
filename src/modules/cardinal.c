/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
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
    fader_t         visible;
} cardinal_t;

static int cardinal_init(obj_t *obj, json_value *args)
{
    cardinal_t *c = (void*)obj;
    fader_init(&c->visible, true);
    return 0;
}

static int cardinal_update(obj_t *obj, double dt)
{
    cardinal_t *c = (void*)obj;
    return fader_update(&c->visible, dt);
}

static int cardinal_render(obj_t *obj, const painter_t *painter)
{
    int i;
    double size = 24;
    const cardinal_t *c = (const cardinal_t*)obj;
    double color[4] = {0.8, 0.4, 0.4, 0.8 * c->visible.value};
    double p[4];

    if (c->visible.value <= 0) return 0;
    for (i = 0; i < 4; i++) {
        if (painter_is_point_clipped_fast(painter, FRAME_OBSERVED,
                POINTS[i].pos, true))
            continue;
        convert_frame(painter->obs, FRAME_OBSERVED, FRAME_VIEW, true,
                      POINTS[i].pos, p);
        project_to_win(painter->proj, p, p);
        painter_t _painter = *painter;
        _painter.color[0] = color[0];
        _painter.color[1] = color[1];
        _painter.color[2] = color[2];
        _painter.color[3] = c->visible.value;
        _painter.lines.width = 4;
        paint_2d_ellipse(&_painter, NULL, 0, p, VEC(1, 1), NULL);
        labels_add_3d(sys_translate("gui", POINTS[i].text), FRAME_OBSERVED,
                      POINTS[i].pos, true, 0, size, color, 0,
                      ALIGN_CENTER | ALIGN_TOP, TEXT_BOLD, 0, NULL);
    }
    return 0;
}


/*
 * Meta class declaration.
 */

static obj_klass_t cardinal_klass = {
    .id = "cardinals",
    .size = sizeof(cardinal_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = cardinal_render,
    .init   = cardinal_init,
    .update = cardinal_update,
    .render_order = 50,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(cardinal_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(cardinal_klass)
