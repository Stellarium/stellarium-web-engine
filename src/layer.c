/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Special type of object that is just here to contain other objects.

typedef struct layer {
    obj_t       obj;
    fader_t     visible;
    double      z;
} layer_t;

static double layer_get_render_order(const obj_t *obj)
{
    layer_t *layer = (void*)obj;
    return layer->z;
}

static int children_sort_cmp(void *a_, void *b_)
{
    // We sort the objects inside a layer according to their "z" attribute.
    obj_t *a = a_;
    obj_t *b = b_;
    double az = 0, bz = 0;
    if (obj_has_attr(a, "z"))
        obj_get_attr(a, "z", &az);
    if (obj_has_attr(b, "z"))
        obj_get_attr(b, "z", &bz);
    return cmp(az, bz);
}

static int layer_update(obj_t *obj, double dt)
{
    layer_t *layer = (layer_t*)obj;
    obj_t *child;
    fader_update(&layer->visible, dt);

    DL_SORT(obj->children, children_sort_cmp);
    MODULE_ITER(obj, child, NULL) {
        if (child->klass->flags & OBJ_MODULE)
            module_update(child, dt);
    }
    return 0;
}

static int layer_render(const obj_t *obj, const painter_t *painter_)
{
    layer_t *layer = (layer_t*)obj;
    painter_t painter = *painter_;
    obj_t *child;
    painter.color[3] *= layer->visible.value;
    MODULE_ITER(obj, child, NULL) {
        obj_render(child, &painter);
    }
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t layer_klass = {
    .id                 = "layer",
    .size               = sizeof(layer_t),
    .flags              = OBJ_IN_JSON_TREE,
    .get_render_order   = layer_get_render_order,
    .update             = layer_update,
    .render             = layer_render,
    .attributes         = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(layer_t, visible.target)),
        PROPERTY(z, TYPE_FLOAT, MEMBER(layer_t, z)),
        {}
    },
};
OBJ_REGISTER(layer_klass)
