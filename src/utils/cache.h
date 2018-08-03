/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: cache.h
 *
 * Utils to store values in cache.
 */

/*
 * Enum: CACHE_KEEP
 * The cache delete function callback can return this value to tell the
 * cache not to delete a item yet.
 */
#define CACHE_KEEP 1

/*
 * Type: cache_t
 * Represent a cache that can contain any kind of data.
 */
typedef struct cache cache_t;

/*
 * Function: cache_create
 * Create a new cache with a given max size.
 *
 * Parameters:
 *   size - Maximum cache size.  The unit can be anything, as long as it
 *          stays consistent with the cost argument given in cache_add.
 *
 * Return:
 *   A new cache object.
 */
cache_t *cache_create(int size);

/*
 * Function: cache_add
 * Add an item into a cache.
 *
 * Parameters:
 *  key     - Pointer to the unique key for the data.
 *  keylen  - Size of the key.
 *  data    - Pointer to the item data.  The cache takes ownership.
 *  cost    - Cost of the data used to compute the cache usage.
 *            It doesn't have to be the size.
 *  delfunc - Function that the case can use to free the data when the
 *            cache gets too large.
 */
void cache_add(cache_t *cache, const void *key, int keylen, void *data,
               int cost, int (*delfunc)(void *data));

/*
 * Function: cache_get
 * Retrieve an item from the cache.
 *
 * Returns:
 *   The data owned by the cache, or NULL if no item with this key is in
 *   the cache.
 */
void *cache_get(cache_t *cache, const void *key, int keylen);

/*
 * Function: cache_set_cost
 * Change the cost of an item already in the cache.
 *
 * This the same a removing the item and adding it back with the new cost.
 */
void cache_set_cost(cache_t *cache, const void *key, int keylen, int cost);

/*
 * Function: cache_get_current_size
 * Return the total cost of all the currently cached items
 */
int cache_get_current_size(const cache_t *cache);

