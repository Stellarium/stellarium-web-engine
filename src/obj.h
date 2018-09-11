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

/*
 * Base class for all the objects in swe (modules, sky objects, etc).
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

/*
 * Enum: OBJ_FLAG
 *
 * Flags that can be set to object.
 *
 * OBJ_IN_JSON_TREE     - The object show up in the json tree dump.
 * OBJ_MODULE           - The object is a module.
 */
enum {
    OBJ_IN_JSON_TREE = 1 << 0,
    OBJ_MODULE       = 1 << 1,
};

typedef struct _json_value json_value;
typedef struct attribute attribute_t;
typedef struct obj obj_t;
typedef struct observer observer_t;
typedef struct painter painter_t;
typedef struct obj_klass obj_klass_t;

/*
 * Type: obj_klass
 * Info structure that represents a given class of objects.
 *
 * Each object should have a pointer to an obj_klass instance that defines
 * his type.  This is the equivalent of a C++ class.
 *
 * The klass allow object to define some basic methods for initializing,
 * updating, rendering... and also the list of public attributes for the
 * object.
 *
 * Attributes:
 *   id     - Name of the class.
 *   size   - Must be set to the size of the instance object struct.  This
 *            is used for the initial calloc done by the constructor.
 *   flags  - Some of <OBJ_FLAG>.
 *   attributes - List of <attribute_t>, NULL terminated.
 *
 * Methods:
 *   init   - Called when a new instance is created.
 *   del    - Called when an instance is destroyed.
 *   update - Update the object for a new observer.
 *   render - Render the object.
 *   clone  - Create a copy of the object.
 *   get    - Find a sub-object for a given query.
 *   get_by_nsid - Find a sub-object for a given nsid.
 *   get_by_oid  - Find a sub-object for a given oid.
 *
 * Module Attributes:
 *   render_order - Used to sort the modules before rendering.
 *   create_order - Used at creation time to make sure the modules are
 *                  created in the right order.
 *
 * Module Methods:
 *   add_res - Try to register a resource from a json into the module.
 *   list    - List all the sky objects children from this module.
 *   get_render_order - Return the render order.
 */
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
    int (*render_pointer)(const obj_t *obj, const painter_t *painter);

    // Find a sub object given an id.
    obj_t *(*get)(const obj_t *obj, const char *id, int flags);
    // Find a sub object given a NSID.
    obj_t *(*get_by_nsid)(const obj_t *obj, uint64_t nsid);
    // Find a sub object given an oid
    obj_t *(*get_by_oid)(const obj_t *obj, uint64_t oid, uint64_t hint);

    void (*gui)(obj_t *obj, int location);

    // For modules objects.
    // Try to register a resource from a json file into this module.
    obj_t *(*add_res)(obj_t *obj, json_value *value, const char *base_path);
    // List all the sky objects children from this module.
    int (*list)(const obj_t *obj, observer_t *obs, double max_mag, void *user,
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

/*
 * Type: obj_t
 * The base structure for all sky objects and modules
 *
 * All the module and sky objects should 'inherit' from this structure,
 * that is must have an obj_t as a first attribute, so that we can cast
 * any object structure to an obj_t.
 *
 * Attributes:
 *   klass      - Pointer to an <obj_klass_t> structure.
 *   ref        - Reference counter.
 *   id         - String id of the object.
 *   oid        - Internal uniq id.
 *   nsid       - Noctuasky internal id if defined, or zero.
 *   type       - Four bytes type id of the object.  Should follow the
 *                condensed values defined by Simbad:
 *                http://simbad.u-strasbg.fr/simbad/sim-display?data=otypes
 *   parent     - Parent object.
 *   children   - Pointer to the list of children.
 *   prev       - Pointer to the previous sibling.
 *   next       - Pointer to the next sibling.
 *   observer_hash - Hash of the observer the last time obj_update was called.
 *                   Can be used to skip update.
 *   vmag       - Visual magnitude.
 *   pos        - TODO: for the moment this is a struct with various computed
 *                positions.  I want to replace it with a single ICRS
 *                pos + speed argument.
 */
struct obj
{
    obj_klass_t *klass;
    int         ref;
    char        *id;    // To be removed.  Use oid instead.
    uint64_t    oid;
    uint64_t    nsid;
    char        type[4];
    char        type_padding_; // Ensure that type is null terminated.
    obj_t       *parent;
    obj_t       *children, *prev, *next;

    uint64_t    observer_hash;

    // Must be up to date after a call to obj_update.
    double vmag;
    struct {
        double pvg[2][4];   // ICRS pos and speed in xyzw.
        double ra, dec;     // CIRS polar.
        double az, alt;     // Observed polar.
    } pos;
};

/*
 * Type: attribute_t
 * Represent an object attribute.
 *
 * An attribute can either be a property (a single value that we can get or
 * set), or a function that we can call.  Attributes also emit signals
 * when they change so that other object can react to it.
 *
 * Attributes allows to manipulate objects without having to expose the
 * underlying C struct.  This is important for javascript binding.
 *
 * We should never create attributes directly, instead use the
 * <PROPERTY> and <FUNCTION> macros inside the <obj_klass_t> declaration.
 *
 * Attributes:
 *   name       - Name of this attribute.
 *   type       - Base type id ('d', 'f', 'i', 'v2', 'v3, 'v4', 's')
 *   is_prop    - Set to true if the attribute is a property.
 *   fn         - Attribute function (setter/getter for property).
 *   member     - Member info for common case of attributes that map directly
 *                to an object struct member.
 *   hint       - Hint about the type of the attribute.
 *   sub        - Sub-object name for special attributes: XXX to cleanup.
 *   desc       - Description of the attribute.
 *   on_changed - Callback called when a property changed.
 *   choices    - Function that returns a list of possible values for an
 *                attribute (deprecated?)
 */
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
    int (*choices)(int (*f)(const char *name, uint64_t oid, void *user),
                   void *user);
};

