/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
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
#include <sys/time.h>

typedef struct item item_t;
struct item {
    UT_hash_handle  hh;
    char            key[256];
    void            *data;
    int             cost;
    int             (*delfunc)(void *data);
    // Used to give a grace period before we remove an item from the cache.
    double          grace_time;
};

struct cache {
    item_t *items;
    int size;
    int max_size;
    double grace_period;
};

static double get_unix_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000. / 1000.;
}

cache_t *cache_create(int size, double grace_period_sec)
{
    cache_t *cache = calloc(1, sizeof(*cache));
    cache->max_size = size;
    cache->grace_period = grace_period_sec;
    return cache;
}

static void cleanup(cache_t *cache)
{
    item_t *item, *tmp;
    double time = get_unix_time();
    HASH_ITER(hh, cache->items, item, tmp) {
        if (!item->grace_time) {
            item->grace_time = time;
            continue;
        }
        if (time - item->grace_time < cache->grace_period) continue;

        if (item->delfunc && item->delfunc(item->data) == CACHE_KEEP) {
            item->grace_time = 0;
            continue;
        }
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
    item->delfunc = delfunc;
    HASH_ADD(hh, cache->items, key, len, item);
}

void *cache_get(cache_t *cache, const void *key, int keylen)
{
    item_t *item;
    HASH_FIND(hh, cache->items, key, keylen, item);
    if (!item) return NULL;
    item->grace_time = 0;
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
