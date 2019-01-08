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
