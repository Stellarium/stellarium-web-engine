/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef HIPS_H
#define HIPS_H

#include "painter.h"
#include "utils/worker.h"

/*
 * File: hips.h
 */

/*
 * Type: hips_t
 * Opaque structure representing a hips survey.
 */
typedef struct hips hips_t;

enum {
    HIPS_FORCE_USE_ALLSKY       = 1 << 1,
    HIPS_LOAD_IN_THREAD         = 1 << 2,
    HIPS_CACHED_ONLY            = 1 << 3,
    // If set in hips_get_tile, do not add a small delay before starting
    // the downloads.  By default we use a small delay of about one sec
    // per tile.
    HIPS_NO_DELAY               = 1 << 4,
};

/*
 * Type: hips_settings
 * Structure passed to hips_create for custom type surveys.
 *
 * Attributes:
 *   create_tile - function used to convert source data into a tile.
 *                 The returned pointer is handled by the hips survey, and
 *                 can be anything.  This is called every time the survey
 *                 load a tile that is not in the cache.  See note [1]
 *   delete_tile - function used to delete the data returned by create_tile.
 *   user        - pointer passed to create_tile.
 *
 * Note 1:
 *   The create_tile function needs to return a cost value (in bytes) for the
 *   cache, and if we know that some children tiles don't need to be loaded, we
 *   can set the transparency value, as a four bits bitmask, one bit per child.
 */
typedef struct hips_settings {
    const void *(*create_tile)(void *user, int order, int pix, void *data,
                               int size, int *cost, int *transparency);
    int (*delete_tile)(void *tile);
    const char *ext; // If set, force the files extension.
    void *user;
} hips_settings_t;


struct hips {
    char        *url;
    char        *service_url;
    const char  *ext; // jpg, png, webp.
    double      release_date; // release date as jd value.
    int         error; // Set if an error occurred.
    char        *label; // Short label used in the progressbar.
    int         frame; // FRAME_ICRF | FRAME_ASTROM | FRAME_OBSERVED.
    uint32_t    hash; // Hash of the url.

    // Stores the allsky image if available.
    // We only do it for order zero allsky.
    struct {
        worker_t    worker; // Worker to load the image in a thread.
        bool        not_available;
        uint8_t     *src_data; // Encoded image data (png, webp...)
        uint8_t     *data;     // RGB[A] image data.
        int         w, h, bpp, size;
        texture_t   *textures[12];
    }           allsky;

    // Contains all the properties as a json object.
    json_value *properties;
    int order;
    int order_min;
    int tile_width;

    // The settings as passed in the create function.
    hips_settings_t settings;
    int ref; // Ref counting of hips survey.
};


/*
 * Function: hips_create
 * Create a new hips survey.
 *
 * Parameters:
 *   url          - URL to the root of the survey.
 *   release_date - If known, release date in utc.  Otherwise 0.
 */
hips_t *hips_create(const char *url, double release_date,
                    const hips_settings_t *settings);

/*
 * Function: hips_delete
 * Delete a hips and all associated memory
 */
void hips_delete(hips_t *hips);

/*
 * Function: hips_get_tile
 * Get a given tile of a hips survey.
 *
 * This only makes sense for custom type surveys (for images we can directly
 * use <hips_get_tile_texture>).
 *
 * Parameters:
 *   hips   - a hips survey.
 *   order  - order of the tile.
 *   pix    - pix of the tile.
 *   flags  - unused for the moment.
 *   code   - get the return code of the tile loading.
 *
 * Return:
 *   The value previously returned by the settings create_tile function.
 *   If the tile data is not available yet set code to 0 and return NULL.
 *   In case of error, set code to the error code and return NULL.
 */
const void *hips_get_tile(hips_t *hips, int order, int pix, int flags,
                          int *code);

/*
 * Function: hips_is_ready
 * Check if a hips survey is ready to use
 *
 * The return true if:
 * - the property file has been parsed.
 * - the allsky image has been loaded (if there is one).
 */
bool hips_is_ready(hips_t *hips);

// Same as hips_is_ready.
bool hips_update(hips_t *hips);

/*
 * Function: hips_traverse
 * Breadth first traversal of healpix grid.
 *
 * Deprecated, better to use the non callback version with the hips_iter_
 * functions instead.
 *
 * The callback should return:
 *   1 to keep going deeper into the tile.
 *   0 to stop iterating inside this tile.
 *   <0 to immediately return (with the same value).
 *
 * Return:
 *   0 if the traverse finished.
 *   -1 if we reach the traverse limite.
 *   -v if the callback returned a negative value -v.
 */
