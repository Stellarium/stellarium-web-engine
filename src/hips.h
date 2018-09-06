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

/*
 * File: hips.h
 */


/*
 * Type: hips_t
 * Opaque structure representing a hips survey.
 */
typedef struct hips hips_t;

enum {
    HIPS_EXTERIOR               = 1 << 0,
    HIPS_FORCE_USE_ALLSKY       = 1 << 1,
};

typedef struct hips_settings {
    const void *(*create_tile)(int order, int pix, void *src,
                               int size, int *cost);
} hips_settings_t;

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
 * Function: hips_set_frame
 * Set the frame of a hips survey (if it is not specified in the properties).
 *
 * Parameters:
 *   hips  - A hips survey.
 *   frame - FRAME_ICRS or FRAME_OBSERVED.
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

const void *hips_get_tile(hips_t *hips, int order, int pix, int flags,
                          int *code);

/*
 * Function: hips_render
 * Render a hips survey.
 *
 * Parameters:
 *   hips    - A hips survey.
 *   painter - The painter used to render.
 *   angle   - Visible angle the survey has in the sky.
 *             (2 * PI for full sky surveys).
 */
int hips_render(hips_t *hips, const painter_t *painter, double angle);

/*
 * Function: hips_render_traverse
 * Similar to hips_render, but instead of actually rendering the tiles
 *  we call a callback function.  This can be used when we need better
 *  control on the rendering.
 */
int hips_render_traverse(hips_t *hips, const painter_t *painter,
                         double angle, void *user,
                         int callback(hips_t *hips, const painter_t *painter,
                                      int order, int pix, int flags,
                                      void *user));

/*
 * Function: hips_get_tile_texture
 * Get the texture for a given hips tile.
 *
 * Parameters:
 *   flags   - <HIPS_FLAGS> union.
 *   uv      - The uv coordinates of the texture.
 *   proj    - An heapix projector already setup for the tile.
 *   split   - Recommended spliting of the texture when we render it.
 *   fade    - Recommended fade alpha.
 *   loading_complete - set to true if the tile is totally loaded.
 *
 * Return:
 *   The texture_t, or NULL if none is found.
 */
texture_t *hips_get_tile_texture(
        hips_t *hips, int order, int pix, int flags,
        const painter_t *painter,
        double uv[4][2], projection_t *proj, int *split, double *fade,
        bool *loading_complete);


bool hips_is_ready(hips_t *hips);


/*
 * Function: hips_parse_hipslist
 * Parse a hipslist file.
 *
 * Return:
 *    >= 0 if list successfully parsed, number of parsed entries.
 *    -1   if still loading.
 *    < -1 if error.
 */
int hips_parse_hipslist(const char *url,
                        int callback(const char *url, double release_date,
                                     void *user),
                        void *user);

/*
 * Function: hips_traverse
 * Depth first traversal of healpix grid.
 *
 * The callback should return:
 *   1 to keep going deeper into the tile.
 *   0 to stop iterating inside this tile.
 *   <0 to immediately return (with the same value).
 *
 * Return:
 *   0 if the traverse finished.
 *   -v if the callback returned a negative value -v.
 */
int hips_traverse(void *user, int callback(int order, int pix, void *user));

#endif // HIPS_H
