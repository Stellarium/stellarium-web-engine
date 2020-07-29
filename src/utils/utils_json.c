/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "utils_json.h"
#include "utstring.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

json_value *json_get_attr(const json_value *val, const char *attr, int type)
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

const char *json_get_attr_s(const json_value *val, const char *attr)
{
    json_value *v;
    v = json_get_attr(val, attr, json_string);
    if (v) return v->u.string.ptr;
    return NULL;
}

double json_get_attr_f(const json_value *val, const char *attr,
                       double default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_double);
    if (v) return v->u.dbl;
    v = json_get_attr(val, attr, json_integer);
    if (v) return v->u.integer;
    return default_value;
}

int64_t json_get_attr_i(const json_value *val, const char *attr,
                        int64_t default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_integer);
    if (v) return v->u.integer;
    return default_value;
}

bool json_get_attr_b(const json_value *val, const char *attr,
                     bool default_value)
{
    json_value *v;
    v = json_get_attr(val, attr, json_boolean);
    if (v) return v->u.boolean;
    return default_value;
}

json_value *json_copy(const json_value *val)
{
    json_value *ret;
    int i;

    if (!val) return json_object_new(0);

    switch (val->type) {
    case json_none:
        assert(false);
        return NULL;
    case json_integer:
        return json_integer_new(val->u.integer);
    case json_double:
        return json_double_new(val->u.dbl);
    case json_string:
        return json_string_new_length(val->u.string.length, val->u.string.ptr);
    case json_boolean:
        return json_boolean_new(val->u.boolean);
    case json_null:
        return json_null_new();
    case json_object:
        ret = json_object_new(val->u.object.length);
        for (i = 0; i < val->u.object.length; i++) {
            json_object_push(ret, val->u.object.values[i].name,
                             json_copy(val->u.object.values[i].value));
        }
        return ret;
    case json_array:
        ret = json_array_new(val->u.array.length);
        for (i = 0; i < val->u.array.length; i++) {
            json_array_push(ret, json_copy(val->u.array.values[i]));
        }
        return ret;
    }

    assert(false);
    return NULL;
}

json_value *json_vector_new(int size, const double *values)
{
    int i;
    json_value *ret;
    ret = json_array_new(size);
    for (i = 0; i < size; i++)
        json_array_push(ret, json_double_new(values[i]));
    return ret;
}

int json_parse_vector(const json_value *data, int size, double *out)
{
    int i;
    const json_value *e;

    if (!data || data->type != json_array) return -1;
    if (data->u.array.length != size) return -1;
    for (i = 0; i < size; i++) {
        e = data->u.array.values[i];
        switch (e->type) {
            case json_double: out[i] = e->u.dbl; break;
            case json_integer: out[i] = e->u.integer; break;
            default: return -1;
        }
    }
    return 0;
}


static int jcon_parse_(json_value *v, va_list *ap)
{
    const char *token;
    json_value *child;
    int i, r;
    bool required;
    union {
        float *f;
        int *i;
        bool *b;
        double *d;
        const char **s;
        json_value **v;
    } ptr = {};

    token = va_arg(*ap, const char*);
    assert(token[1] == '\0');

    if (token[0] == ']') return 1;

    if (token[0] == 'f') {
        ptr.f = va_arg(*ap, float*);
        *ptr.f = va_arg(*ap, double);
        if (!v) return 0;
        if (v->type != json_double && v->type != json_integer) return -1;
        if (v->type == json_double) *ptr.f = v->u.dbl;
        if (v->type == json_integer) *ptr.f = v->u.integer;
        return 0;
    }

    if (token[0] == 'd') {
        ptr.d = va_arg(*ap, double*);
        *ptr.d = va_arg(*ap, double);
        if (!v) return 0;
        if (v->type != json_double && v->type != json_integer) return -1;
        if (v->type == json_double) *ptr.d = v->u.dbl;
        if (v->type == json_integer) *ptr.d = v->u.integer;
        return 0;
    }

    if (token[0] == 'i') {
        ptr.i = va_arg(*ap, int*);
        *ptr.i = va_arg(*ap, int);
        if (!v) return 0;
        if (v->type != json_integer) return -1;
        *ptr.i = v->u.integer;
        return 0;
    }

    if (token[0] == 'b') {
        ptr.b = va_arg(*ap, bool*);
        *ptr.b = va_arg(*ap, int);
        if (!v) return 0;
        if (v->type != json_boolean) return -1;
        *ptr.b = v->u.boolean;
        return 0;
    }

    if (token[0] == 's') {
        ptr.s = va_arg(*ap, const char **);
        *ptr.s = NULL;
        if (!v) return 0;
        if (v->type != json_string) return -1;
        *ptr.s = v->u.string.ptr;
        return 0;
    }

    if (token[0] == 'v') {
        ptr.v = va_arg(*ap, json_value **);
        *ptr.v = NULL;
        if (!v) return 0;
        *ptr.v = v;
        return 0;
    }

    if (token[0] == '{') {
        if (v && v->type != json_object) return -1;
        while (true) {
            token = va_arg(*ap, const char *);
            if (token[0] == '}') {
                assert(token[1] == '\0');
                break;
            }

            // attribute staring with '?' are optional
            required = true;
            if (token[0] == '?') {
                required = false;
                token++;
            }
            assert(token[0] >= 'A' && token[0] <= 'z');

            child = v ? json_get_attr(v, token, 0) : NULL;
            if (v && !child && required) return -1;
            r = jcon_parse_(child, ap);
            if (r) return -1;
        }
        return 0;
    }

    if (token[0] == '[') {
        if (v && v->type != json_array) return -1;
        for (i = 0; ; i++) {
            child = (v && i < v->u.array.length) ? v->u.array.values[i] : NULL;
            r = jcon_parse_(child, ap);
            if (r == 1) return 0;
            if (r) return -1;
        }
    }

    return -1;
}

int jcon_parse(json_value *v, ...)
{
    int ret;
    va_list ap;
    va_start(ap, v);
    ret = jcon_parse_(v, &ap);
    va_end(ap);
    return ret ? -1 : 0;
}
