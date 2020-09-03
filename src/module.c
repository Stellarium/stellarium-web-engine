/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static void (*g_listener)(obj_t *module, const char *attr) = NULL;

EMSCRIPTEN_KEEPALIVE
int module_update(obj_t *module, double dt)
{
    assert(module->klass->flags & OBJ_MODULE);
    if (!module->klass->update) return 0;
    return module->klass->update(module, dt);
}


/*
 * Function: module_list_obj
 * List all astro objects in a module.
 *
 * Parameters:
 *   obj      - The module (core for all objects).
 *   max_mag  - If set to a value different than NAN, filter out the objects
 *              that we know can never have a magnitude lower than this
 *              value.  This is just a hint given to the modules, and the
 *              caller should still check the magnitude if needed.
 *   user     - Data passed to the callback.
 *   f        - Callback function called once per object.
 *
 * Return:
 *    0         - Success.
 *   -1         - The object doesn't support listing, or a hint is needed.
 *   OBJ_AGAIN  - Some resources are still loading and so calling the function
 *                again later might return more values.
 */
int module_list_objs(const obj_t *obj,
                     double max_mag, uint64_t hint, const char *source,
                     void *user, int (*f)(void *user, obj_t *obj))
{
    obj_t *child;
    assert(f);

    if (obj->klass->list)
        return obj->klass->list(obj, max_mag, hint, source, user, f);
    if (!(obj->klass->flags & OBJ_LISTABLE)) return -1;

    // Default for listable modules: list all the children.
    DL_FOREACH(obj->children, child) {
        if (f(user, child)) break;
    }
    return 0;
}

// Only there because we can't easily call module_list_objs from js.
EMSCRIPTEN_KEEPALIVE
int module_list_objs2(const obj_t *obj, observer_t *obs,
                     double max_mag, void *user,
                     int (*f)(void *, obj_t *))
{
    return module_list_objs(obj, max_mag, 0, NULL, user, f);
}

static int module_add_data_source_task(task_t *task, double dt)
{
    struct {
        obj_t *module;
        char *url;
        char *key;
    } *data = task->user;
    int r;
    obj_t *module = data->module;
    r = module->klass->add_data_source(module, data->url, data->key);
    if (r == MODULE_AGAIN) return 0;
    free(data->url);
    free(data->key);
    free(data);
    return 1;
}

/*
 * Function: module_add_data_source
 * Add a data source url to a module
 *
 * Parameters:
 *   module - A module.
 *   url    - Url of the data.
 *   key    - Key passed to the module.  The meaning depends on the module,
 *            and is used to differentiate the sources when a module accepts
 *            several sources.
 */
EMSCRIPTEN_KEEPALIVE
int module_add_data_source(obj_t *module, const char *url, const char *key)
{
    int r;
    assert(module);
    assert(module->klass->add_data_source);
    r = module->klass->add_data_source(module, url, key);
    // If the module needs to be called again (for example to have time to
    // read a hips properties file, we enqueue a call in the core tasks.
    if (r == MODULE_AGAIN) {
        struct {
            obj_t *module;
            char *url;
            char *key;
        } *data = calloc(1, sizeof(*data));
        data->module = module;
        data->url = strdup(url);
        data->key = key ? strdup(key) : NULL;
        core_add_task(module_add_data_source_task, data);
    }
    return r;
}

// For modules: return the order in which the modules should be rendered.
// NOTE: if we used deferred rendering this wouldn't be needed at all!
double module_get_render_order(const obj_t *module)
{
    if (module->klass->get_render_order)
        return module->klass->get_render_order(module);
    else
        return module->klass->render_order;
}

EMSCRIPTEN_KEEPALIVE
void module_add_global_listener(void (*f)(obj_t *module, const char *attr))
{
    g_listener = f;
}

void module_changed(obj_t *module, const char *attr)
{
    if (g_listener)
        g_listener(module, attr);
}

EMSCRIPTEN_KEEPALIVE
void module_add(obj_t *parent, obj_t *child)
{
    assert(!child->parent);
    assert(parent);
    child->parent = parent;
    DL_APPEND(parent->children, child);
    obj_retain(child);
}

