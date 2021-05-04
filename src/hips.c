/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "ini.h"
#include <string.h>
#include <zlib.h> // For crc32.

// Should be good enough...
#define URL_MAX_SIZE 4096

// Size of the cache allocated to all the hips tiles.
// Note: we get into trouble if the tiles visible on screen actually use
// more space than that.  We could use a more clever cache that can grow
// past its limit if the items are still in use!
#define CACHE_SIZE (256 * (1 << 20))

// Flags of the tiles:
enum {
    // Bit fields set by tile if we know that we don't have further tiles
    // for a given child.
    TILE_NO_CHILD_0     = 1 << 0,
    TILE_NO_CHILD_1     = 1 << 1,
    TILE_NO_CHILD_2     = 1 << 2,
    TILE_NO_CHILD_3     = 1 << 3,

    TILE_LOAD_ERROR     = 1 << 4,
};

#define TILE_NO_CHILD_ALL \
    (TILE_NO_CHILD_0 | TILE_NO_CHILD_1 | TILE_NO_CHILD_2 | TILE_NO_CHILD_3)

typedef struct tile tile_t;
struct tile {
    struct {
        int order;
        int pix;
    } pos;
    hips_t      *hips;
    fader_t     fader;
    int         flags;
    const void  *data;

    // Loader to parse the image in a thread.
    struct {
        worker_t worker;
        tile_t *tile;
        void *data;
        int size;
        int cost;
    } *loader;
};

/*
 * Type: tile_key_t
 * Key used for the tiles cache.
 */
typedef struct {
    uint32_t    hips_hash;
    int         order;
    int         pix;
} tile_key_t;

/*
 * Type: img_tile_t
 * type data for images surveys.
 */
typedef struct {
    void        *img;
    int         w, h, bpp;
    texture_t   *tex;
} img_tile_t;

// Gobal cache for all the tiles.
static cache_t *g_cache = NULL;


static const void *create_img_tile(
        void *user, int order, int pix, void *src, int size,
        int *cost, int *transparency);
static int delete_img_tile(void *tile);

hips_t *hips_create(const char *url, double release_date,
                    const hips_settings_t *settings)
{
    const hips_settings_t default_settings = {
        .create_tile = create_img_tile,
        .delete_tile = delete_img_tile,
    };
    hips_t *hips = calloc(1, sizeof(*hips));
    if (!settings) settings = &default_settings;

    hips->ref = 1;
    hips->settings = *settings;
    hips->url = strdup(url);
    hips->service_url = strdup(url);
    hips->ext = settings->ext ?: "jpg";
    hips->order_min = 3;
    hips->release_date = release_date;
    hips->frame = FRAME_ASTROM;
    hips->hash = crc32(0, (void*)url, strlen(url));
    return hips;
}

void hips_delete(hips_t *hips)
{
    int i;
    if (!hips) return;
    hips->ref--;
    assert(hips->ref >= 0);
    if (hips->ref > 0) return;
    free(hips->url);
    free(hips->service_url);
    for (i = 0; i < 12; i++)
        texture_release(hips->allsky.textures[i]);
    json_builder_free(hips->properties);
    free(hips);
}

void hips_set_frame(hips_t *hips, int frame)
{
    hips->frame = frame;
}

// Get the url for a given file in the survey.
// Automatically add ?v=<release_date> for online surveys.
static const char *get_url_for(const hips_t *hips, char *buf,
                               const char *format, ...)
    __attribute__((format(printf, 3, 4)));

static const char *get_url_for(const hips_t *hips, char *buf,
                               const char *format, ...)
{
    va_list ap;
    char *p = buf;

    va_start(ap, format);
    p += sprintf(p, "%s/", hips->service_url);
    p += vsprintf(p, format, ap);
    va_end(ap);

    // If we are using http, add the release date parameter for better
    // cache control.
    if (    hips->release_date &&
            (strncmp(hips->service_url, "http://", 7) == 0 ||
             strncmp(hips->service_url, "https://", 8) == 0)) {
        sprintf(p, "?v=%d", (int)hips->release_date);
    }
    return buf;
}

