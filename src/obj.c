/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "utstring.h"
#include <inttypes.h>

// Make sure the obj type padding is just after the type.
_Static_assert(offsetof(obj_t, type_padding_) ==
               offsetof(obj_t, type) + 4, "");

typedef struct {
    int         id;
    const char *id_str;
    const char *name;
    int         isattr; // Set to 1 if this is an attribute
    int         type;
    int         arg0;
    int         arg1;
} attr_info_t;

// All unparented objects get added to the root object.
static obj_t g_root_obj = {0};
static void (*g_listener)(obj_t *obj, const char *attr) = NULL;
// Global list of all the registered klasses.
static obj_klass_t *g_klasses = NULL;

static json_value *obj_fn_default_pos(obj_t *obj, const attribute_t *attr,
                                      const json_value *args);
static json_value *obj_fn_default_name(obj_t *obj, const attribute_t *attr,
                                       const json_value *args);

// List of default attributes for given names.
static const attribute_t DEFAULT_ATTRIBUTES[] = {
    { "visible", "b"},
    { "name", "s", .fn = obj_fn_default_name,
      .desc = "Common name for the object." },
    { "ra", "f", .hint = "h_angle", MEMBER(obj_t, pos.ra),
      .desc = "RA position angle (ICRS)." },
    { "dec", "f", .hint = "d_angle", MEMBER(obj_t, pos.dec),
      .desc = "Dec position angle (ICRS)."},
    { "alt", "f", .hint = "d_angle", MEMBER(obj_t, pos.alt),
      .desc = "Altitude position angle" },
    { "az", "f", .hint = "d_angle", MEMBER(obj_t, pos.az),
      .desc = "Azimuth position angle" },
    { "radec", "v4", .hint = "radec", .fn = obj_fn_default_pos,
      .desc = "Cartesian 3d vector of the ra/dec position (ICRS)."},
    { "azalt", "v3", .hint = "azalt", .fn = obj_fn_default_pos,
      .desc = "Cartesian 3d vector of the alt/az position."},
    { "vmag", "f", .hint = "mag", MEMBER(obj_t, vmag),
      .desc = "Visual magnitude"},
    { "distance", "f", .hint = "dist", .fn = obj_fn_default_pos,
      .desc = "Distance (AU)." },
    { "rise", "f", .hint = "mjd", .fn = obj_fn_default_pos,
      .desc = "Rising time." },
    { "set", "f", .hint = "mjd", .fn = obj_fn_default_pos,
      .desc = "Setting time." },
    { "type", "S", .hint = "obj_type", MEMBER(obj_t, type),
      .desc = "Type id string as defined by Simbad."},
};


EMSCRIPTEN_KEEPALIVE
void obj_add_global_listener(void (*f)(obj_t *obj, const char *attr))
{
    g_listener = f;
}

void obj_changed(obj_t *obj, const char *attr)
{
    if (g_listener)
        g_listener(obj, attr);
}

static obj_t *obj_create_(obj_klass_t *klass, const char *id, obj_t *parent,
                          json_value *args)
{
    const char *attr;
    int i;
    obj_t *obj;
    json_value *v;

    assert(klass->size);
    obj = calloc(1, klass->size);
    obj->id = strdup(id ?: "");
    obj->ref = 1;
    obj->klass = klass;
    // XXX: auto set the parent of core to the root object as a hack.
    // I think I should totally remove the root_obj.
    if (id && strcmp(id, "core") == 0) parent = &g_root_obj;
    if (parent) obj_add(parent, obj);
    if (obj->klass->init) obj->klass->init(obj, args);

    // Set the attributes.
    // obj_call_json only accepts array arguments, so we have to put the
    // values into an array, which is not really supported by the json
    // library, that's why I set the array value to NULL before calling
    // json_builder_free!
    if (args && args->type == json_object) {
        for (i = 0; i < args->u.object.length; i++) {
            attr = args->u.object.values[i].name;
            if (obj_has_attr(obj, attr)) {
                v = json_array_new(1);
                json_array_push(v, args->u.object.values[i].value);
                obj_call_json(obj, attr, v);
                v->u.array.values[0] = NULL;
                json_builder_free(v);
            }
        }
    }

    return obj;
}

