/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdbool.h>
#include <stdint.h>

enum {
    TF_MIPMAP           = 1 << 0,
    TF_LAZY_LOAD        = 1 << 1
};

/*
 * Type: texture_t
 * Represent an OpenGL texture.
 *
 * Since a common case is to load a texture asynchronously from an url,
 * when we create a texture with <texture_from_url>, the actual data won't
 * be available immediately.  We need to call texture_load to check that the
 * texture is ready.  If we use asynchronous textures, the loading function
 * should be set with <texture_set_load_callback>.
 *
 * Attributes:
 *   id     - OpenGL texture id.
 *   ref    - For ref counting.
 *   w      - Width of the texture.
 *   h      - Height of the texture.
 *   tex_w  - Internal width of the texture.
 *   tex_h  - Internal height of the texture.
 *   format - OpenGL format.
 *   flags  - Configuration bit flags
 *   url    - For async texture: url source of the image.
 */
typedef struct texture {
    uint32_t        id;
    int             ref;
    int             w, h, tex_w, tex_h;
    int             format;
    int             flags;
    char            *url;
} texture_t;

/*
 * Function: texture_set_load_callback
 * Set the callback function that will be used for asynchronous textures.
 *
 * Parameters:
 *   user - User data data will be passed to the callback.  Can be NULL.
 *   load - The callback function.  The function takes an url as input and
 *          should return the image data or NULL if the image is not ready
 *          yet.  We also set the following values:
 *            code  - Http code that will be passed back by <texture_load>.
 *            w     - The width of the image.
 *            h     - The height of the image.
 *            bpp   - Number of bytes per pixel of the image (1, 2, 3 or 4).
 */
void texture_set_load_callback(void *user,
        uint8_t *(*load)(void *user, const char *url, int *code,
                         int *w, int *h, int *bpp));

texture_t *texture_create(int w, int h, int bpp);
texture_t *texture_from_data(const void *data, int img_w, int img_h, int bpp,
                             int x, int y, int w, int h, int flags);
texture_t *texture_from_url(const char *url, int flags);
bool texture_load(texture_t *tex, int *code);
void texture_set_data(texture_t *tex, const void *data, int w, int h, int bpp);
void texture_release(texture_t *tex);