static int property_handler(void* user, const char* section,
                            const char* name, const char* value)
{
    hips_t *hips = user;
    double version;

    json_object_push(hips->properties, name, json_string_new(value));
    if (strcmp(name, "hips_order") == 0)
        hips->order = atoi(value);
    if (strcmp(name, "hips_order_min") == 0)
        hips->order_min = atoi(value);
    if (strcmp(name, "hips_tile_width") == 0)
        hips->tile_width = atoi(value);
    if (strcmp(name, "hips_release_date") == 0)
        hips->release_date = hips_parse_date(value);
    if (strcmp(name, "hips_tile_format") == 0) {
             if (strstr(value, "webp")) hips->ext = "webp";
        else if (strstr(value, "jpeg")) hips->ext = "jpg";
        else if (strstr(value, "png"))  hips->ext = "png";
        else if (strstr(value, "eph"))  {
            hips->ext = "eph";
            hips->allsky.not_available = true;
        } else if (!hips->ext || !strstr(value, hips->ext)) {
            LOG_W("Unknown hips format: %s", value);
        }
    }

    /* Starting from version 1.4, hips format doesn't have allsky texture.
     * XXX: probably better to disable allsky by default, and only use if
     * the property file has an allsky attribute (for the planets).  */
    if (strcmp(name, "hips_version") == 0) {
        version = atof(value);
        if (version >= 1.4) hips->allsky.not_available = true;
    }

    // Guillaume 2018 Aug 30: disable the hips_service_url, because
    // it poses probleme when it changes the protocole from https to
    // http.  Still not sure if we are supposed to use it of it it's just
    // a hint.
    /*
    if (strcmp(name, "hips_service_url") == 0) {
        free(hips->service_url);
        hips->service_url = strdup(value);
    }
    */
    return 0;
}


static int parse_properties(hips_t *hips)
{
    const char *data;
    char url[URL_MAX_SIZE];
    int code;
    get_url_for(hips, url, "properties");
    data = asset_get_data2(url, ASSET_USED_ONCE, NULL, &code);
    if (!data && code) {
        LOG_E("Cannot get hips properties file at '%s': %d", url, code);
        return -1;
    }
    if (!data) return 0;
    hips->properties = json_object_new(0);
    ini_parse_string(data, property_handler, hips);
    return 0;
}


// Used by the cache.
static int del_tile(void *data)
{
    tile_t *tile = data;
    // XXX: why the worker_is_running?
    if (tile->loader && worker_is_running(&tile->loader->worker))
        return CACHE_KEEP;
    if (tile->data) {
        if (tile->hips->settings.delete_tile(tile->data) == CACHE_KEEP)
            return CACHE_KEEP;
    }
    hips_delete(tile->hips);
    free(tile);
    return 0;
}

static bool img_is_transparent(
        const uint8_t *img, int img_w, int img_h, int bpp,
        int x, int y, int w, int h)
{
    int i, j;
    if (bpp < 4) return false;
    assert(bpp == 4);
    for (i = y; i < y + h; i++)
        for (j = x; j < x + w; j++)
            if (img[(i * img_w + j) * 4 + 3])
                return false;
    return true;
}

int hips_traverse(void *user, int callback(int order, int pix, void *user))
{
    typedef struct {
        int order;
        int pix;
    } node_t;
    node_t queue[1024];
    const int n = ARRAY_SIZE(queue);
    int start = 0, size = 12, r, i;
    int order, pix;
    // Enqueue the first 12 pix at order 0.
    for (i = 0; i < 12; i++) {queue[i] = (node_t){0, i};}
    while (size) {
        order = queue[start % n].order;
        pix = queue[start % n].pix;
        start++;
        size--;
        r = callback(order, pix, user);
        if (r < 0) return r;
        if (r == 1) {
            // Enqueue the 4 children tiles.
            if (size + 4 >= n) {
                // No more space.
                LOG_W("Abort HIPS traverse");
                return -1;
            }
            for (i = 0; i < 4; i++) {
                queue[(start + size) % n] = (node_t){order + 1, pix * 4 + i};
                size++;
            }
        }
    }
    return 0;
}

