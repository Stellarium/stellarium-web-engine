/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


#include "font.h"
#include "utf8.h"

// Until stb_truetype bundled with imgui fix the warnings.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

static stbtt_fontinfo font;

static int max(int x, int y) { return x > y ? x : y; }
static int min(int x, int y) { return x < y ? x : y; }

void font_init(const void *font_data)
{
    int r;
    (void)r;
    r = stbtt_InitFont(&font, (unsigned char*)font_data, 0);
    assert(r);
}

static void pixel_combine(uint8_t *dst, int color, int alpha)
{
    dst[0] = max(dst[1], alpha);
}

static void blit_buffer(uint8_t *img, int img_w, int img_h,
                        const uint8_t *buff, int buff_w, int buff_h,
                        int x, int y, int w, int h)
{
    int i, j;
    assert(x >= 0);
    assert(y >= 0);
    assert(x + w < img_w);
    assert(y + h < img_h);
    for (i = 0; i < h; i++) for (j = 0; j < w; j++) {
        pixel_combine(&img[(img_h - (y + i + 1)) * img_w + x + j],
                      255, buff[i * buff_w + j]);
    }
}

uint8_t *font_render(const char *text, float height, int *w, int *h)
{
    int code;
    int advance, lsb, x0, y0, x1, y1;
    float xpos = 0, ypos = 0, xshift, yshift;
    int ascent, descent, linegap;
    const char *text_ptr;
    int xmin = +32000, xmax = -32000, ymin = +32000, ymax = -32000;
    int buff_w = 0, buff_h = 0;
    uint8_t *image, *buff;
    int line_height;
    const float scale = stbtt_ScaleForPixelHeight(&font, height);

    stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
    line_height = (ascent - descent + linegap) * scale;

    // Compute the out image bbox and the max char buffer size.
    for (text_ptr = text; *text_ptr; text_ptr += u8_char_len(text_ptr)) {
        if (*text_ptr == '\n') {
            ypos += line_height;
            xpos = 0;
            continue;
        }
        xshift = xpos - floor(xpos);
        yshift = ypos - floor(ypos);
        code = u8_char_code(text_ptr);
        stbtt_GetCodepointHMetrics(&font, code, &advance, &lsb);
        stbtt_GetCodepointBitmapBoxSubpixel(&font, code, scale, scale,
                                    xshift, yshift, &x0, &y0, &x1, &y1);
        xmin = min(xmin, xpos + x0);
        xmax = max(xmax, xpos + x1);
        ymin = min(ymin, ypos + y0);
        ymax = max(ymax, ypos + y1);
        buff_w = max(buff_w, x1 - x0);
        buff_h = max(buff_h, y1 - y0);
        xpos += advance * scale;
    }
    assert(buff_w > 0 && buff_h > 0);
    if (buff_w == 0 || buff_h == 0) return NULL;
    // Create the image and the rendering buffer.
    buff = calloc(1, buff_w * buff_h);
    *w = xmax - xmin + 1;
    *h = ymax - ymin + 1;
    image = calloc(1, (*w) * (*h));

    // Render all the characters
    xpos = 0;
    ypos = 0;
    for (text_ptr = text; *text_ptr; text_ptr += u8_char_len(text_ptr)) {
        if (*text_ptr == '\n') {
            ypos += line_height;
            xpos = 0;
            continue;
        }
        xshift = xpos - floor(xpos);
        yshift = ypos - floor(ypos);
        code = u8_char_code(text_ptr);
        stbtt_GetCodepointHMetrics(&font, code, &advance, &lsb);
        stbtt_GetCodepointBitmapBoxSubpixel(&font, code, scale, scale,
                                    xshift, yshift, &x0, &y0, &x1, &y1);
        stbtt_MakeCodepointBitmapSubpixel(&font, buff, buff_w, buff_h, buff_w,
                                  scale, scale, xshift, yshift, code);
        blit_buffer(image, *w, *h, buff, buff_w, buff_h,
                    xpos + x0 - xmin, ypos + y0 - ymin,
                    x1 - x0, y1 - y0);
        xpos += advance * scale;
    }
    free(buff);
    return image;
}
