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
int module_list_objs(const obj_t *module, observer_t *obs,
                     double max_mag, uint64_t hint, void *user,
                     int (*f)(void *user, obj_t *obj));

int module_list_objs2(const obj_t *obj, observer_t *obs,
                     double max_mag, void *user,
                     int (*f)(void *, obj_t *));

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
int module_add_data_source(obj_t *module, const char *url, const char *type,
                           json_value *args);


//XXX: probably should rename this to obj_query.
/*
 * Function: obj_get
 * Find an object by query.
 *
 * Parameters:
 *   module - The parent module we search from, NULL for all modules.
 *   query  - An identifier that represents the object, can be:
 *      - A direct object id (HD 456, NGC 8)
 *      - A module name (constellations)
 *      - An object name (polaris)
 *   flags  - always zero for the moment.
 */
obj_t *obj_get(const obj_t *module, const char *query, int flags);

/*
 * Function: obj_get_by_oid
 * Find an object by its oid.
 */
obj_t *obj_get_by_oid(const obj_t *module, uint64_t oid, uint64_t hint);

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
obj_t *module_add_new(obj_t *module, const char *type, const char *id,
                      json_value *args);

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