/*
 * Function: hips_iter_init
 * Initialize the iterator with the initial twelve order zero healpix pixels.
 */
void hips_iter_init(hips_iterator_t *iter)
{
    int i;
    typedef __typeof__(iter->queue[0]) node_t;
    memset(iter, 0, sizeof(*iter));
    // Enqueue the first 12 pix at order 0.
    iter->size = 12;
    for (i = 0; i < 12; i++) {
        iter->queue[i] = (node_t){0, i};
    }
}

/*
 * Function: hips_iter_next
 * Pop the next healpix pixel from the iterator.
 *
 * Return false if there are no more pixel enqueued.
 */
bool hips_iter_next(hips_iterator_t *iter, int *order, int *pix)
{
    const int n = ARRAY_SIZE(iter->queue);
    if (!iter->size) return false;
    // Get the first tile from the queue.
    *order = iter->queue[iter->start % n].order;
    *pix = iter->queue[iter->start % n].pix;
    iter->start++;
    iter->size--;
    return true;
}

/*
 * Function: hips_iter_push_children
 * Add the four children of the giver pixel to the iterator.
 *
 * The children will be retrieved after all the currently queued values
 * from the iterator have been processed.
 */
void hips_iter_push_children(hips_iterator_t *iter, int order, int pix)
{
    typedef __typeof__(iter->queue[0]) node_t;
    const int n = ARRAY_SIZE(iter->queue);
    int i;
    // Enqueue the next four tiles.
    if (iter->size + 4 >= n) {
        // Todo: we should probably use a dynamic array instead.
        LOG_E("No more space!");
        return;
    }
    for (i = 0; i < 4; i++) {
        iter->queue[(iter->start + iter->size) % n] = (node_t) {
            order + 1, pix * 4 + i
        };
        iter->size++;
    }
}


/*
 * Function: hips_get_tile_texture
 * Get the texture for a given hips tile.
 *
 * The algorithm is more or less:
 *   - If the tile is loaded, return its texture.
 *   - If not, try to use a parent tile as a fallback.
 *   - If no parent is loaded, but we have an allsky image, use it.
 *   - If all else failed, return NULL.  In that case the UV and projection
 *     are still set, so that the client can still render a fallback texture.
 *
 * Parameters:
 *   order   - Order of the tile we are looking for.
 *   pix     - Pixel index of the tile we are looking for.
 *   flags   - <HIPS_FLAGS> union.
 *   transf  - If the returned texture is larger than the healpix pixel,
 *             this matrix is multiplied by the transformation to apply
 *             to the original UV coordinates to get the part of the texture.
 *   fade    - Recommended fade alpha.
 *   loading_complete - set to true if the tile is totally loaded.
 *
 * Return:
 *   The texture_t, or NULL if none is found.
 */
