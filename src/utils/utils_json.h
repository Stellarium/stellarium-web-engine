/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: utils_json.h
 * Some extra functions that extend the json lib we use.
 */

#include <stdbool.h>

#include "json.h"
#include "json-builder.h"

// Convenience function to get an attribute out of a json object.
json_value *json_get_attr(json_value *val, const char *attr, int type);
const char *json_get_attr_s(json_value *val, const char *attr);
double json_get_attr_f(json_value *val, const char *attr, double default_val);
int64_t json_get_attr_i(json_value *val, const char *attr,
                        int64_t default_value);
bool json_get_attr_b(json_value *val, const char *attr, bool default_value);

/*
 * Function: json_to_string
 * Interpret a json value as a string.
 *
 * If the value is a list, returns the concatenation of all the lines with
 * spaces added.  Return a new allocated string, or NULL in case of error.
 */
char *json_to_string(json_value *val);

/*
 * Function: json_copy
 * Make a deep copy of a json value
 */
json_value *json_copy(json_value *val);

/*
 * Special interface to parse json document using a syntax similar to bson
 * C Object notation.
 *
 * e.g:
 *
 * float x;
 * int y, z;
 * const char *str;
 *
 * jcon_parse(json, "{",
 *    "attr1", "{",
 *        "x",      JCON_FLOAT(x),
 *        "y",      JCON_INT(y),
 *        "s",      JCON_STR(str),
 *        "!z",     JCON_INT(z), // Non optional.
 *    "}",
 *    "list", "[", JCON_FLOAT(y), JCON_FLOAT(z), "]"
 * "}");
 *
 * By default all dict attribute are optional: if the key is not present
 * we set the value to NULL/zero.  We can make a key required by adding a '!'
 * before the key.
 */

#define JCON_NEW(...) jcon_new(0, __VA_ARGS__)
#define JCON_INT(v) "i", &(v)
#define JCON_FLOAT(v) "f", &(v)
#define JCON_DOUBLE(v) "d", &(v)
#define JCON_STR(v) "s", &(v)
#define JCON_VAL(v) "v", &(v)

int jcon_parse(json_value *v, ...);
