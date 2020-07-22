/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdbool.h>

/*
 * File: assets.h
 *
 * Asset manager that can be used as an abstraction over bundled, local file
 * system or online data.
 *
 * All assets are uniquely identified by a url, that can be either:
 * - A url to an online resource (https://something).
 * - A bundled data url (asset://something).
 * - A local filesytem path (/path/to/something).
 *
 * The function <asset_get_data> return the data associated with an url
 * if available, and the function <asset_release> is a hint to the assets
 * manager that we won't need this asset anymore.
 */

/*
 * Enum: ASSET_FLAGS
 * Flags that can be passed to asset_get to optimize the network requests.
 *
 * Values:
 *   ASSET_DELAY        - Delay the network request for a few seconds.  This
 *                        is useful to prevent loading tiles resources too
 *                        quickly.
 *   ASSET_ACCEPT_404   - Do not log error on a 404 return.
 *   ASSET_USED_ONCE    - Hint that the data can be release after it has
 *                        been read.
 */
enum {
    ASSET_DELAY             = 1 << 0,
    ASSET_ACCEPT_404        = 1 << 1,
    ASSET_USED_ONCE         = 1 << 2,
};

/*
 * Function: asset_get_data
 * Get the data associated with an asset url.
 *
 * This is non blocking.  If the asset is an online resource, then the
 * function returns NULL, set the return code to zero, and will need to be
 * called again in the future.
 *
 * The data returned is owned by the asset manager and so should not be
 * directly freed by the caller function (instead we can use <asset_release> to
 * notify that we won't need this asset anymore).
 *
 * Parameters:
 *   url    - url to an asset.
 *   size   - get the size of the returned data.  Can be NULL.
 *   code   - get the return code of the request.  For http assets this
 *            is set to the http return code, for other requests the code
 *            is set to either 200 or 404.  If the requests hasn't completed
 *            yet, the value is set to 0.
 *
 * Return:
 *   A pointer to the asset data, or NULL in case of error or if the data
 *   is not available yet.  To distinguish between error and loading, we
 *   have to check the code value.
 *
 */
const void *asset_get_data(const char *url, int *size, int *code);

/*
 * Same as asset_get_data, but accepts a flag argument.
 */
const void *asset_get_data2(const char *url, int flags, int *size, int *code);

/*
 * Function: asset_release
 * Release the memory associated with an asset.
 *
 * This should be called after asset_get_data, once we don't need the data
 * anymore.
 */
void asset_release(const char *url);

/*
 * Macro: ASSET_ITER
 * Iter all the asset url that start with a given prefix.
 *
 * Probably need to be replaced by something cleaner.
 */
#define ASSET_ITER(base_, path_) \
    for (void *i_ = NULL; (path_ = asset_iter_(base_, &i_)); )
const char *asset_iter_(const char *base, void **i);

/*
 * Function: asset_register
 * Register a bundled asset with a given url
 *
 * Not supposed to be used directly.  Instead we should use the ASSET_REGISTER
 * macro.
 */
void asset_register(const char *url, const void *data, int size,
                    bool compressed);

#define ASSET_REGISTER(id_, name_, data_, comp_) \
    static void register_asset_##id_(void) __attribute__((constructor)); \
    static void register_asset_##id_(void) { \
        asset_register("asset://" name_, data_, sizeof(data_), comp_); }

/*
 * Function: asset_set_hook
 * Set a global function to handle special urls.
 *
 * The hook function will be called for each new requests, and will bypass
 * the normal query, except if the return code is set to -1.
 */
void asset_set_hook(void *user,
        void *(*fn)(void *user, const char *url, int *size, int *code));
