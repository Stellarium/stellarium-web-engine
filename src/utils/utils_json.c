/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "utils_json.h"
#include <string.h>

json_value *json_get_attr(json_value *val, const char *attr, int type)
{
    int i;
    if (!val || val->type != json_object) return NULL;
    for (i = 0; i < val->u.object.length; i++) {
        if (strcmp(attr, val->u.object.values[i].name) != 0) continue;
        if (type && val->u.object.values[i].value->type != type) {
            return NULL;
        }
        return val->u.object.values[i].value;
    }
    return NULL;
}

const char *json_get_attr_s(json_value *val, const char *attr)
{
    json_value *v;
    v = json_get_attr(val, attr, json_string);
    if (v) return v->u.string.ptr;
    return NULL;
}

double json_get_attr_f(json_value *val, const char *attr, double default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_double);
    if (v) return v->u.dbl;
    v = json_get_attr(val, attr, json_integer);
    if (v) return v->u.integer;
    return default_value;
}

int64_t json_get_attr_i(json_value *val, const char *attr,
                        int64_t default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_integer);
    if (v) return v->u.integer;
    return default_value;
}

bool json_get_attr_b(json_value *val, const char *attr, bool default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_boolean);
    if (v) return v->u.boolean;
    return default_value;
}
