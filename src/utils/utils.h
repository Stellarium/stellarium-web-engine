/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * File: utils.h
 * Some general utility functions.
 *
 * We put here all the general functions and macros that are not specific to
 * this project, without any external dependencies, and crossplatform.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>


// DEFINE(NAME) returns 1 if NAME is defined to 1, 0 otherwise.
#define DEFINED(macro) DEFINED_(macro)
#define macrotest_1 ,
#define DEFINED_(value) DEFINED__(macrotest_##value)
#define DEFINED__(comma) DEFINED___(comma 1, 0)
#define DEFINED___(_, v, ...) v



#define ARRAY_SIZE(x) ((int)(sizeof(x) / sizeof((x)[0])))

#define min(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; \
      })

#define max(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a > _b ? _a : _b; \
      })

#define clamp(x, a, b) (min(max(x, a), b))

#define cmp(a, b) ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    (_a > _b) ? +1 : (_a < _b) ? -1 : 0; \
})

// Can be used to pass array as `void *user` argument in callback functions.
#define USER_PASS(...) ((void*[]){__VA_ARGS__})
#define USER_GET(var, n) (((void**)var)[n])

static inline double mix(double x, double y, double t)
{
    return x * (1.0 - t) + y * t;
}

static inline double smoothstep(double edge0, double edge1, double x)
{
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

// Code inspired from https://stackoverflow.com/questions/2249110/
static inline int swe_isnan(double x)
{
    union { uint64_t u; double f; } ieee754;
    ieee754.f = x;
    return ( (unsigned)(ieee754.u >> 32) & 0x7fffffff ) +
           ( (unsigned)ieee754.u != 0 ) > 0x7ff00000;
}
#undef isnan
#define isnan(x) swe_isnan(x)

#undef isinf
#define isinf(x) static_assert(0, \
    "Please don't rely on isinf, we build with -ffast-math")

/*
 * Function: unix_to_mjd
 * Convert a time in unix time to MJD time.
 */
double unix_to_mjd(double t);

/*
 * Function: read_file
 * Read a local file (blocking).
 */
void *read_file(const char *path, int *size);

/*
 * Function: img_read
 * Read a png/jpeg image from a file.
 */
uint8_t *img_read(const char *path, int *w, int *h, int *bpp);

/*
 * Function: img_read_from_mem
 * Read a png/jpeg image from memory.
 */
uint8_t *img_read_from_mem(const void *data, int size,
                           int *w, int *h, int *bpp);

/*
 * Function: img_write
 * Write an image to file.
 */
void img_write(const uint8_t *img, int w, int h, int bpp, const char *path);

/*
 * Function: z_uncompress
 * Like zlib uncompress, but using stb instead.
 */
int z_uncompress(void *dest, int dest_size, const void *src, int src_size);

/*
 * Function: z_uncompress_gz
 * uncompress gz data.
 */
void *z_uncompress_gz(const void *src, int src_size, int *out_size);

/*
 * Function: str_startswith
 * Test is a string starts with an other one.
 */
static inline bool str_startswith(const char *str, const char *s)
{
    return strncmp(str, s, strlen(s)) == 0;
}

bool str_endswith(const char *str, const char *s);
void str_to_upper(const char *str, char *out);


/*
 * Function: iter_lines
 * Iter all the lines in a string
 *
 * This supports all the corner cases, like data not ending with a newline
 * or not null terminated data.
 *
 * Parameters:
 *   data       - Pointer to a string.
 *   size       - Size of data.
 *   line_ptr   - Pointer to the current line, will be set to the next line.
 *                Should be NULL at the first call.
 *   len_ptr    - Pointer to the current line length, will be set the next
 *                line length.
 *
 * Return:
 *   true as long as there are lines in the data.
 */
bool iter_lines(const char *data, int size,
                const char **line_ptr, int *len_ptr);