int hips_traverse(void *user, int callback(int order, int pix, void *user));


/*
 * Struct: hips_iterator_t
 * Used for breadth first traversal of hips.
 *
 * To iter a hips index we can use the <hips_iter_init>, <hips_iter_next>
 * and <hips_iter_push_children> functions.  e.g:
 *
 *  hips_iterator_t iter;
 *  int order, pix;
 *  hips_iter_init(&iter);
 *  while (hips_iter_next(&iter, &order, &pix)) {
 *      // Process healpix pixel at order, pix.
 *      if (go_deeper) {
 *         hips_iter_push_children(iter, order, pix);
 *      }
 *  }
 *
 */
typedef struct hips_iterator
{
    struct {
        int order;
        int pix;
    } queue[1024]; // Todo: make it dynamic?
    int size;
    int start;
} hips_iterator_t;

/*
 * Function: hips_iter_init
 * Initialize the iterator with the initial twelve order zero healpix pixels.
 */
void hips_iter_init(hips_iterator_t *iter);

/*
 * Function: hips_iter_next
 * Pop the next healpix pixel from the iterator.
 *
 * Return false if there are no more pixel enqueued.
 */
bool hips_iter_next(hips_iterator_t *iter, int *order, int *pix);

/*
 * Function: hips_iter_push_children
 * Add the four children of the giver pixel to the iterator.
 *
 * The children will be retrieved after all the currently queued values
 * from the iterator have been processed.
 */
void hips_iter_push_children(hips_iterator_t *iter, int order, int pix);

/*
 * Function: hips_get_tile_texture
 * Get the texture for a given hips tile.
 *
 * This should return the most appropriate texture, no matter if the actual
 * tile exists.  It tries to use a parent texture, or the allsky as
 * fallback.
 *
 * The algorithm is more or less:
 *   - If the tile is loaded, return its texture.
 *   - If not, or if the order is higher than the survey max order,
 *     try to use a parent tile as a fallback.
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
        double transf[3][3], double *fade, bool *loading_complete);

/*
 * Function: hips_parse_hipslist
 * Parse a hipslist file.
 *
 * Return:
 *    >= 0 if list successfully parsed, number of parsed entries.
 *    < 0 if error.
 */
int hips_parse_hipslist(
        const char *data, void *user,
        int callback(void *user, const char *url, double release_date));

/*
 * Function: hips_set_frame
 * Set the frame of a hips survey (if it is not specified in the properties).
 *
 * Parameters:
 *   hips  - A hips survey.
 *   frame - FRAME_ICRF or FRAME_OBSERVED.
 */
void hips_set_frame(hips_t *hips, int frame);

/*
 * Function: hips_set_label
 * Set the label for a hips survey
 *
 * Parameters:
 *   hips  - A hips survey.
 *   label - The new label to use. It will override existing labels
 *           such as the ones taken from the properties file.
 */
void hips_set_label(hips_t *hips, const char* label);

/*
 * Function: hips_get_render_order
 * Return the max order at which a survey will be rendered.
 *
 * Parameters:
 *   hips    - A hips survey.
 *   painter - The painter used to render.
 */
int hips_get_render_order(const hips_t *hips, const painter_t *painter);

/*
 * Function: hips_get_render_order
 * Return the max order at which a planet survey will be rendered.
 *
 * Parameters:
 *   hips    - A hips survey.
 *   painter - The painter used to render.
 *   mat     - 4x4 transformation matrix of the planet position / scale.
 */
int hips_get_render_order_planet(const hips_t *hips, const painter_t *painter,
                                 const double mat[4][4]);

/*
 * Function: hips_render
 * Render a hips survey.
 *
 * Parameters:
 *   hips    - A hips survey.
 *   painter - The painter used to render.
 *   transf  - Transformation applied to the unit sphere to set the position
 *             in the sky.  Can be set to NULL for identity.
 *   split_order - The requested order of the final quad divisions.
 *                 The actual split order could be higher if the rendering
 *                 order is too high for this value.
 */
int hips_render(hips_t *hips, const painter_t *painter,
                const double transf[4][4], int split_order);

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
double hips_parse_date(const char *str);

#endif // HIPS_H
