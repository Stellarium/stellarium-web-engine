/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "cache.h"
#include "uthash.h"
#include <assert.h>

typedef struct item item_t;
struct item {
    UT_hash_handle  hh;
    char            key[256];
    void            *data;
    int             cost;
    uint64_t        last_used;
    int             (*delfunc)(void *data);
};

struct cache {
    item_t *items;
    uint64_t clock;
    int size;
    int max_size;
};

cache_t *cache_create(int size)
{
    cache_t *cache = calloc(1, sizeof(*cache));
    cache->max_size = size;
    return cache;
}

static void cleanup(cache_t *cache)
{
    item_t *item, *tmp;
    HASH_ITER(hh, cache->items, item, tmp) {
        if (item->delfunc && item->delfunc(item->data) == CACHE_KEEP)
            continue;
        HASH_DEL(cache->items, item);
        assert(item != cache->items);
        cache->size -= item->cost;
        free(item);
        if (cache->size < cache->max_size) return;
    }
}

void cache_add(cache_t *cache, const void *key, int len, void *data,
               int cost, int (*delfunc)(void *data))
{
    item_t *item;
    assert(len <= sizeof(item->key));
    cache->size += cost;
    if (cache->size >= cache->max_size) cleanup(cache);
    item = calloc(1, sizeof(*item));
    memcpy(item->key, key, len);
    item->data = data;
    item->cost = cost;
    item->last_used = cache->clock++;
    item->delfunc = delfunc;
    HASH_ADD(hh, cache->items, key, len, item);
}

void *cache_get(cache_t *cache, const void *key, int keylen)
{
    item_t *item;
    HASH_FIND(hh, cache->items, key, keylen, item);
    if (!item) return NULL;
    item->last_used = cache->clock++;
    // Reinsert item on top of the hash list so that it stays sorted.
    HASH_DEL(cache->items, item);
    HASH_ADD(hh, cache->items, key, keylen, item);
    return item->data;
}

void cache_set_cost(cache_t *cache, const void *key, int keylen, int cost)
{
    item_t *item;
    HASH_FIND(hh, cache->items, key, keylen, item);
    if (!item) return;
    cache->size -= item->cost;
    item->cost = cost;
    cache->size += cost;
    if (cache->size >= cache->max_size) cleanup(cache);
}

/*
 * Function: cache_get_current_size
 * Return the total cost of all the currently cached items
 */
int cache_get_current_size(const cache_t *cache)
{
    return cache->size;
}