texture_t *hips_get_tile_texture(
        hips_t *hips, int order, int pix, int flags,
        double transf[3][3], double *fade,
        bool *loading_complete)
{
    bool loading_complete_;
    int code, x, y, nbw;
    img_tile_t *tile = NULL;
    texture_t *tex;

    if (!loading_complete) loading_complete = &loading_complete_;
    // Set all the default values.
    *loading_complete = false;
    if (fade) *fade = 1.0;

    if (!hips_is_ready(hips)) return NULL;
    if (order < hips->order_min) return NULL;

    if (order <= hips->order && !(flags & HIPS_FORCE_USE_ALLSKY)) {
        tile = hips_get_tile(hips, order, pix, flags, &code);
        if (!tile && code && code != 598)
            *loading_complete = true;
    }

    // Create texture if needed.
    if (tile && tile->img && !tile->tex) {
        tile->tex = texture_from_data(tile->img, tile->w, tile->h, tile->bpp,
                                      0, 0, tile->w, tile->h, 0);
        free(tile->img);
        tile->img = NULL;
    }
    if (tile && tile->tex) {
        *loading_complete = true;
        return tile->tex;
    }


    // Return the allsky texture if the tile is not ready yet.  Only do
    // it for level 0 allsky for the moment.
    if (!tile && order == 0 && hips->allsky.data) {
        if (!hips->allsky.textures[pix]) {
            nbw = (int)sqrt(12 * (1 << (2 * hips->order_min)));
            x = (pix % nbw) * hips->allsky.w / nbw;
            y = (pix / nbw) * hips->allsky.w / nbw;
            hips->allsky.textures[pix] = texture_from_data(
                    hips->allsky.data, hips->allsky.w, hips->allsky.h,
                    hips->allsky.bpp,
                    x, y, hips->allsky.w / nbw, hips->allsky.w / nbw, 0);
        }
        if (flags & HIPS_FORCE_USE_ALLSKY) *loading_complete = true;
        return hips->allsky.textures[pix];
    }

    // If we didn't find the tile, or the texture is not loaded yet,
    // fallback to one of the parent tile texture.
    if (order == hips->order_min) return NULL; // No parent.
    tex = hips_get_tile_texture(
            hips, order - 1, pix / 4, flags, transf, NULL, NULL);
    if (!tex) return NULL;
    if (transf) {
        mat3_iscale(transf, 0.5, 0.5, 1.0);
        mat3_itranslate(transf, (pix % 4) / 2, (pix % 4) % 2);
    }
    return tex;
}


static int render_visitor(hips_t *hips, const painter_t *painter_,
                          const double transf[4][4],
                          int order, int pix, int split,
                          int *nb_tot, int *nb_loaded)
{
    painter_t painter = *painter_;
    texture_t *tex;
    uv_map_t map;
    bool loaded;
    double fade;
    // UV transfo mat with swapped x and y.
    const double uv_swap[3][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}};
    double uv[3][3] = MAT3_IDENTITY;
    int flags = HIPS_LOAD_IN_THREAD;

    (*nb_tot)++;
    tex = hips_get_tile_texture(hips, order, pix, flags, uv, &fade, &loaded);
    mat3_mul(uv, uv_swap, uv);
    if (loaded) (*nb_loaded)++;
    if (!tex) return 0;
    painter.color[3] *= fade;
    painter_set_texture(&painter, PAINTER_TEX_COLOR, tex, uv);
    uv_map_init_healpix(&map, order, pix, false, true);
    if (transf)
        map.transf = (void*)transf;
    paint_quad(&painter, hips->frame, &map, split);
    return 0;
}

int hips_render(hips_t *hips, const painter_t *painter,
                const double transf[4][4], int split_order)
{
    int nb_tot = 0, nb_loaded = 0;
    int render_order, order, pix, split;
    hips_iterator_t iter;
    uv_map_t map;

    assert(split_order >= 0);
    if (painter->color[3] == 0.0) return 0;
    if (!hips_is_ready(hips)) return 0;

    render_order = hips_get_render_order(hips, painter);
    // Clamp the render order into physically possible range.
    render_order = clamp(render_order, hips->order_min, hips->order);
    render_order = min(render_order, 9); // Hard limit.

    // Can't split less than the rendering order.
    split_order = max(split_order, render_order);

    // Breath first traversal of all the tiles.
    hips_iter_init(&iter);
    while (hips_iter_next(&iter, &order, &pix)) {
        // Early exit if the tile is clipped.
        uv_map_init_healpix(&map, order, pix, false, false);
        map.transf = (void*)transf;
        if (painter_is_quad_clipped(painter, hips->frame, &map))
            continue;
        if (order < render_order) { // Keep going.
            hips_iter_push_children(&iter, order, pix);
            continue;
        }
        split = 1 << (split_order - render_order);
        render_visitor(hips, painter, transf, order, pix, split,
                       &nb_tot, &nb_loaded);
    }

    progressbar_report(hips->url, hips->label, nb_loaded, nb_tot, -1);
    return 0;
}

