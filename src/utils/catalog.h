/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: catalog.h
 * Some util functions to parse catalog files in fortran format.
 */

#include <stdbool.h>

typedef struct
{
    int     start, end;
    char    format[8];
    char    name[16];
    char    optional;   // Set to '?' if optional.
    union {
        int     default_i;
        double  default_f;
    };
} catalog_t;

int catalog_parse_line(const catalog_t *cat, const char *line, int i, ...);
void *catalog_parse(const catalog_t *cat, const char *data);
// Test if some data matches a catalog.
bool catalog_match(const catalog_t *cat, const char *data);

#define CATALOG_ITER(cat, data, i, ...) \
    if (i = 0, true) \
    for (const char *l_ = data; *l_; l_ = strchr(l_, '\n') + 1, i++) \
        if (catalog_parse_line(cat, l_, i, __VA_ARGS__) == 0)
