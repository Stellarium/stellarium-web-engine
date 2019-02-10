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

static const int DEFAULT_DELAY = 60;


static void assets_update(void);

enum {
    STATIC      = 1 << 8,
    COMPRESSED  = 1 << 9,
    FREE_DATA   = 1 << 10,
    LOGGED      = 1 << 11,
    CAN_RELEASE = 1 << 12,
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
    int             delay;
};

// Global map of all the assets.
static asset_t *g_assets = NULL;

// Global list of the alias.
static struct {
    char *base;
    char *alias;
} g_alias[8] = {};

// Global list of handlers for special url schemes.
static struct {
    char *prefix;
    void *(*fn)(const char *path, int *size, int *code);
} g_handlers[2] = {};

/*
 * Convenience function to log return code errors if needed.
 */
void LOG_RET(asset_t *asset, const char *url, int code, int flags)
{
    if (code < ((flags & ASSET_ACCEPT_404) ? 500 : 400)) return;
    if (asset && (asset->flags & LOGGED)) return;
    LOG_W("Asset error %d: %s", code, url);
    if (asset) asset->flags |= LOGGED;
}


static bool file_exists(const char *path)
{
    FILE *file;
    if ((file = fopen(path, "r"))) {
        fclose(file);
        return true;
    }
    return false;
}

static asset_t *asset_get(const char *url, int flags)
{
    asset_t *asset;
    HASH_FIND_STR(g_assets, url, asset);
    if (!asset && str_startswith(url, "asset://")) return NULL;
    if (!asset) {
        asset = calloc(1, sizeof(*asset));
        asset->url = strdup(url);
        asset->flags |= flags;
        if (flags & ASSET_DELAY) asset->delay = DEFAULT_DELAY;
        HASH_ADD_KEYPTR(hh, g_assets, asset->url, strlen(asset->url), asset);
    }
    asset->last_used = 0;
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
    return asset_get_data2(url, 0, size, code);
}

const void *asset_get_data2(const char *url, int flags, int *size, int *code)
{
    asset_t *asset;
    int i, r, default_size, default_code;
    char alias[1024];
    const void *data = NULL;
    (void)r;
    size = size ?: &default_size;
    code = code ?: &default_code;

    assets_update();
    asset = asset_get(url, flags);
    *code = 0;
    *size = 0;

    if (!asset) {
        *code = 404;
        goto end;
    }

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
        asset->flags |= FREE_DATA;
        assert(r == 0);
    }

    // Special handler for local files.
    if (!asset->data && !strchr(url, ':')) {
        if (!file_exists(url)) {
            *code = 404;
            goto end;
        }
        asset->data = read_file(url, &asset->size);
        asset->flags |= FREE_DATA;
    }

    if (asset->data) {
        *code = 200;
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
            data = asset_get_data2(alias, flags | ASSET_ACCEPT_404, size, code);
            if (data) goto end;
            *code = 0;
        }
    }

    // Check if we have a special handler for this asset.
    for (i = 0; i < ARRAY_SIZE(g_handlers); i++) {
        if (!g_handlers[i].prefix) break;
        if (str_startswith(url, g_handlers[i].prefix)) {
            asset->data = g_handlers[i].fn(url, &asset->size, code);
            asset->flags |= FREE_DATA;
            *size = asset->size;
            data = asset->data;
            goto end;
        }
    }

    if (!asset->request) {
        if (asset->delay) {
            asset->delay--;
            assert(*code == 0 && *size == 0);
            return NULL;
        }
        asset->request = request_create(asset->url);
    }
    data = request_get_data(asset->request, size, code);
    if (*code && (flags & ASSET_USED_ONCE))
        asset->flags |= CAN_RELEASE;

end:
    LOG_RET(asset, url, *code, flags);
    return data;
}

static void asset_release_(asset_t *asset)
{
    if (asset->flags & FREE_DATA) {
        free(asset->data);
        asset->data = NULL;
        asset->size = 0;
    }
    if (asset->request)
        request_delete(asset->request);
    if (!(asset->flags & STATIC)) {
        HASH_DEL(g_assets, asset);
        free(asset->url);
        free(asset);
    }
}

static void assets_update(void)
{
    asset_t *asset, *tmp;
    HASH_ITER(hh, g_assets, asset, tmp) {
        if ((asset->flags & CAN_RELEASE)) {
            asset_release_(asset);
            continue;
        }
        if (asset->flags & STATIC) continue;
        if (asset->last_used++ < 128) continue;
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
    assert(!str_endswith(base, "/"));
    assert(!str_endswith(alias, "/"));
    for (i = 0; i < ARRAY_SIZE(g_alias); i++) {
        if (!g_alias[i].base) break;
    }
    assert(i < ARRAY_SIZE(g_alias));
    g_alias[i].base = strdup(base);
    g_alias[i].alias = strdup(alias);
}

void asset_release(const char *url)
{
    asset_t *asset;
    HASH_FIND_STR(g_assets, url, asset);
    if (!asset) return;
    asset_release_(asset);
}

void asset_add_handler(
        const char *prefix,
        void *(*handler)(const char *path, int *size, int *code))
{
    int i;
    for (i = 0; i < ARRAY_SIZE(g_handlers); i++) {
        if (!g_handlers[i].prefix) break;
    }
    assert(i < ARRAY_SIZE(g_handlers));
    g_handlers[i].prefix = strdup(prefix);
    g_handlers[i].fn = handler;
}

#include "assets/cities.txt.inl"
#include "assets/font.inl"
#include "assets/mpcorb.dat.inl"
#include "assets/planets.ini.inl"
#include "assets/skycultures.inl"
#include "assets/shaders.inl"
#include "assets/stars.inl"
#include "assets/symbols.png.inl"
#include "assets/textures.inl"