obj_t *obj_create(const char *type, const char *id, obj_t *parent,
                  json_value *args)
{
    obj_t *obj;
    obj_klass_t *klass;
    const char *nsid_str;

    for (klass = g_klasses; klass; klass = klass->next) {
        if (klass->id && strcmp(klass->id, type) == 0) break;
    }
    assert(klass);
    obj = obj_create_(klass, id, parent, args);
    // Special handling for nsid... Eventually it should be passed as
    // an argument like the id?
    if (args && (nsid_str = json_get_attr_s(args, "nsid"))) {
        obj->nsid = strtoull(nsid_str, NULL, 16);
        // Use it as default id if none has been set.
        if (!obj->id) asprintf(&obj->id, "NSID %016" PRIx64, obj->nsid);
    }
    return obj;
}

// Same as obj_create but the json arguments are passes the json arguments
// as a string.
EMSCRIPTEN_KEEPALIVE
obj_t *obj_create_str(const char *type, const char *id, obj_t *parent,
                      const char *args)
{
    obj_t *ret;
    json_value *jargs;
    json_settings settings = {.value_extra = json_builder_extra};
    jargs = args ? json_parse_ex(&settings, args, strlen(args), NULL) : NULL;
    ret = obj_create(type, id, parent, jargs);
    json_value_free(jargs);
    return ret;
}

EMSCRIPTEN_KEEPALIVE
void obj_add(obj_t *parent, obj_t *child)
{
    assert(!child->parent);
    assert(parent);
    child->parent = parent;
    DL_APPEND(parent->children, child);
}

EMSCRIPTEN_KEEPALIVE
void obj_remove(obj_t *parent, obj_t *child)
{
    assert(child->parent == parent);
    assert(parent);
    child->parent = NULL;
    DL_DELETE(parent->children, child);
    obj_release(child);
}

/**** Support for obj_sub type ******************************************/
// An sub obj is a light obj that uses its parent call method.
// It is useful for example to support constellations.images.VISIBLE
// attributes.

typedef struct {
    obj_t       obj;
    obj_klass_t  *parent_klass;
} obj_sub_t;

static obj_klass_t obj_sub_klass = {
    .size  = sizeof(obj_sub_t),
    .flags = OBJ_IN_JSON_TREE,
};

obj_t *obj_add_sub(obj_t *parent, const char *name) {
    obj_sub_t *obj;
    obj = (obj_sub_t*)obj_create_(&obj_sub_klass, name, parent, NULL);
    obj->parent_klass = parent->klass;
    return &obj->obj;
}

/***********************************************************************/