static void init_label(hips_t *hips)
{
    const char *collection;
    const char *title;
    if (!hips->label) {
        collection = json_get_attr_s(hips->properties, "obs_collection");
        title = json_get_attr_s(hips->properties, "obs_title");
        if (collection) asprintf(&hips->label, "%s", collection);
        else if (title) asprintf(&hips->label, "%s", title);
        else asprintf(&hips->label, "%s", hips->url);
    }
}

void hips_set_label(hips_t *hips, const char* label)
{
    free(hips->label);
    asprintf(&hips->label, "%s", label);
}

static int load_allsky_worker(worker_t *worker)
{
    typeof(((hips_t*)0)->allsky) *allsky = (void*)worker;
    allsky->data = img_read_from_mem(allsky->src_data, allsky->size,
            &allsky->w, &allsky->h, &allsky->bpp);
    free(allsky->src_data);
    allsky->src_data = NULL;
    return 0;
}

bool hips_update(hips_t *hips)
{
    int code, err, size;
    char url[1024];
    char *data;
    if (hips->error) return false;
    if (!hips->properties) {
        err = parse_properties(hips);
        if (err) {
            LOG_E("Cannot parse hips property file (%s)", hips->url);
            hips->error = err;
        }
        if (!hips->properties) return false;
        init_label(hips);
    }

    // Get the allsky before anything else if available.
    // Only for level zero allsky images.  We don't use the other ones.
    if (!hips->allsky.worker.fn &&
            !hips->allsky.not_available && !hips->allsky.data &&
            hips->order_min == 0) {
        snprintf(url, sizeof(url), "%s/Norder%d/Allsky.%s?v=%d",
                 hips->service_url, hips->order_min, hips->ext,
                 (int)hips->release_date);
        data = asset_get_data2(url, ASSET_USED_ONCE, &size, &code);
        if (!code) return false;
        if (!data) hips->allsky.not_available = true;
        if (data) {
            worker_init(&hips->allsky.worker, load_allsky_worker);
            hips->ref++;
            hips->allsky.src_data = malloc(size);
            hips->allsky.size = size;
            memcpy(hips->allsky.src_data, data, size);
        }
    }

    // If the allsky image is loading wait for it to finish.
    if (hips->allsky.worker.fn) {
        if (!worker_iter(&hips->allsky.worker)) return false;
        hips_delete(hips); // Release ref from worker.
        if (!hips->allsky.data) hips->allsky.not_available = true;
        hips->allsky.worker.fn = NULL;
    }

    return true;
}

bool hips_is_ready(hips_t *hips)
{
    return hips_update(hips);
}

int hips_get_render_order(const hips_t *hips, const painter_t *painter)
{
    /*
     * Formula based on the fact that the number of pixels of the survey
     * covering a small angle 'a' is:
     * px1 = a * w * 4 * sqrt(2) * 2^order
     * with w the pixel width of a tile.
     *
     * We also know that the number of pixels on screen in the segment 'a' is:
     * px2 = a * f * win_h / 2
     *
     * solving px1 = px2 gives us the formula.
     */
    double w = hips->tile_width ?: 256;
    double win_h = painter->proj->window_size[1];
    double f = fabs(painter->proj->mat[1][1]);
    return round(log2(M_PI * f * win_h / (4.0 * sqrt(2.0) * w)));
}

int hips_get_render_order_planet(const hips_t *hips, const painter_t *painter,
                                 const double mat[4][4])
{
    /*
     * To come up with this formula, considering a small view angle 'a',
     * we know this map on screen to a pixel number:
     *
     *   px1 = a * f * winh / 2
     *
     * We also know this angle covers a segment of the planet of length:
     *
     *   x = (d - r) * a
     *
     * A planet meridian of length 2πr has '4 * sqrt(2) * w * 2^order' pixels,
     * so the segment x has:
     *
     *   px2 = 4 * sqrt(2) * w * 2^order / (2 pi r) * (d - r) * a
     *
     * Solving px1 = px2 gives us the formula.
     */
    double w = hips->tile_width ?: 256;
    double win_h = painter->proj->window_size[1];
    double f = painter->proj->mat[1][1];
    double r = vec3_norm(mat[0]);
    double d = vec3_norm(mat[3]);
    double order;
    order = log2(f * win_h * M_PI * r / (4.0 * sqrt(2.0) * w * (d - r)));
    // Note: I add 1 to make sure the planets look sharp.  Note sure why
    // this is needed (because of the interpolation?)
    return ceil(order + 1);
}

