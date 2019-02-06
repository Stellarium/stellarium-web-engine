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
    module_add(parent, obj);
    return obj;
}


EMSCRIPTEN_KEEPALIVE
obj_t *obj_get(const obj_t *obj, const char *query, int flags)
{
    obj_t *child;
    char *sep;
    char tmp[128];
    uint64_t nsid;

    assert(flags == 0);

    // If the query contains '|' we split into several queries.
    while ((sep = strchr(query, '|'))) {
        strncpy(tmp, query, sep - query);
        tmp[sep - query] = '\0';
        child = obj_get(obj, tmp, flags);
        if (child) return child;
        query = sep + 1;
    }

    // Special case for nsid.
    if (sscanf(query, "NSID %" PRIx64, &nsid) == 1)
        return obj_get_by_nsid(obj, nsid);

    // Turn queries like: obj_get(NULL, "core.abc") into:
    // obj_get(core, "abc").
    if (!obj && strncmp(query, "core.", 5) == 0) {
        obj = &core->obj;
        query += 5;
    }
    // Special case for just 'core':
    if (!obj && strcmp(query, "core") == 0) return &core->obj;

    // Default to core if we passed NULL.
    obj = obj ?: &core->obj;

    // If the id contains '.', it means we specified a sub object.
    // XXX: might not be a very good idea, what if the object id contains
    // point?
    while ((sep = strchr(query, '.'))) {
        if (sep > query && *(sep - 1) == ' ') break;
        strncpy(tmp, query, sep - query);
        tmp[sep - query] = '\0';
        DL_FOREACH(obj->children, child) {
            if (strcasecmp(child->id, tmp) == 0) {
                obj = child;
                break;
            }
        }
        assert(child);
        query = sep + 1;
    }

    // Check direct sub objects.
    // XXX: this is a waste of time in most cases!
    DL_FOREACH(obj->children, child) {
        if (child->id && strcasecmp(child->id, query) == 0) {
            child->ref++;
            return child;
        }
    }

    if (!obj->klass->get) return NULL;
    return obj->klass->get(obj, query, flags);
}

// Find an object by its nsid.
obj_t *obj_get_by_nsid(const obj_t *obj, uint64_t nsid)
{
    obj_t *child;
    obj = obj ?: (obj_t*)core;
    if (!nsid) return NULL;
    if (obj->klass->get_by_nsid)
        return obj->klass->get_by_nsid(obj, nsid);
    // Default algo: search all the children.
    // XXX: remove that: let the modules
    DL_FOREACH(obj->children, child) {
        if (child->nsid == nsid) {
            child->ref++;
            return child;
        }
    }
    return NULL;
}

// Find an object by its oid.
obj_t *obj_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj = obj ?: (obj_t*)core;
    if (obj->klass->get_by_oid)
        return obj->klass->get_by_oid(obj, oid, hint);
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_get_by_nsid_str(const obj_t *obj, const char *nsid_str)
{
    uint64_t nsid;
    nsid = strtoull(nsid_str, NULL, 16);
    return obj_get_by_nsid(obj, nsid);
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
}

EMSCRIPTEN_KEEPALIVE
void module_remove(obj_t *parent, obj_t *child)
{
    assert(child->parent == parent);
    assert(parent);
    child->parent = NULL;
    DL_DELETE(parent->children, child);
    obj_release(child);
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
    const char *sub = NULL;

    assert(obj);
    klass = obj->klass;
    if (klass->flags & OBJ_SUB) {
        sub = obj->id;
        klass = obj->parent->klass;
    }

    ret = json_object_new(0);
    // Add all the properties.
    for (i = 0; ; i++) {
        if (!klass || !klass->attributes) break;
        attr = &klass->attributes[i];
        if (!attr->name) break;
        if (!attr->is_prop) continue;
        if ((bool)attr->sub != (bool)sub) continue;
        if (attr->sub && !str_equ(attr->sub, sub)) continue;
        val = obj_call_json(obj, attr->name, NULL);
        // Remove the attributes informations if we want a simple tree.
        if (!detailed && json_get_attr(val, "swe_", 0)) {
            tmp = json_extract_attr(val, "v");
            json_builder_free(val);
            val = tmp;
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
    if (!obj->parent || !obj->id) return NULL;
    if (obj->parent == root) return strdup(obj->id);
    base = module_get_path(obj->parent, root);
    asprintf(&ret, "%s.%s", base, obj->id);
    free(base);
    return ret;
}