EMSCRIPTEN_KEEPALIVE
void obj_release(obj_t *obj)
{
    if (!obj) return;
    assert(obj->ref);
    obj->ref--;
    if (obj->ref == 0) {
        if (obj->parent)
            DL_DELETE(obj->parent->children, obj);
        if (obj->klass->del) obj->klass->del(obj);
        free(obj->id);
        free(obj);
    }
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_clone(const obj_t *obj)
{
    assert(obj->klass->clone);
    return obj->klass->clone(obj);
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_get(const obj_t *obj, const char *query, int flags)
{
    obj_t *child;
    char *sep;
    char tmp[128], tmp2[128];

    assert(flags == 0);

    // If the query contains '|' we split into several queries.
    while ((sep = strchr(query, '|'))) {
        strncpy(tmp, query, sep - query);
        tmp[sep - query] = '\0';
        child = obj_get(obj, tmp, flags);
        if (child) return child;
        query = sep + 1;
    }

    identifiers_make_canonical(query, tmp2, sizeof(tmp2));
    query = tmp2;

    obj = obj ?: &g_root_obj;
    // If the id contains '.', it means we specified a sub object.
    // XXX: might not be a very good idea, what if the object id contains
    // point?
    while ((sep = strchr(query, '.'))) {
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
    if (obj == &g_root_obj) {
        if (strcasecmp(query, "core") == 0) return &core->obj;
        obj = &core->obj;
    }

    // Check direct sub objects.
    // XXX: this is a waste of time in most cases!
    DL_FOREACH(obj->children, child) {
        if (strcasecmp(child->id, query) == 0) {
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
    if (obj->klass->get_by_nsid)
        return obj->klass->get_by_nsid(obj, nsid);
    // Default algo: search all the children.
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

typedef union
{
    bool    b;
    int     d;
    double  f;
    void   *p;
    char   *s;
    char    str[8];
    double  color[4];
} any_t;


// For modules: return the order in which the modules should be rendered.
// NOTE: if we used deferred rendering this wouldn't be needed at all!
double obj_get_render_order(const obj_t *obj)
{
    if (obj->klass->get_render_order)
        return obj->klass->get_render_order(obj);
    else
        return obj->klass->render_order;
}

// Return the object first name, or if there is no name, the object id.
const char *obj_get_name(const obj_t *obj)
{
    const char *value;
    IDENTIFIERS_ITER(obj->id, "NAME", NULL, NULL, &value, NULL, NULL) {
        return value;
    }
    return obj->id;
}

int obj_render(const obj_t *obj, const painter_t *painter)
{
    if (obj->klass->render)
        return obj->klass->render(obj, painter);
    else
        return 0;
}

EMSCRIPTEN_KEEPALIVE
int obj_update(obj_t *obj, observer_t *obs, double dt)
{
    int ret;
    assert(!obs || obs->hash != 0);
    if (!obj->klass->update) return 0;
    observer_update(obs, true);
    assert(obs->astrom.em);
    ret = obj->klass->update(obj, obs, dt);
    obj->observer_hash = obs->hash;
    if (ret != 0) return ret;
    return ret;
}

EMSCRIPTEN_KEEPALIVE
const char *obj_get_id(const obj_t *obj)
{
    return obj->id;
}

// Return the object nsid as a hex string.
// If the object doesn't have an nsid, return an empty string.
EMSCRIPTEN_KEEPALIVE
void obj_get_nsid_str(const obj_t *obj, char out[32])
{
    if (obj->nsid) sprintf(out, "%016" PRIx64, obj->nsid);
    else out[0] = '\0';
}


EMSCRIPTEN_KEEPALIVE
int obj_get_ids(const obj_t *obj,
                void (*f)(const char *cat, const char *value, void *user),
                void *user)
{
    const char *cat, *value;
    int nb = 0;
    IDENTIFIERS_ITER(obj->id, NULL, NULL, &cat, &value, NULL, NULL) {
        f(cat, value, user);
        nb++;
    }
    return nb;
}

EMSCRIPTEN_KEEPALIVE
int obj_list(const obj_t *obj, observer_t *obs, double max_mag, void *user,
             int (*f)(const char *id, void *user))
{
    if (obj->klass->list)
        return obj->klass->list(obj, obs, max_mag, user, f);
    return 0;
}

static obj_t *obj_add_res_(obj_t *obj, json_value *value,
                           const char *base_path)
{
    obj_t *child, *r;
    int i;

    // XXX: I keep array support for the constellations, but I think we
    // should eventually remove it since we can't return an array of
    // objects.
    if (value->type == json_array) {
        for (i = 0; i < value->u.array.length; i++) {
            obj_add_res_(obj, value->u.array.values[i], base_path);
        }
        return NULL;
    }

    if (obj->klass->add_res) {
        r = obj->klass->add_res(obj, value, base_path);
        if (r) return r;
    }
    DL_FOREACH(obj->children, child) {
        if (!child->klass->add_res) continue;
        r = child->klass->add_res(child, value, base_path);
        if (r) return r;
    }
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_add_res(obj_t *obj, const char *data, const char *base_path)
{
    json_value *value;
    obj_t *ret;
    assert(data);
    value = json_parse(data, strlen(data));
    if (!value) {
        LOG_W("malformed json");
        return NULL;
    }
    ret = obj_add_res_(obj, value, base_path);
    json_value_free(value);
    return ret;
}

static json_value *obj_fn_default_name(obj_t *obj, const attribute_t *attr,
                                       const json_value *args)
{
    return args_value_new("s", NULL, obj_get_name(obj));
}

static json_value *obj_fn_default_pos(obj_t *obj, const attribute_t *attr,
                                      const json_value *args)
{
    double time;
    double v[4] = {};
    if (str_equ(attr->name, "distance")) {
        return args_value_new("f", "dist",
                obj->pos.pvg[0][3] == 0 ? NAN :
                vec3_norm(obj->pos.pvg[0]));
    }
    // XXX: not correct!  radec should be in CIRS, not ICRS!
    if (str_equ(attr->name, "radec")) {
        vec4_copy(obj->pos.pvg[0], v);
        return args_value_new("v4", "radec", v);
    }
    if (str_equ(attr->name, "azalt")) {
        eraS2c(obj->pos.az, obj->pos.alt, v);
        return args_value_new("v3", "azalt", v);
    }

    if (str_equ(attr->name, "rise")) {
        time = compute_event(core->observer, obj, EVENT_RISE,
                             core->observer->tt - 1.0 / 2,
                             core->observer->tt + 1.0 / 2,
                             1.0 / 24 / 60 / 2);
        return args_value_new("f", "jdm", time);
    }
    if (str_equ(attr->name, "set")) {
        time = compute_event(core->observer, obj, EVENT_SET,
                             core->observer->tt - 1.0 / 2,
                             core->observer->tt + 1.0 / 2,
                             1.0 / 24 / 60 / 2);
        return args_value_new("f", "jdm", time);
    }
    assert(false);
    return NULL;
}

// XXX: cleanup this code.
static json_value *obj_fn_default(obj_t *obj, const attribute_t *attr,
                                  const json_value *args)
{
    obj_t *container_obj = obj->klass == &obj_sub_klass ? obj->parent : obj;
    assert(attr->type);
    void *p = ((void*)container_obj) + attr->member.offset;
    // Buffer large enough to contain any kind of property data, including
    // static strings.
    char buf[4096] __attribute__((aligned(8)));
    obj_t *o;

    // If no input arguents, return the value.
    if (!args || (args->type == json_array && !args->u.array.length)) {
        if (strcmp(attr->type, "b") == 0)
            return args_value_new(attr->type, attr->hint, *(bool*)p);
        else if (strcmp(attr->type, "d") == 0)
            return args_value_new(attr->type, attr->hint, *(int*)p);
        else if (strcmp(attr->type, "f") == 0)
            return args_value_new(attr->type, attr->hint, *(double*)p);
        else if (strcmp(attr->type, "p") == 0)
            return args_value_new(attr->type, attr->hint, *(void**)p);
        else if (strcmp(attr->type, "s") == 0)
            return args_value_new(attr->type, attr->hint, *(char**)p);
        else
            return args_value_new(attr->type, attr->hint, p);
    } else { // Set the value.
        assert(attr->member.size <= sizeof(buf));
        args_get(args, NULL, 1, attr->type, attr->hint, buf);
        if (memcmp(p, buf, attr->member.size) != 0) {
            // If we override an object, don't forget to release the
            // previous value and increment the ref to the new one.
            if (attr->hint && strcmp(attr->hint, "obj") == 0) {
                o = *(obj_t**)p;
                obj_release(o);
                memcpy(&o, buf, sizeof(o));
                if (o) o->ref++;
            }
            memcpy(p, buf, attr->member.size);
            if (attr->on_changed) attr->on_changed(obj, attr);
            obj_changed(obj, attr->name);
        }
        return NULL;
    }
}

EMSCRIPTEN_KEEPALIVE
const attribute_t *obj_get_attr_(const obj_t *obj, const char *attr_name)
{
    attribute_t *attr;
    const char *sub = NULL;
    int i;
    assert(obj);
    if (obj->klass == &obj_sub_klass) {
        sub = obj->id;
        obj = obj->parent;
    }
    ASSERT(obj->klass->attributes, "type '%s' has no attributes '%s'",
           obj->klass->id, attr_name);
    for (i = 0; ; i++) {
        attr = &obj->klass->attributes[i];
        if (!attr->name) return NULL;
        if (sub && attr->sub && !str_equ(attr->sub, sub)) continue;
        if (str_equ(attr->name, attr_name)) break;
    }
    return attr;
}

EMSCRIPTEN_KEEPALIVE
void obj_foreach_attr(const obj_t *obj,
                      void *user,
                      void (*f)(const char *attr, int is_property, void *user))
{
    int i;
    attribute_t *attr;
    if (!obj->klass) return;
    if (obj->klass == &obj_sub_klass) obj = obj->parent;
    if (!obj->klass->attributes) return;
    for (i = 0; ; i++) {
        attr = &obj->klass->attributes[i];
        if (!attr->name) return;
        f(attr->name, attr->is_prop, user);
    }
}

// Only to be used in the js code so that we can access object children
// directly using properties.
EMSCRIPTEN_KEEPALIVE
void obj_foreach_child(const obj_t *obj, void (*f)(const char *id))
{
    obj_t *child;
    for (child = obj->children; child; child = child->next) {
        if (!(child->klass->flags & OBJ_IN_JSON_TREE)) continue;
        assert(child->id);
        f(child->id);
    }
}

bool obj_has_attr(const obj_t *obj, const char *attr)
{
    return obj_get_attr_(obj, attr) != NULL;
}

json_value *obj_call_json(obj_t *obj, const char *name,
                          const json_value *args)
{

    const attribute_t *attr;
    attr = obj_get_attr_(obj, name);
    if (!attr) {
        LOG_E("Cannot find attribute %s of object %s", name, obj->id);
        return NULL;
    }
    return (attr->fn ?: obj_fn_default)(obj, attr, args);
}

static int split_types(const char *sig, char (*out)[4])
{
    int i;
    for (i = 0; *sig; i++, sig++) {
        out[i][0] = sig[0];
        out[i][1] = '\0';
        if (sig[1] >= '0' && sig[1] <= '9') {
            out[i][1] = sig[1];
            out[i][2] = '\0';
            sig++;
        }
    }
    return i;
}

int obj_call(obj_t *obj, const char *attr, const char *sig, ...)
{
    char types[8][4];
    int i, nb_args;
    va_list ap;
    json_value *args, *ret;

    va_start(ap, sig);
    nb_args = split_types(sig, types);
    args = json_array_new(nb_args);
    for (i = 0; i < nb_args; i++)
        json_array_push(args, args_vvalue_new(types[i], NULL, ap));
    ret = obj_call_json(obj, attr, args);
    json_builder_free(args);
    json_builder_free(ret);
    return 0;
}

EMSCRIPTEN_KEEPALIVE
char *obj_call_json_str(obj_t *obj, const char *attr, const char *args)
{
    json_value *jargs, *jret;
    char *ret;
    int size;

    jargs = args ? json_parse(args, strlen(args)) : NULL;
    jret = obj_call_json(obj, attr, jargs);
    size = json_measure(jret);
    ret = calloc(1, size);
    json_serialize(ret, jret);
    json_value_free(jargs);
    json_builder_free(jret);
    return ret;
}

int obj_get_attr(const obj_t *obj, const char *name, const char *type, ...)
{
    json_value *ret;
    va_list ap;
    va_start(ap, type);
    ret = obj_call_json(obj, name, NULL);
    assert(ret);
    args_vget(ret, NULL, 1, type, NULL, ap);
    json_builder_free(ret);
    va_end(ap);
    return 0;
}

int obj_set_attr(const obj_t *obj, const char *name, const char *type, ...)
{
    json_value *arg, *ret;
    va_list ap;
    va_start(ap, type);
    arg = args_vvalue_new(type, NULL, ap);
    ret = obj_call_json(obj, name, arg);
    json_builder_free(arg);
    json_builder_free(ret);
    va_end(ap);
    return 0;
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

static json_value *obj_get_tree_json(const obj_t *obj, bool detailed)
{
    int i;
    attribute_t *attr;
    json_value *ret, *val, *tmp;
    obj_t *child;
    obj_klass_t *klass;
    const char *sub = NULL;

    obj = obj ?: &g_root_obj;

    klass = obj->klass;
    if (klass == &obj_sub_klass) {
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
        val = obj_get_tree_json(child, detailed);
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
char *obj_get_tree(const obj_t *obj, bool detailed)
{
    char *ret;
    int size;
    json_value *jret;
    json_serialize_opts opts = {
        .mode = json_serialize_mode_multiline,
        .indent_size = 4,
    };

    jret = obj_get_tree_json(obj, detailed);
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
char *obj_get_path(const obj_t *obj, const obj_t *root)
{
    char *base, *ret;
    root = root ?: &g_root_obj;
    if (!obj->parent) return NULL;
    if (obj->parent == root) return strdup(obj->id);
    base = obj_get_path(obj->parent, root);
    asprintf(&ret, "%s.%s", base, obj->id);
    free(base);
    return ret;
}

// Setup the default values of an attribute.
static void init_attribute(attribute_t *attr)
{
    int i;
    if (attr->fn) return;
    for (i = 0; i < ARRAY_SIZE(DEFAULT_ATTRIBUTES); i++) {
        if (str_equ(attr->name, DEFAULT_ATTRIBUTES[i].name)) break;
    }
    if (i == ARRAY_SIZE(DEFAULT_ATTRIBUTES)) return;
    attr->hint = attr->hint ?: DEFAULT_ATTRIBUTES[i].hint;
    attr->desc = attr->desc ?: DEFAULT_ATTRIBUTES[i].desc;
    if (!attr->member.size) {
        attr->member = DEFAULT_ATTRIBUTES[i].member;
        attr->type = attr->type ?: DEFAULT_ATTRIBUTES[i].type;
        attr->fn = attr->fn ?: DEFAULT_ATTRIBUTES[i].fn;
    }
    if (attr->member.size) assert(attr->type);
}

void obj_register_(obj_klass_t *klass)
{
    attribute_t *attr;
    assert(klass->size);
    LL_PREPEND(g_klasses, klass);
    // Init all the default attributes.
    if (klass->attributes) {
        for (attr = &klass->attributes[0]; attr->name; attr++) {
            init_attribute(attr);
        }
    }
}

static int klass_sort_cmp(void *a, void *b)
{
    obj_klass_t *at, *bt;
    at = a;
    bt = b;
    int ak = at->create_order ?: at->render_order;
    int bk = bt->create_order ?: bt->render_order;
    return cmp(ak, bk);
}

obj_klass_t *obj_get_all_klasses(void)
{
    LL_SORT(g_klasses, klass_sort_cmp);
    return g_klasses;
}

obj_klass_t *obj_get_klass_by_name(const char *name)
{
    obj_klass_t *klass;
    for (klass = g_klasses; klass; klass = klass->next) {
        if (klass->id && strcmp(klass->id, name) == 0) return klass;
    }
    return NULL;
}

void obj_get_pos_icrs(obj_t *obj, observer_t *obs, double pos[4])
{
    obj_update(obj, obs, 0);
    vec4_copy(obj->pos.pvg[0], pos);
}

void obj_get_pos_observed(obj_t *obj, observer_t *obs, double pos[4])
{
    double p[4];
    obj_get_pos_icrs(obj, obs, p);
    convert_coordinates(obs, FRAME_ICRS, FRAME_OBSERVED, 0, p, p);
    vec4_copy(p, pos);
}

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

typedef struct test {
    obj_t   obj;
    double  alt;
    int     proj;
    double  my_attr;
    int     nb_changes;
} test_t;

static void test_my_attr_changed(obj_t *obj, const attribute_t *attr)
{
    ((test_t*)obj)->nb_changes++;
}

static int test_choices(
        int (*f)(const char *name, const char *id, void *user),
        void *user)
{
    if (f) {
        f("TEST1", "1", user);
        f("TEST2", "2", user);
    }
    return 2;
}

static json_value *test_lookat_fn(obj_t *obj, const attribute_t *attr,
                                  const json_value *args)
{
    return NULL;
}

static obj_klass_t test_klass = {
    .id = "test",
    .attributes = (attribute_t[]) {
        PROPERTY("altitude", "f", MEMBER(test_t, alt)),
        PROPERTY("my_attr", "f", MEMBER(test_t, my_attr),
                 .on_changed = test_my_attr_changed),
        PROPERTY("projection", "d", MEMBER(test_t, proj), .desc = "Projection",
                 .choices = test_choices),
        FUNCTION("lookat", .fn = test_lookat_fn),
        {}
    },
};

static void test_simple(void)
{
    test_t test = {};
    double alt;

    test.obj.klass = &test_klass;
    test.alt = 10.0;
    obj_get_attr(&test.obj, "altitude", "f", &alt);
    assert(alt == 10.0);
    obj_set_attr(&test.obj, "altitude", "f", 5.0);
    assert(test.alt == 5.0);
    obj_call(&test.obj, "lookat", "pf", NULL, 10.0);

    obj_set_attr(&test.obj, "my_attr", "f", 20.0);
    assert(test.my_attr == 20.0);
    assert(test.nb_changes == 1);
    obj_set_attr(&test.obj, "my_attr", "f", 20.0);
    assert(test.nb_changes == 1);
    obj_set_attr(&test.obj, "my_attr", "f", 30.0);
    assert(test.nb_changes == 2);
}

TEST_REGISTER(NULL, test_simple, TEST_AUTO);

#endif
