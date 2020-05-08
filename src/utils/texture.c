/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "texture.h"
#include "gl.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static struct {
    void *user;
    uint8_t *(*load)(void *user, const char *url, int *code,
                     int *w, int *h, int *bpp);
} g_callback = {};

static inline bool is_pow2(int n) {return (n & (n - 1)) == 0;}
static inline int next_pow2(int x) {return pow(2, ceil(log(x) / log(2)));}


static void blit(const uint8_t *src, int src_w, int src_h, int bpp,
                 uint8_t *dst, int dst_w, int dst_h,
                 int x, int y, int w, int h)
{
    int i, j;
    for (i = 0; i < h; i++)
    for (j = 0; j < w; j++) {
        memcpy(&dst[(i * dst_w + j) * bpp],
               &src[((i + y) * src_w + (j + x)) * bpp],
               bpp);
    }
}

void texture_set_load_callback(void *user,
        uint8_t *(*load)(void *user, const char *url, int *code,
                         int *w, int *h, int *bpp))
{
    g_callback.user = user;
    g_callback.load = load;
}

void texture_set_data(texture_t *tex, const void *data, int w, int h, int bpp)
{
    uint8_t *buff0 = NULL;
    int data_type = GL_UNSIGNED_BYTE;
    assert(tex->id);

    tex->w = w;
    tex->h = h;
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->format = (int[]){
        0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA
    }[bpp];
    assert(tex->format);

    if (!is_pow2(w) || !is_pow2(h)) {
        buff0 = calloc(bpp, tex->tex_w * tex->tex_h);
        blit(data, w, h, bpp, buff0, tex->tex_w, tex->tex_h, 0, 0, w, h);
        data = buff0;
    }
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->id));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            (tex->flags & TF_MIPMAP)? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->tex_w, tex->tex_h,
                0, tex->format, data_type, data));
    free(buff0);

    if (tex->flags & TF_MIPMAP)
        GL(glGenerateMipmap(GL_TEXTURE_2D));
}

texture_t *texture_create(int w, int h, int bpp)
{
    texture_t *tex;
    tex = calloc(1, sizeof(*tex));
    tex->ref = 1;
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->format = (int[]){0, 0, 0, GL_RGB, GL_RGBA}[bpp];
    GL(glGenTextures(1, &tex->id));
    return tex;
}

void texture_release(texture_t *tex)
{
    if (!tex) return;
    tex->ref--;
    if (tex->ref) return;
    free(tex->url);
    if (tex->id) GL(glDeleteTextures(1, &tex->id));
    free(tex);
}

texture_t *texture_from_data(const void *data, int img_w, int img_h, int bpp,
                             int x, int y, int w, int h, int flags)
{
    texture_t *tex;
    uint8_t *img;

    assert(x >= 0 && x + w <= img_w && y >= 0 && y + h <= img_h);
    tex = calloc(1, sizeof(*tex));
    tex->ref = 1;
    tex->flags = flags;
    GL(glGenTextures(1, &tex->id));

    if (x != 0 || y != 0 || w != img_w || h != img_h) {
        img = calloc(w * h, bpp);
        blit(data, img_w, img_h, bpp, img, w, h, x, y, w, h);
        texture_set_data(tex, img, w, h, bpp);
        free(img);
    } else {
        texture_set_data(tex, data, w, h, bpp);
    }

    return tex;
}

texture_t *texture_from_url(const char *url, int flags)
{
    texture_t *tex;
    tex = calloc(1, sizeof(*tex));
    tex->ref = 1;
    tex->url = strdup(url);
    tex->flags = flags;
    if (!(flags & TF_LAZY_LOAD)) texture_load(tex, NULL);
    return tex;
}

bool texture_load(texture_t *tex, int *code)
{
    int w, h, bpp = 0;
    void *img;
    if (tex->id) return true;
    assert(tex->url);
    assert(g_callback.load);
    img = g_callback.load(g_callback.user, tex->url, code, &w, &h, &bpp);
    if (!img) return false;
    GL(glGenTextures(1, &tex->id));
    texture_set_data(tex, img, w, h, bpp);
    free(img);
    return true;
}
