
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

