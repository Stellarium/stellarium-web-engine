/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef MODULE_H
#define MODULE_H

enum {
    MODULE_AGAIN       = 2 // Returned if obj_list can be called again.
};


/*
 * Function: module_update
 * Update the module.
 *
 * Parameters:
 *   dt  - User delta time (used for example for fading effects).
 */
int module_update(obj_t *module, double dt);

/*
 * Function: module_list_obj
 * List all astro objects in a module.
 *
 * Parameters:
 *   module   - The module (core for all objects).
 *   max_mag  - If set to a value different than NAN, filter out the objects
 *              that we know can never have a magnitude lower than this
 *              value.  This is just a hint given to the modules, and the
 *              caller should still check the magnitude if needed.
 *   source   - Only consider objects from the given data source.  Can be
 *              set to NULL to ignore.
 *   user     - Data passed to the callback.
 *   f        - Callback function called once per object.
 *
 * Return:
 *    0           - Success.
 *   -1           - The object doesn't support listing, or a hint is needed.
 *   MODULE_AGAIN - Some resources are still loading and so calling the
 *                  function again later might return more values.
 */
int module_list_objs(const obj_t *module,
                     double max_mag, uint64_t hint, const char *source,
                     void *user, int (*f)(void *user, obj_t *obj))
__attribute__((nonnull(1, 6)));

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
int module_add_data_source(obj_t *module, const char *url, const char *key);

/*
 * Function: obj_get_by_hip
 * Find a star object by its Hipparcos number.
 *
 * Note: this could be replaced by obj_get, but this is faster for the case
 * of HIP, and most important this function returns an error code that can
 * be used to know if we can call again.
 *
 * Parameters:
 *   hip    - A Hipparcos number.
 *   code   - Output for request code:
 *            200 - OK
 *            404 - Not found.
 *              0 - Not found yet, but we can try again later.
 *
 * Return:
 *   A star object, or NULL.  The caller is responsible for calling
 *   obj_release on the object.
 */
obj_t *obj_get_by_hip(int hip, int *code);

/*
 * Function: module_get_render_order
 *
 * For modules: return the order in which the modules should be rendered.
 * NOTE: if we used deferred rendering this wouldn't be needed at all!
 */
double module_get_render_order(const obj_t *module);

/*
 * Function: module_add_global_listener
 * Register a callback to be called anytime an attribute of a module changes.
 *
 * For the moment we can only have one listener for all the modules.  This
 * is enough for the javascript binding.
 */
void module_add_global_listener(void (*f)(obj_t *module, const char *attr));

/*
 * Function: module_changed
 * Should be called by modules after they manually change one of their
 * attributes.
 */
void module_changed(obj_t *module, const char *attr);

/*
 * Macro: MODULE_ITER
 * Iter all the children of a given module of a given type.
 *
 * Properties:
 *   module - The module we want to iterate.
 *   child  - Pointer to an object, that will be set with each child.
 *   klass_ - Children klass type id string, or NULL for no filter.
 */
#define MODULE_ITER(module, child, klass_) \
    for (child = (void*)(((obj_t*)module)->children); child; \
                        child = (void*)(((obj_t*)child)->next)) \
        if (!(klass_) || \
                ((((obj_t*)child)->klass) && \
                 (((obj_t*)child)->klass->id) && \
                 (strcmp(((obj_t*)child)->klass->id, klass_ ?: "") == 0)))

/*
 * Function: module_add
 * Add an object as a child of a module
 */
void module_add(obj_t *module, obj_t *child);

/*
 * Function: module_add_new
 * Add a new object as a child of a module
 *
 * The object is owned by the module, so we don't have to call obj_release
 * but instead module_remove.
 */
obj_t *module_add_new(obj_t *module, const char *type, json_value *args);

/*
 * Function: module_remove
 * Remove an object from a parent.
 */
void module_remove(obj_t *module, obj_t *child);

/*
 * Function: module_get_child
 * Return a module child by id.
 *
 * Note: this increases the ref counting of the returned module.
 */
obj_t *module_get_child(const obj_t *module, const char *id);

/*
 * Function: module_get_tree
 * Return a json tree of all the attributes and children of this module.
 *
 * Parameters:
 *   obj        - The root object or NULL for global tree (starts at 'core')
 *   detailed   - Whether to add hints to the values or not.
 *
 * Return:
 *   A newly allocated string.  Caller should delete it.
 */
char *module_get_tree(const obj_t *obj, bool detailed);

/*
 * Function: obj_get_path
 * Return the path of the module relative to a root module.
 *
 * Parameters:
 *   obj    - The object.
 *   root   - The base object or NULL for the global path ("core.xyz").
 *
 * Return:
 *   A newly allocated string.  Caller should delete it.
 */
char *module_get_path(const obj_t *obj, const obj_t *root);

#endif // MODULE_H