/*
 * Macro: PROPERTY
 * Convenience macro to define a property attribute.
 */
#define PROPERTY(name, ...) {name, ##__VA_ARGS__, .is_prop = true}

/*
 * Macro: FUNCTION
 * Convenience macro to define a function attribute.
 */
#define FUNCTION(name, ...) {name, ##__VA_ARGS__}

/*
 * Macro: MEMBER
 * Convenience macro to define that a property map to a C struct attribute.
 */
#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

/*
 * Function: obj_create
 * Create a new object.
 *
 * Properties:
 *   type   - The type of the object (must have been registered).
 *   id     - Unique id to give the object.
 *   parent - The new object is added to the parent.
 *   args   -  Attributes to set.
 */
obj_t *obj_create(const char *type, const char *id, obj_t *parent,
                  json_value *args);

/*
 * Function: obj_create
 * Same as obj_create but the json arguments are passed as a string.
 */
obj_t *obj_create_str(const char *type, const char *id, obj_t *parent,
                      const char *args);

/*
 * Macro: OBJ_ITER
 * Iter all the children of a given object of a given type.
 *
 * Properties:
 *   obj    - The object we want to iterate.
 *   child  - Pointer to an object, that will be set with each child.
 *   klass_ - Children klass type id string, or NULL for no filter.
 */
#define OBJ_ITER(obj, child, klass_) \
    for (child = (void*)(((obj_t*)obj)->children); child; \
                        child = (void*)(((obj_t*)child)->next)) \
        if (!(klass_) || \
                ((((obj_t*)child)->klass) && \
                 (((obj_t*)child)->klass->id) && \
                 (strcmp(((obj_t*)child)->klass->id, klass_ ?: "") == 0)))

/*
 * Function: obj_add
 * Add an object as a child of an other one.
 */
void obj_add(obj_t *parent, obj_t *child);

/*
 * Function: obj_remove
 * Remove an object from a parent.
 */
void obj_remove(obj_t *parent, obj_t *child);

/*
 * Function: obj_release
 * Decrement object ref count and delete it if needed.
 */
void obj_release(obj_t *obj);

/*
 * Function: obj_clone
 * Create a clone of the object.
 *
 * Fails if the object doesn't support cloning.
 */
obj_t *obj_clone(const obj_t *obj);

/*
 * Function: obj_add_sub
 * Create a dummy sub object
 *
 * This can be used in the obj_call function to Assign attributes to sub
 * object.  For example it can be used to have constellations.images, without
 * having to create a special class just for that.
 */
obj_t *obj_add_sub(obj_t *parent, const char *name);

//XXX: probably should rename this to obj_query.
/*
 * Function: obj_get
 * Find an object by query.
 *
 * Parameters:
 *   obj    - The parent object we search from, NULL for any object.
 *   query  - An identifier that represents the object, can be:
 *      - A direct object id (HD 456, NGC 8)
 *      - A module name (constellations)
 *      - A submodule (constellations.lines)
 *      - An object name (polaris)
 *   flags  - always zero for the moment.
 */
obj_t *obj_get(const obj_t *obj, const char *query, int flags);

/*
 * Function: obj_get_by_oid
 * Find an object by its oid.
 */
obj_t *obj_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint);

/*
 * Function: obj_get_by_nsid
 * Find an object by its nsid.
 */
obj_t *obj_get_by_nsid(const obj_t *obj, uint64_t nsid);

/*
 * Function: obj_get_render_order
 *
 * For modules: return the order in which the modules should be rendered.
 * NOTE: if we used deferred rendering this wouldn't be needed at all!
 */
double obj_get_render_order(const obj_t *obj);

/*
 * Function: obj_render
 * Render an object.
 */
int obj_render(const obj_t *obj, const painter_t *painter);

/*
 * Function: obj_update
 * Update the internal state of the object.
 *
 * Parameters:
 *   obs - The observer (can be null for non astro objects, like ui, etc).
 *   dt  - User delta time (used for example for fading effects).
 */
int obj_update(obj_t *obj, observer_t *obs, double dt);