EMSCRIPTEN_KEEPALIVE
obj_t *module_add_new(obj_t *module, const char *type, json_value *args)
{
    obj_t *ret;
    ret = obj_create(type, args);
    module_add(module, ret);
    obj_release(ret);
    return ret;
}

EMSCRIPTEN_KEEPALIVE
void module_remove(obj_t *parent, obj_t *child)
{
    assert(child->parent == parent);
    assert(parent);
    assert(child->ref > 0);
    child->parent = NULL;
    DL_DELETE(parent->children, child);
    obj_release(child);
}

EMSCRIPTEN_KEEPALIVE
obj_t *module_get_child(const obj_t *module, const char *id)
{
    obj_t *ret;
    assert(id);
    DL_FOREACH(module->children, ret) {
        assert(ret->ref > 0);
        if (ret->id && strcmp(ret->id, id) == 0) {
            ret->ref++;
            return ret;
        }
    }
    return NULL;
}

static json_value *json_extract_attr(json_value *val, const char *attr)
{
    int i;
    json_value *ret = NULL;
    for (i = 0; i < val->u.object.length; i++) {
        if (strcmp(attr, val->u.object.values[i].name) != 0) continue;
        ret = val->u.object.values[i].value;
        val->u.object.values[i].value = NULL;
        break;
    }
    return ret;
}

static json_value *module_get_tree_json(const obj_t *obj, bool detailed)
{
    int i;
    attribute_t *attr;
    json_value *ret, *val, *tmp;
    obj_t *child;
    obj_klass_t *klass;

    assert(obj);
    klass = obj->klass;

    ret = json_object_new(0);
    // Add all the properties.
    for (i = 0; ; i++) {
        if (!klass || !klass->attributes) break;
        attr = &klass->attributes[i];
        if (!attr->name) break;
        if (!attr->is_prop) continue;

        // Special case for the observer.  Instead we should add an attribute
        // flag to say that we want to get the detailed value here.
        if (obj == &core->obj && strcmp(attr->name, "observer") == 0) {
            val = module_get_tree_json(&core->observer->obj, detailed);
        } else {
            val = obj_call_json(obj, attr->name, NULL);
            // Remove the attributes informations if we want a simple tree.
            if (!detailed && json_get_attr(val, "swe_", 0)) {
                tmp = json_extract_attr(val, "v");
                json_builder_free(val);
                val = tmp;
            }
        }
        json_object_push(ret, attr->name, val);
    }
    // Add all the children
    for (child = obj->children; child; child = child->next) {
        if (!child->id) continue;
        if (!(child->klass->flags & OBJ_IN_JSON_TREE)) continue;
        val = module_get_tree_json(child, detailed);
        json_object_push(ret, child->id, val);
    }
    return ret;
}

// Convenience function for the js binding.
// Return a json tree of all the attributes and children of this object.
// Inputs:
//  obj         The root object.  Default to the core object if set to NULL.
//  detailed    Whether to add hints to the values or not.
// Return:
//  A newly allocated string.  Caller should delete it.
EMSCRIPTEN_KEEPALIVE
char *module_get_tree(const obj_t *obj, bool detailed)
{
    char *ret;
    int size;
    json_value *jret;
    json_serialize_opts opts = {
        .mode = json_serialize_mode_multiline,
        .indent_size = 4,
    };

    jret = module_get_tree_json(obj, detailed);
    size = json_measure_ex(jret, opts);
    ret = calloc(1, size);
    json_serialize_ex(ret, jret, opts);
    json_builder_free(jret);
    return ret;
}

// Return the path of the object relative to a root object.
// Inputs:
//  obj         The object.
//  root        The base object or NULL.
// Return:
//  A newly allocated string.  Caller should delete it.
EMSCRIPTEN_KEEPALIVE
char *module_get_path(const obj_t *obj, const obj_t *root)
{
    char *base, *ret;
    assert(root);

    // Little hack to keep the code backward compatible.
    if (root == &core->obj && obj == &core->observer->obj)
        return strdup("observer");

    if (!obj->parent || !obj->id) return NULL;
    if (obj->parent == root) return strdup(obj->id);
    base = module_get_path(obj->parent, root);
    asprintf(&ret, "%s.%s", base, obj->id);
    free(base);
    return ret;
}