int hips_parse_hipslist(
        const char *data, void *user,
        int callback(void *user, const char *url, double release_date))
{
    int len, nb = 0;
    char *line, *hips_service_url = NULL, *key, *value, *tmp = NULL;
    const char *end;
    double hips_release_date = 0;

    assert(data);
    while (*data) {
        end = strchr(data, '\n') ?: data + strlen(data);
        len = end - data;
        asprintf(&line, "%.*s", len, data);

        if (*line == '\0' || *line == '#') goto next;
        key = strtok_r(line, "= ", &tmp);
        value = strtok_r(NULL, "= ", &tmp);
        if (strcmp(key, "hips_service_url") == 0) {
            free(hips_service_url);
            hips_service_url = strdup(value);
        }
        if (strcmp(key, "hips_release_date") == 0)
            hips_release_date = hips_parse_date(value);

next:
        free(line);
        data += len;
        if (*data) data++;

        // Next survey.
        if ((*data == '\0' || *data == '\n') && hips_service_url) {
            callback(user, hips_service_url, hips_release_date);
            free(hips_service_url);
            hips_service_url = NULL;
            hips_release_date = 0;
            nb++;
        }
    }
    return nb;
}

static int load_tile_worker(worker_t *worker)
{
    int transparency = 0;
    typeof(((tile_t*)0)->loader) loader = (void*)worker;
    tile_t *tile = loader->tile;
    hips_t *hips = tile->hips;
    tile->data = hips->settings.create_tile(
                    hips->settings.user, tile->pos.order, tile->pos.pix,
                    loader->data, loader->size, &loader->cost, &transparency);
    if (!tile->data) tile->flags |= TILE_LOAD_ERROR;
    tile->flags |= (transparency * TILE_NO_CHILD_0);
    free(loader->data);
    return 0;
}