/*
 * Function: obj_get_pos_icrs
 * Conveniance function that updates an object and return its ICRS
 * position.
 *
 * Parameters:
 *   obj - An object.
 *   obs - An observer.
 *   pos - Get the ICRS position in homogeneous coordinates (xyzw, AU).
 */
void obj_get_pos_icrs(obj_t *obj, observer_t *obs, double pos[4]);

/*
 * Function: obj_get_pos_observed
 * Conveniance function that updates an object and return its observed
 * position (az/alt frame, but as a cartesian vector).
 *
 * Parameters:
 *   obj - An object.
 *   obs - An observer.
 *   pos - Get the observed position in homogeneous coordinates (xyzw, AU).
 */
void obj_get_pos_observed(obj_t *obj, observer_t *obs, double pos[4]);

/*
 * Function: obj_get_name
 * Return the given name for an object.
 */
const char *obj_get_name(const obj_t *obj);

/*
 * Function: obj_get_ids
 * Return the list of all the identifiers of an object.
 */
int obj_get_ids(const obj_t *obj,
                void (*f)(const char *id, const char *value, void *user),
                void *user);

/*
 * Function: obj_get_tree
 * Return a json tree of all the attributes and children of this object.
 *
 * Parameters:
 *   obj        - The root object or NULL for global tree (starts at 'core')
 *   detailed   - Whether to add hints to the values or not.
 *
 * Return:
 *   A newly allocated string.  Caller should delete it.
 */
char *obj_get_tree(const obj_t *obj, bool detailed);

/*
 * Function: obj_get_path
 * Return the path of the object relative to a root object.
 *
 * Parameters:
 *   obj    - The object.
 *   root   - The base object or NULL for the global path ("core.xyz").
 *
 * Return:
 *   A newly allocated string.  Caller should delete it.
 */
char *obj_get_path(const obj_t *obj, const obj_t *root);

/*
 * Function: obj_list
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
 *   The number of objects listed.
 */
int obj_list(const obj_t *obj, observer_t *obs, double max_mag, void *user,
             int (*f)(const char *id, void *user));

/*
 * Function: obj_add_res
 * Register a new resource into an object.
 *
 * This is a bit experimental, maybe I will remove that.  The idea is that
 * we can just put resources into a public URL online and any module that
 * can handle it will automatically do so.
 */
obj_t *obj_add_res(obj_t *obj, const char *data, const char *base_path);

/*
 * Function: obj_add_global_listener
 * Register a callback to be called anytime an attribute of an object changes.
 *
 * For the moment we can only have one listener for all the object.  This
 * is enough for the javascript binding.
 */
void obj_add_global_listener(void (*f)(obj_t *obj, const char *attr));

/*
 * Function: obj_changed
 * Should be called by object after they manually change one of their
 * attributes.
 */
void obj_changed(obj_t *obj, const char *attr);

/*
 * Section: attributes manipulation.
 */

/*
 * Function: obj_get_attr
 * Get an attribute from an object.
 *
 * Parameters:
 *   obj    - An object.
 *   attr   - The name of the attribute.
 *   type   - Type string of the attribute (to remove?)
 *   ...    - Pointer to the output value.
 */
int obj_get_attr(const obj_t *obj, const char *attr, const char *type, ...);

/*
 * Function: obj_set_attr
 * Set an attribute from an object.
 *
 * Parameters:
 *   obj    - An object.
 *   attr   - The name of the attribute.
 *   type   - Type string of the attribute (to remove?)
 *   ...    - Pointer to the new value.
 */
int obj_set_attr(const obj_t *obj, const char *attr, const char *type, ...);


/*
 * Function: obj_has_attr
 * Check whether an object has a given attribute.
 */
bool obj_has_attr(const obj_t *obj, const char *attr);

/*
 * Function: obj_call_json
 * Call an object attribute function passing json arguments.
 */
json_value *obj_call_json(obj_t *obj, const char *attr,
                          const json_value *args);

/*
 * Function: obj_call_json_str
 * Same as obj_call_json, but the json is passed as a string.
 */
char *obj_call_json_str(obj_t *obj, const char *attr, const char *args);

/*
 * Function: obj_get_attr_
 * Return the actual <attribute_t> pointer for a given attr name
 *
 * XXX: need to use a proper name!
 */
const attribute_t *obj_get_attr_(const obj_t *obj, const char *attr);

/*
 * Function: obj_call
 * Call an object attribute function with array attributes.
 *
 * I think I will deprecate this and only allow to call functions using
 * json attribute.
 */
int obj_call(obj_t *obj, const char *attr, const char *sig, ...);


// Register an object klass, so that we can create instances dynamically
#define OBJ_REGISTER(klass) \
    static void obj_register_##klass##_(void) __attribute__((constructor)); \
    static void obj_register_##klass##_(void) { obj_register_(&klass); }
void obj_register_(obj_klass_t *klass);

// Return a pointer to the registered object klasses list.
obj_klass_t *obj_get_all_klasses(void);

void obj_foreach_attr(const obj_t *obj,
                      void *user,
                      void (*f)(const char *attr, int is_property, void *user));

obj_klass_t *obj_get_klass_by_name(const char *name);

#endif // OBJ_H
