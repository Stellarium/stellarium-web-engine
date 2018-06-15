/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "catalog.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static bool test_format(const char *format, const char *s)
{
    int i;
    if (format[0] == 'I' || format[0] == 'F') {
        for (i = 0; i < strlen(s); i++) {
            if (s[i] == ' ') continue;
            if (s[i] == '.') continue;
            if (s[i] == '-' || s[i] == '+') continue;
            if (s[i] >= '0' && s[i] <= '9') continue;
            return false;
        }
    }
    return true;
}

static int parse_field(const catalog_t *f, const char *line, void *v)
{
    int size, i;
    char buff[64];

    if (!v) return 0;
    size = f->end - f->start + 1;
    if (f->format[0] == 'A') {
        memcpy(v, line + f->start - 1, size);
        return 0;
    }
    assert(size < sizeof(buff));
    memcpy(buff, line + f->start - 1, size);
    buff[size] = '\0';

    // Check if there is no value.
    for (i = 0; i < size; i++)
        if (buff[i] != ' ') break;
    if (i == size) {
        if (f->optional != '?') return 1;
        switch (f->format[0]) {
        case 'I':
            *((int*)v) = f->default_i;
            break;
        case 'Z':
            *((int*)v) = f->default_i;
            break;
        case 'F':
            *((double*)v) = f->default_f;
            break;
        }
        return 0;
    }

    if (!test_format(f->format, buff)) return -1;
    switch (f->format[0]) {
    case 'I':
        *((int*)v) = atoi(buff);
        break;
    case 'Z':
        *((int*)v) = strtol(buff, NULL, 16);
        break;
    case 'F':
        *((double*)v) = atof(buff);
        break;
    }
    return 0;
}

int catalog_parse_line(const catalog_t *cat, const char *line, int i, ...)
{
    int err = 0;
    va_list ap;
    void *v;

    va_start(ap, i);
    for (; cat->start; cat++) {
        v = va_arg(ap, void*);
        err = parse_field(cat, line, v);
        if (err) break;
    }
    va_end(ap);
    if (err) LOG_W("Cannot parse line %d: (field %s)", i + 1, cat->name);
    return err;
}

static int catalog_test_line(const catalog_t *cat, const char *line)
{
    int err = 0;
    char d[128];
    for (; cat->start; cat++) {
        err = parse_field(cat, line, d);
        if (err) break;
    }
    return err;
}

bool catalog_match(const catalog_t *cat, const char *data)
{
    int r = catalog_test_line(cat, data);
    return r == 0;
}

// XXX: move this somewhere else?
#if 0 // COMPILE_TESTS

static void test_catalog_simple(void)
{
    catalog_t cat1[] = {
        {1, 2, "I2", "x"},
        {4, 5, "I2", "y"},
        {7, 8, "I2", "z"},
        {0},
    };
    catalog_t cat2[] = {
        {1, 2, "I2",   "x"},
        {4, 7, "F4.1", "f"},
        {0}
    };
    int x, y, z;
    double f;
    catalog_parse_line(cat1, "10 20 30", 0, &x, &y, &z);
    assert(x == 10 && y == 20 && z == 30);

    catalog_parse_line(cat2, "10 31.2", 0, &x, &f);
    assert(x == 10 && f == 31.2);
}

static void test_catalog_full(void)
{
    catalog_t cat[] = {
        {1, 2, "I2", "x"},
        {4, 5, "I2", "y"},
        {7, 8, "I2", "z"},
        {0},
    };
    int i, x, y, z, sum = 0;
    const char *data = "10 20 30\n"
                       "40 50 60\n";
    CATALOG_ITER(cat, data, i, &x, &y, &z) {
        sum += x + y + z;
    }
    assert(sum == 210);
}

TEST_REGISTER(NULL, test_catalog_simple, 0);
TEST_REGISTER(NULL, test_catalog_full, 0);

#endif