static tile_t *hips_get_tile_(hips_t *hips, int order, int pix, int flags,
                              int *code)
{
    const void *data;
    int size, parent_code, asset_flags, cost = 0, transparency = 0;
    char url[URL_MAX_SIZE];
    tile_t *tile, *parent;
    tile_key_t key = {hips->hash, order, pix};

    assert(order >= 0);
    *code = 0;

    if (!g_cache) g_cache = cache_create(CACHE_SIZE);
    tile = cache_get(g_cache, &key, sizeof(key));

    // Got a tile but it is still loading.
    if (tile && tile->loader) {
        if (!worker_iter(&tile->loader->worker)) return NULL;
        cache_set_cost(g_cache, &key, sizeof(key), tile->loader->cost);
        free(tile->loader);
        tile->loader = NULL;
    }
    if (tile) {
        *code = 200;
        return tile;
    }
    if (flags & HIPS_CACHED_ONLY) return 0;

    if (!hips_is_ready(hips)) return NULL;
    // Don't bother looking for tile outside the hips order range.
    if ((hips->order && (order > hips->order)) || order < hips->order_min) {
        *code = 404;
        return NULL;
    }

    // Skip if we already know that this tile doesn't exists.
    if (order > hips->order_min) {
        parent = hips_get_tile_(hips, order - 1, pix / 4, flags, &parent_code);
        if (!parent) {
            *code = parent_code;
            return NULL; // Always get parent first.
        }
        if (parent->flags & (TILE_NO_CHILD_0 << (pix % 4))) {
            *code = 404;
            return NULL;
        }
    }
    get_url_for(hips, url, "Norder%d/Dir%d/Npix%d.%s",
                order, (pix / 10000) * 10000, pix, hips->ext);
    asset_flags = ASSET_ACCEPT_404;
    if (order > 0 && !(flags & HIPS_NO_DELAY))
        asset_flags |= ASSET_DELAY;
    data = asset_get_data2(url, asset_flags, &size, code);
    if (!(*code)) return NULL; // Still loading the file.

    // If the tile doesn't exists, mark it in the parent tile so that we
    // won't have to search for it again.
    if ((*code) / 100 == 4) {
        if (order > hips->order_min) {
            parent = hips_get_tile_(hips, order - 1, pix / 4, flags,
                                    &parent_code);
            if (parent) parent->flags |= (TILE_NO_CHILD_0 << (pix % 4));
        }
        return NULL;
    }

    // Anything else that doesn't return the data is an actual error.
    if (!data) {
        if (*code != 598) LOG_E("Cannot get url '%s' (%d)", url, *code);
        return NULL;
    }

    assert(hips->settings.create_tile);

    tile = calloc(1, sizeof(*tile));
    tile->pos.order = order;
    tile->pos.pix = pix;
    tile->hips = hips;
    hips->ref++;
    cache_add(g_cache, &key, sizeof(key), tile, sizeof(*tile) + cost,
              del_tile);

    if (!(flags & HIPS_LOAD_IN_THREAD)) {
        tile->data = hips->settings.create_tile(
                hips->settings.user, order, pix, data, size,
                &cost, &transparency);
        tile->flags |= (transparency * TILE_NO_CHILD_0);
        if (!tile->data) {
            LOG_W("Cannot parse tile %s", url);
            tile->flags |= TILE_LOAD_ERROR;
        }
        asset_release(url);
    } else {
        tile->loader = calloc(1, sizeof(*tile->loader));
        worker_init(&tile->loader->worker, load_tile_worker);
        tile->loader->data = malloc(size);
        tile->loader->size = size;
        tile->loader->tile = tile;
        memcpy(tile->loader->data, data, size);
        asset_release(url);
        *code = 0;
        return NULL;
    }
    return tile;
}

const void *hips_get_tile(hips_t *hips, int order, int pix, int flags,
                          int *code)
{
    tile_t *tile = hips_get_tile_(hips, order, pix, flags, code);
    if (*code == 0) assert(!tile);
    return tile ? tile->data : NULL;
}

/*
 * Default tile support for images surveys
 */
static const void *create_img_tile(
        void *user, int order, int pix, void *data, int size,
        int *cost, int *transparency)
{
    void *img;
    int i, w, h, bpp = 0;
    img_tile_t *tile;

    // Special case for allsky tiles!  Just return an empty image tile.
    if (order == -1) {
        tile = calloc(1, sizeof(*tile));
        return tile;
    }

    img = img_read_from_mem(data, size, &w, &h, &bpp);
    if (!img) {
        LOG_W("Cannot parse img");
        return NULL;
    }
    tile = calloc(1, sizeof(*tile));
    tile->img = img;
    tile->w = w;
    tile->h = h;
    tile->bpp = bpp;
    // Compute transparency.
    for (i = 0; i < 4; i++) {
        if (img_is_transparent(img, w, h, bpp,
                    (i / 2) * w / 2, (i % 2) * h / 2, w / 2, h / 2)) {
                *transparency |= 1 << i;
        }
    }
    *cost = w * h * bpp;
    return tile;
}

static int delete_img_tile(void *tile_)
{
    img_tile_t *tile = tile_;
    texture_release(tile->tex);
    free(tile);
    return 0;
}

/*
 * Function: hips_parse_date
 * Parse a date in the format supported for HiPS property files
 *
 * Parameters:
 *   str    - A date string (like 2019-01-02T15:27Z)
 *
 * Returns:
 *   The time in MJD, or 0 in case of error.
 */
double hips_parse_date(const char *str)
{
    int iy, im, id, ihr, imn;
    double d1, d2;
    if (sscanf(str, "%d-%d-%dT%d:%dZ", &iy, &im, &id, &ihr, &imn) != 5)
        return 0;
    eraDtf2d("UTC", iy, im, id, ihr, imn, 0, &d1, &d2);
    return d1 - DJM0 + d2;
}
