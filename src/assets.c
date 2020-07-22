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

#ifdef __EMSCRIPTEN__
static const bool HAS_FS = false;
#else
static const bool HAS_FS = true;
#endif

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

// Global hook function.
static struct {
    void *user;
    void *(*fn)(void *user, const char *path, int *size, int *code);
} g_hook = {};

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

/*
 * Remove the parameters part of an url
 * eg: /something?v=10 -> /something
 */
static void remove_url_parameters(const char *url, char *out, int size)
{
    char *pos;
    pos = strrchr(url, '?');
    if (!pos)
        snprintf(out, size, "%s", url);
    else
        snprintf(out, size, "%.*s", (int)(pos - url), url);
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
    int r, default_size, default_code;
    const void *data = NULL;
    (void)r;
    char path[1204];

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

    // Apply hook if set.
    if (g_hook.fn && !asset->request && !asset->data) {
        asset->data = g_hook.fn(g_hook.user, url, &asset->size, code);
        if (*code != -1) {
            asset->flags |= FREE_DATA;
            *size = asset->size;
            data = asset->data;
            goto end;
        }
        *code = 0;
    }

    // Special handler for local files.
    if (HAS_FS && !asset->data && !strchr(url, ':')) {
        remove_url_parameters(url, path, sizeof(path));
        if (!file_exists(path)) {
            *code = 404;
            goto end;
        }
        asset->data = read_file(path, &asset->size);
        asset->flags |= FREE_DATA;
    }

    if (asset->data) {
        *code = 200;
        *size = asset->size;
        return asset->data;
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
    if (*code && data && (flags & ASSET_USED_ONCE))
        asset->flags |= CAN_RELEASE;

    // All error return codes return NULL data.
    if (*code >= 400) data = NULL;

end:
    LOG_RET(asset, url, *code, flags);
    return data;
}

static int asset_release_(asset_t *asset)
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
    return 0;
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

void asset_release(const char *url)
{
    asset_t *asset;
    HASH_FIND_STR(g_assets, url, asset);
    if (!asset) return;
    asset_release_(asset);
}

/*
 * Function: asset_set_hook
 * Set a global function to handle special urls.
 *
 * The hook function will be called for each new requests, and will bypass
 * the normal query, except if the return code is set to -1.
 */
void asset_set_hook(void *user,
        void *(*fn)(void *user, const char *url, int *size, int *code))
{
    assert(!g_hook.fn);
    g_hook.user = user;
    g_hook.fn = fn;
}


#include "assets/font.inl"
#include "assets/planets.ini.inl"
#include "assets/shaders.inl"
#include "assets/symbols.png.inl"
#include "assets/textures.inl"
