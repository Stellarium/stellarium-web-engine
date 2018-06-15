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

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>


// DEFINE(NAME) returns 1 if NAME is defined to 1, 0 otherwise.
#define DEFINED(macro) DEFINED_(macro)
#define macrotest_1 ,
#define DEFINED_(value) DEFINED__(macrotest_##value)
#define DEFINED__(comma) DEFINED___(comma 1, 0)
#define DEFINED___(_, v, ...) v



#define ARRAY_SIZE(x) ((int)(sizeof(x) / sizeof((x)[0])))

// C lambda trick, to be used only when testing small hackish code.
// Used like this:
//      func(x, y, LAMBDA(void _(int x) { ... }));
#if DEBUG
    #define LAMBDA(f_) ({ f_ _; })
#endif

// IS_IN(x, ...): returns true if x is equal to any of the other arguments.
#define IS_IN(x, ...) ({ \
        bool _ret = false; \
        const typeof(x) _V[] = {__VA_ARGS__}; \
        int _i; \
        for (_i = 0; _i < (int)ARRAY_SIZE(_V); _i++) \
            if (x == _V[_i]) _ret = true; \
        _ret; \
    })

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
 * Function: is_clipped
 * Test if a shape in clipping coordinate is clipped or not.
 */
bool is_clipped(int n, double (*pos)[4]);

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
 * Function: move_toward
 * Move a value toward a target value.
 *
 * Return:
 *   True if the value changed.
 */
bool move_toward(double *x,
                 double target,
                 int easing, // For the moment need to be 0.
                 double speed,
                 double dt);

/*
 * Function: join_paths
 * Join two urls.  The returned path is only valid until the next call.
 */
const char *join_paths(const char *base, const char *path);

/*
 * Function binpack
 * Pack binary data.
 *
 * Supported formats are:
 *   i - 4 bytes signed integer.
 *   f - 4 bytes float (still require a double argument).
 */
int binpack(void *ptr, const char *format, ...);

/*
 * Function binunpack
 * Unpack binary data.
 *
 * This is using the same format as binpack.
 */
int binunpack(const void *ptr, const char *format, ...);

static inline bool str_equ(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

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
bool str_is_upper(const char *s);
void str_rstrip(char *s);
