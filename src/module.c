/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

/*
 * Function: module_list_obj
 * List all astro objects in a module.
 *
 * Parameters:
 *   obj      - The module (core for all objects).
 *   obs      - The observer used to compute the object vmag.
 *   max_mag  - Only consider objects below this magnitude.
 *   user     - Data passed to the callback.
 *   f        - Callback function called once per object.
 *
 * Return:
 *    0         - Success.
 *   -1         - The object doesn't support listing, or a hint is needed.
 *   OBJ_AGAIN  - Some resources are still loading and so calling the function
 *                again later might return more values.
 */
EMSCRIPTEN_KEEPALIVE
int module_list_objs(const obj_t *obj, observer_t *obs,
                     double max_mag, uint64_t hint, void *user,
                     int (*f)(void *user, obj_t *obj))
{
    obj_t *child;

    if (obj->klass->list)
        return obj->klass->list(obj, obs, max_mag, hint, user, f);
    if (!(obj->klass->flags & OBJ_LISTABLE)) return -1;

    // Default for listable modules: list all the children.
    DL_FOREACH(obj->children, child) {
        obj_update(child, obs, 0);
        if (child->vmag > max_mag) continue;
        if (f && f(user, child)) break;
    }
    return 0;
}

/*
 * Function: module_add_data_source
 * Add a data source url to a module
 *
 * Parameters:
 *   module - a module, or NULL for any module.
 *   url    - base url of the data.
 *   type   - type of data.  NULL for directory.
 *   args   - additional arguments passed.  Can be used by the modules to
 *            check if they can handle the source or not.
 *
 * Return:
 *   0 if the source was accepted.
 *   1 if the source was no recognised.
 *   a negative error code otherwise.
 */
EMSCRIPTEN_KEEPALIVE
int module_add_data_source(obj_t *obj, const char *url, const char *type,
                           json_value *args)
{
    if (!obj) obj = &core->obj;
    if (obj->klass->add_data_source)
        return obj->klass->add_data_source(obj, url, type, args);
    return 1; // Not recognised.
}

/**** Support for obj_sub type ******************************************/
// An sub obj is a light obj that uses its parent call method.
// It is useful for example to support constellations.images.VISIBLE
// attributes.

static obj_klass_t obj_sub_klass = {
    .size  = sizeof(obj_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_SUB,
};

obj_t *module_add_sub(obj_t *parent, const char *name)
{
    obj_t *obj;
    obj = calloc(1, sizeof(*obj));
    obj->ref = 1;
    obj->id = strdup(name);
    obj->klass = &obj_sub_klass;
    obj_add(parent, obj);
    return obj;
}
