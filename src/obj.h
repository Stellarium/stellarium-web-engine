/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef OBJ_H
#define OBJ_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Base class for all the objects in swe (modules, sky objects, etc).
 *
 * Inspired by linux kernel kobj.
 *
 * An object is any C structure that starts with an obj_t attribute.
 * The obj_t structure contains a reference counter (for automatic memory
 * managment), an uniq id, a vtable pointer, and various attributes
 * specific to sky objects (J2000 position, ra/de...)
 *
 * The vtable for a given class is specified in a static obj_klass instance.
 */

enum {
    OBJ_IN_JSON_TREE = 1 << 0,    // Show up in json tree.
    OBJ_MODULE       = 1 << 1,    // Auto add to core modules.
};

typedef struct _json_value json_value;
typedef struct attribute attribute_t;
typedef struct obj obj_t;
typedef struct observer observer_t;
typedef struct painter painter_t;
typedef struct obj_klass obj_klass_t;

// Info structure that represents a given class of objects.
struct obj_klass
{
    const char  *id;
    size_t   size; // must be set to the size of the object struct.
    uint32_t flags;
    // Various methods to manipulate objects of this class.
    int (*init)(obj_t *obj, json_value *args);
    void (*del)(obj_t *obj);
    int (*update)(obj_t *obj, const observer_t *obs, double dt);
    int (*render)(const obj_t *obj, const painter_t *painter);
    obj_t* (*clone)(const obj_t *obj);

    // Find a sub object given an id.
    obj_t *(*get)(const obj_t *obj, const char *id, int flags);
    // Find a sub object given a NSID.
    obj_t *(*get_by_nsid)(const obj_t *obj, uint64_t nsid);

    void (*gui)(obj_t *obj, int location);

    // For modules objects.
    // Try to register a resource from a json file into this module.
    obj_t *(*add_res)(obj_t *obj, json_value *value, const char *base_path);
    // List all the sky objects children from this module.
    int (*list)(const obj_t *obj, double max_mag, void *user,
                int (*f)(const char *id, void *user));
    // Return the render order.
    // By default this return the class attribute `render_order`.
    double (*get_render_order)(const obj_t *obj);

    // Used to sort the modules when we render and create them.
    double render_order;
    double create_order;

    // List of object attributes that can be read, set or called with the
    // obj_call and obj_toogle_attr functions.
    attribute_t *attributes;

    // All the registered klass are put in a list, sorted by create_order.
    obj_klass_t *next;
};

struct obj
{
    obj_klass_t *klass;
    int         ref;
    char        *id;
    uint64_t    nsid; // Noctuasky id if defined, or zero.
    // 4 bytes type id of the object.  Should follow the condensed values
    // defined by Simbad:
    // http://simbad.u-strasbg.fr/simbad/sim-display?data=otypes
    // NULL terminated.
    char        type[4];
    char        type_padding_; // Ensure that type is null terminated.
    obj_t       *parent;
    obj_t       *children, *prev, *next;

    // Hash of the observer the last time obj_update was called.
    // Can be used to skip update.
    uint64_t    observer_hash;

    // Must be up to date after a call to obj_update.
    double vmag;
    struct {
        // equ, J2000.0, AU geocentric pos and speed.
        double pvg[2][3];
        // Can be set to INFINITY for stars that are too far to have a
        // position un AU.
        double unit;
        double ra, dec; // ICRS.
        double az, alt;
    } pos;
};

struct attribute {
    const char *name;
    const char *type;
    bool is_prop;
    json_value *(*fn)(obj_t *obj, const attribute_t *attr,
                      const json_value *args);
    struct {
        int offset;
        int size;
    } member;
    const char *hint;
    const char *sub;
    const char *desc;
    void (*on_changed)(obj_t *obj, const attribute_t *attr);
    int (*choices)(int (*f)(const char *name, const char *id, void *user),
                   void *user);
};

#define PROPERTY(name, ...) {name, ##__VA_ARGS__, .is_prop = true}
#define FUNCTION(name, ...) {name, ##__VA_ARGS__}
#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

// Create a new object.
// Arguments:
//  type:   The type of the object (must have been registered).
//  id:     Unique id to give the object.
//  parent: The new object is added to the parent.
//  args:   Attributes to set.
obj_t *obj_create(const char *type, const char *id, obj_t *parent,
                  json_value *args);

