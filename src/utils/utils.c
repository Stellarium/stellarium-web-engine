/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "utils.h"
#include "utstring.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

double unix_to_mjd(double t)
{
    return t / 86400.0 + 2440587.5 - 2400000.5;
}

void *read_file(const char *path, int *size)
{
    FILE *file;
    char *ret;
    int read_size __attribute__((unused));
    int size_default;

    size = size ?: &size_default; // Allow to pass NULL as size;
    file = fopen(path, "rb");
    if (!file) return NULL;
    assert(file);
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    ret[*size] = '\0';
    fclose(file);
    return ret;
}

// Test if a shape in clipping coordinate is clipped or not.
bool is_clipped(int n, double (*pos)[4])
{
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    int i, p;
    for (p = 0; p < 6; p++) {
        for (i = 0; i < n; i++) {
            if (    P[p][0] * pos[i][0] +
                    P[p][1] * pos[i][1] +
                    P[p][2] * pos[i][2] +
                    P[p][3] * pos[i][3] <= 0) {
                break;
            }
        }
        if (i == n) // All the points are outside a clipping plane.
            return true;
    }
    return false;
}

uint8_t *img_read(const char *path, int *w, int *h, int *bpp)
{
    void *data;
    uint8_t *ret;
    int size;
    data = read_file(path, &size);
    ret = img_read_from_mem(data, size, w, h, bpp);
    free(data);
    return ret;
}

uint8_t *img_read_from_mem(const void *data, int size,
                           int *w, int *h, int *bpp)
{
    return stbi_load_from_memory(data, size, w, h, bpp, *bpp);
}

void img_write(const uint8_t *img, int w, int h, int bpp, const char *path)
{
    stbi_write_png(path, w, h, bpp, img, 0);
}

int z_uncompress(void *dest, int dest_size, const void *src, int src_size)
{
    stbi_zlib_decode_buffer(dest, dest_size, src, src_size);
    return 0;
}

// Uncompress gz file data.
// Only used for the star source data.
void *z_uncompress_gz(const void *src, int src_size, int *out_size)
{
    int isize;
    const uint8_t *d = src;
    void *ret;
    uint8_t footer[8];
    assert(src_size >= 10);
    assert(*d++ == 0x1f);
    assert(*d++ == 0x8b);
    assert(*d++ == 8); // Only support deflate.
    assert(*d++ == 8); // Only support this flags for the moment!
    d += 6;
    d += strlen((const char*)d) + 1;
    // XXX: this might break with js!
    memcpy(footer, (src + src_size - 8), 8);
    isize = *((uint32_t*)(footer + 4));
    *out_size = isize;
    ret = malloc(isize + 1);
    stbi_zlib_decode_noheader_buffer(
            ret, isize, (void*)d, src_size - 8 - ((void*)d - src));
    ((char*)ret)[isize] = '\0';
    return ret;
}

// Move a value toward a target value.
// Return true if the value changed.
bool move_toward(double *x,
                 double target,
                 int easing, // For the moment need to be 0.
                 double speed,
                 double dt)
{
    double d = speed * dt;
    assert(easing == 0);
    if (*x == target) return false;
    if (fabs(*x - target) <= d) {
        *x = target;
    } else {
        *x += d * cmp(target, *x);
    }
    return true;
}

const char *join_paths(const char *base, const char *path)
{
    const int BUFF_SIZE = 256;
    static char *ret = NULL;
    if (!base) return path;
    if (!ret) ret = calloc(BUFF_SIZE, 1);
    snprintf(ret, BUFF_SIZE, "%s/%s", base, path);
    return ret;
}

// Pack / unpack binary data.
// Supported formats are:
//      i:  4 bytes signed integer.
//      f:  4 bytes float.
//      Q:  8 bytes unsigned uint64_t
int binpack(void *ptr, const char *format, ...)
{
    va_list ap;
    char f;
    int n;
    union {
        char       *s;
        int         i;
        float       f;
    } v;
    va_start(ap, format);
    while ((f = *format++)) {
        switch (f) {
        case 'i':
            v.i = va_arg(ap, int);
            memcpy(ptr, &v.i, 4);
            ptr += 4;
            break;
        case 'f':
            v.f = va_arg(ap, double);
            memcpy(ptr, &v.f, 4);
            ptr += 4;
            break;
        case 's':
            n = va_arg(ap, int);
            v.s = va_arg(ap, char*);
            memcpy(ptr, v.s, n);
            ptr += n;
            break;
        default:
            assert(false);
        }
    }
    va_end(ap);
    return 0;
}

int binunpack(const void *ptr, const char *format, ...)
{
    va_list ap;
    char f;
    int n;
    union {
        char    *s;
        int      i;
        float    f;
        uint64_t q;
    } v;
    va_start(ap, format);
    while ((f = *format++)) {
        switch (f) {
        case 'i':
            memcpy(&v.i, ptr, 4);
            *va_arg(ap, int*) = v.i;
            ptr += 4;
            break;
        case 'f':
            memcpy(&v.f, ptr, 4);
            *va_arg(ap, double*) = v.f;
            ptr += 4;
            break;
        case 'Q':
            memcpy(&v.q, ptr, 8);
            *va_arg(ap, uint64_t*) = v.q;
            ptr += 8;
            break;
        case 's':
            n = va_arg(ap, int);
            memcpy(va_arg(ap, char*), ptr, n);
            ptr += n;
            break;
        }
    }
    va_end(ap);
    return 0;
}

bool str_endswith(const char *str, const char *end)
{
    if (!str || !end) return false;
    if (strlen(str) < strlen(end)) return false;
    const char *start = str + strlen(str) - strlen(end);
    return strcmp(start, end) == 0;
}

void str_to_upper(const char *s, char *out)
{
    char c;
    while ((c = *s++)) {
        if (c >= 'a' && c <= 'z') c += 'A' - 'a';
        *out++ = c;
    }
    *out = '\0';
}

bool str_is_upper(const char *s)
{
    char c;
    while ((c = *s++)) {
        if (c >= 'a' && c <= 'z') return false;
    }
    return true;
}

void str_rstrip(char *s)
{
    int i;
    for (i = strlen(s) - 1; i >= 0; i--) {
        if (s[i] != ' ') return;
        s[i] = '\0';
    }
}
