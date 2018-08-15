/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <sys/stat.h>

static void assets_update(void);

enum {
    STATIC      = 1 << 0,
    COMPRESSED  = 1 << 1,
};

typedef struct asset asset_t;
struct asset
{
    UT_hash_handle  hh;
    char            *url;
    request_t       *request;
    int             flags;
    int             compressed_size;
    void            *compressed_data;
    void            *data;
    int             size;
    int             last_used;
};

// Global map of all the assets.
static asset_t *g_assets = NULL;

// Global list of the alias.
static struct {
    char *base;
    char *alias;
} g_alias[8] = {};

static asset_t *asset_get(const char *url)
{
    asset_t *asset;
    HASH_FIND_STR(g_assets, url, asset);
    if (!asset && str_startswith(url, "asset://")) return NULL;
    if (!asset) {
        asset = calloc(1, sizeof(*asset));
        asset->url = strdup(url);
        HASH_ADD_KEYPTR(hh, g_assets, asset->url, strlen(asset->url), asset);
    }
    asset->last_used = 0;
    assets_update();
    return asset;
}

void asset_register(const char *url, const void *data, int size,
                    bool compressed)
{
    asset_t *asset;
    assert(str_startswith(url, "asset://"));
    asset = calloc(1, sizeof(*asset));
    asset->flags = STATIC;
    asset->url = url;
    if (compressed) {
        asset->flags |= COMPRESSED;
        asset->compressed_data = (void*)data;
        asset->compressed_size = size;
    } else {
        asset->data = (void*)data;
        asset->size = size;
    }
    HASH_ADD_KEYPTR(hh, g_assets, asset->url,
                    strlen(asset->url), asset);
}

const void *asset_get_data(const char *url, int *size, int *code)
{
    asset_t *asset;
    int i, r, default_size;
    char alias[1024];
    const void *data;
    (void)r;
    size = size ?: &default_size;
    asset = asset_get(url);

    if (!asset) {
        if (code) *code = 404;
        return NULL;
    }

    if (code) *code = 0;
    if (!asset->data && asset->compressed_data) {
        asset->size = ((uint32_t*)asset->compressed_data)[0];
        assert(asset->size > 0);
        // Always add a NULL byte at the end so that text data are properly
        // null terminated.
        asset->data = malloc(asset->size + 1);
        ((char*)asset->data)[asset->size] = '\0';
        r = z_uncompress(asset->data, asset->size,
                         asset->compressed_data + 4,
                         asset->compressed_size - 4);
        assert(r == 0);
    }

    if (asset->data) {
        if (code) *code = 200;
        *size = asset->size;
        return asset->data;
    }

    // Check if we can use an alias.
    for (i = 0; i < ARRAY_SIZE(g_alias); i++) {
        if (!g_alias[i].base) break;
        if (str_startswith(url, g_alias[i].base)) {
            sprintf(alias, "%s%s",
                    g_alias[i].alias, url + strlen(g_alias[i].base));
            // Remove http parameters for alias!
            if (strrchr(alias, '?')) *strrchr(alias, '?') = '\0';
            data = asset_get_data(alias, size, code);
            if (data) return data;
        }
    }

    if (!asset->request) asset->request = request_create(asset->url);
    return request_get_data(asset->request, size, code);
}

static void assets_update(void)
{
    asset_t *asset, *tmp;
    HASH_ITER(hh, g_assets, asset, tmp) {
        if (asset->last_used++ < 128) continue;
        if (asset->flags & STATIC) continue;
        // XXX: to finish: delete unused assets.
    }
}

const char *asset_iter_(const char *base, void **i)
{
    asset_t *asset = (*i);
    if (asset == NULL) asset = g_assets;
    else asset = asset->hh.next;
    while (asset && !str_startswith(asset->url, base)) {
        asset = asset->hh.next;
    }
    *i = asset;
    return asset ? asset->url : NULL;
}

void asset_set_alias(const char *base, const char *alias)
{
    int i;
    assert(str_startswith(base, "http://") ||
           str_startswith(base, "https://"));
    assert(str_startswith(alias, "asset://"));
    assert(!str_endswith(base, "/"));
    assert(!str_endswith(alias, "/"));
    for (i = 0; i < ARRAY_SIZE(g_alias); i++) {
        if (!g_alias[i].base) break;
    }
    assert(i < ARRAY_SIZE(g_alias));
    g_alias[i].base = strdup(base);
    g_alias[i].alias = strdup(alias);
}

#include "assets/cities.txt.inl"
#include "assets/font.inl"
#include "assets/landscapes.inl"
#include "assets/mpcorb.dat.inl"
#include "assets/planets.ini.inl"
#include "assets/skycultures.inl"
#include "assets/shaders.inl"
#include "assets/stars.inl"
#include "assets/symbols.png.inl"
#include "assets/textures.inl"