// Same as obj_create but the json arguments are passed as a string.
obj_t *obj_create_str(const char *type, const char *id, obj_t *parent,
                      const char *args);

#define OBJ_ITER(obj, child, klass_) \
    for (child = (void*)(((obj_t*)obj)->children); child; \
                        child = (void*)(((obj_t*)child)->next)) \
        if ((uintptr_t)(klass_) == (uintptr_t)NULL || \
                ((obj_t*)child)->klass == klass_)

// Add an object as a child of an other one.
void obj_add(obj_t *parent, obj_t *child);

// Add an object from a parent.
void obj_remove(obj_t *parent, obj_t *child);

void obj_delete(obj_t *obj);
// Create a clone of the object.  Fails if the object doesn't support
// cloning.
obj_t *obj_clone(const obj_t *obj);

// Create a dummy sub object that can be used in the obj_call function to
// Assign attributes to sub object.  For example it can be used to have
// constellations.images, without having to create a special class just for
// that.
obj_t *obj_add_sub(obj_t *parent, const char *name);

// Find an object by query.
// obj: The parent object we search from, NULL for any object.
// query: an identifier that represents the object, can be:
//  - A direct object id (HD 456, NGC 8)
//  - module name (constellations)
//  - submodule (constellations.lines)
//  - object name (polaris)
// flags: always 0.
obj_t *obj_get(const obj_t *obj, const char *query, int flags);

// Find an object by its nsid.
obj_t *obj_get_by_nsid(const obj_t *obj, uint64_t nsid);

// For modules: return the order in which the modules should be rendered.
// NOTE: if we used deferred rendering this wouldn't be needed at all!
double obj_get_render_order(const obj_t *obj);

int obj_render(const obj_t *obj, const painter_t *painter);

// Update the internal state of the object.
// obs: the observer (can be null for non astro objects, like ui, etc).
// dt: user delta time (used for example for fading effects).
int obj_update(obj_t *obj, observer_t *obs, double dt);

const char *obj_get_name(const obj_t *obj);

int obj_get_ids(const obj_t *obj,
                void (*f)(const char *id, const char *value, void *user),
                void *user);

// Return a json tree of all the attributes and children of this object.
// Inputs:
//  obj         The root object or NULL for global tree (starts at 'core')
//  detailed    Whether to add hints to the values or not.
// Return:
//  A newly allocated string.  Caller should delete it.
char *obj_get_tree(const obj_t *obj, bool detailed);

// Return the path of the object relative to a root object.
// Inputs:
//  obj         The object.
//  root        The base object or NULL for the global path ("core.xyz").
// Return:
//  A newly allocated string.  Caller should delete it.
char *obj_get_path(const obj_t *obj, const obj_t *root);

// List all astro objects in a module.
int obj_list(const obj_t *obj, double max_mag, void *user,
             int (*f)(const char *id, void *user));

obj_t *obj_add_res(obj_t *obj, const char *data, const char *base_path);

// Register a callback to be called anytime an attribute of an object changes.
// XXX: maybe make it individual per object?
void obj_add_global_listener(void (*f)(obj_t *obj, const char *attr));
// Should be called by object after they change one of their attributes.
void obj_changed(obj_t *obj, const char *attr);

const attribute_t *obj_get_attr_(const obj_t *obj, const char *attr);

bool obj_has_attr(const obj_t *obj, const char *attr);
int obj_call(obj_t *obj, const char *attr, const char *sig, ...);
json_value *obj_call_json(obj_t *obj, const char *attr,
                          const json_value *args);
// Make an object call from a json string.
char *obj_call_json_str(obj_t *obj, const char *attr, const char *args);

// Convenience functions to access obj attributes.
int obj_get_attr(const obj_t *obj, const char *attr, const char *type, ...);
int obj_set_attr(const obj_t *obj, const char *attr, const char *type, ...);


// Register an object klass, so that we can create instances dynamically
#define OBJ_REGISTER(klass) \
    static void obj_register_##klass##_(void) __attribute__((constructor)); \
    static void obj_register_##klass##_(void) { obj_register_(&klass); }
void obj_register_(obj_klass_t *klass);

// Return a pointer to the registered object klasses list.
obj_klass_t *obj_get_all_klasses(void);

#endif // OBJ_H
