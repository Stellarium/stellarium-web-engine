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
json_value *json_get_attr(const json_value *val, const char *attr, int type);
const char *json_get_attr_s(const json_value *val, const char *attr);
double json_get_attr_f(const json_value *val, const char *attr,
                       double default_val);
int64_t json_get_attr_i(const json_value *val, const char *attr,
                        int64_t default_value);
bool json_get_attr_b(const json_value *val, const char *attr,
                     bool default_value);

/*
 * Function: json_copy
 * Make a deep copy of a json value
 */
json_value *json_copy(const json_value *val);

/*
 * Function: json_double_array_new
 * Create an array of double
 */
json_value *json_vector_new(int size, const double *values);

/*
 * Function: parse_float_array
 * Conveniance function to parse a json array of the form [x, y, ...]
 *
 * Parameters:
 *   data   - A json array.
 *   size   - Number of double values to parse.
 *   out    - Pointer to a double array large enough to get all the data.
 */
int json_parse_vector(const json_value *data, int size, double *out);

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
 *        "x",      JCON_FLOAT(x, 0),
 *        "y",      JCON_INT(y, 0),
 *        "s",      JCON_STR(str),
 *        "?z",     JCON_INT(z, 0), // Optional, default to 0.
 *    "}",
 *    "list", "[", JCON_FLOAT(y), JCON_FLOAT(z), "]"
 * "}");
 *
 * By default all dict attribute are compulsary: if the key is not present
 * the function stops and returns an error.  We can add a '?' before a key
 * to make it optional.
 *
 * If a dict attribute is optional, and the value is not present in the json,
 * then all the children get the default values.  for example:
 *
 * float x;
 * const char *json = "{}";
 * int r;
 * r = jcon_parse(json, "{",
 *     "?dict", "{",
 *         "x", JCON_INT(x, 1),
 *     "}"
 * "}")
 * // r is set to 0 (success), and x is set to 1 (default value).
 *
 */

#define JCON_NEW(...) jcon_new(0, __VA_ARGS__)
#define JCON_INT(v, d) "i", &(v), ((int)d)
#define JCON_BOOL(v, d) "b", &(v), ((bool)d)
#define JCON_FLOAT(v, d) "f", &(v), ((double)d)
#define JCON_DOUBLE(v, d) "d", &(v), ((double)d)
#define JCON_STR(v) "s", &(v)
#define JCON_VAL(v) "v", &(v)

int jcon_parse(json_value *v, ...) __attribute__((warn_unused_result));
