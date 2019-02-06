/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static int convert_to_f(const json_value *val, const char *hint, double *out)
{
    // String to MJD:
    // 2009-09-06T17:00:00.000Z
    if (val->type == json_string && hint && strcmp(hint, "mjd") == 0) {
        return time_parse_iso("UTC", val->u.string.ptr, out);
    }
    if (val->type == json_double) *out = val->u.dbl;
    if (val->type == json_integer) *out = val->u.integer;
    return 0;
}

static int convert_to_p(const json_value *val, const char *hint, void **out)
{
    // String to object.
    if (val->type == json_string && strcmp(hint, "obj") == 0) {
        *out = obj_get(NULL, val->u.string.ptr, 0);
        return 0;
    }
    if (val->type == json_integer) *out = (void*)val->u.integer;
    return 0;
}

static int convert_to_s(const json_value *val, const char *hint, char **out)
{
    const char *vhint = NULL;
    char buf[64];
    if (val->type == json_object && json_get_attr(val, "swe_", 0)) {
        vhint = json_get_attr_s(val, "hint");
        val = json_get_attr(val, "v", 0);
    }
    if (val->type == json_double && vhint && strcmp(vhint, "d_angle") == 0) {
        asprintf(out, "%s", format_dangle(buf, val->u.dbl));
        return 0;
    }
    if (val->type == json_double && vhint && strcmp(vhint, "h_angle") == 0) {
        asprintf(out, "%s", format_hangle(buf, val->u.dbl));
        return 0;
    }
    if (val->type == json_string) *out = strdup(val->u.string.ptr);
    if (val->type == json_boolean)
        asprintf(out, "%s", val->u.boolean ? "true" : "false");
    if (val->type == json_integer) asprintf(out, "%" PRId64, val->u.integer);
    if (val->type == json_double) asprintf(out, "%f", val->u.dbl);
    return 0;
}

int args_vget(const json_value *args, const char *name, int pos,
              const char *type, const char *hint, va_list* ap)
{
    const json_value *val;
    assert(args);
    int i;
    double *v;
    char *str;

    if (args->type == json_array && pos) {
        if (pos <= args->u.array.length) {
            val = args->u.array.values[pos - 1];
            return args_vget(val, NULL, 0, type, hint, ap);
        }
    }

    // An Swe object.
    if (args->type == json_object && json_get_attr(args, "swe_", 0)) {
        val = json_get_attr(args, "v", 0);
        return args_vget(val, NULL, 0, type, hint, ap);
    }

    // Named arguments.
    if (    name && args->type == json_object &&
            !json_get_attr(args, "swe_", 0)) {
        val = json_get_attr(args, name, 0);
        assert(val);
        return args_vget(val, NULL, 0, type, hint, ap);
    }

    if (pos != 0) {
        // Not necessarily an error.
        // LOG_E("Cannot get attr '%s' %d", name, pos);
        return -1;
    }
    val = args;

    if (strcmp(type, "b") == 0) {
        assert(val->type == json_boolean);
        *va_arg(*ap, bool*) = val->u.boolean;
    }
    else if (strcmp(type, "d") == 0) {
        assert(val->type == json_integer);
        *va_arg(*ap, int*) = val->u.integer;
    }
    else if (strcmp(type, "f") == 0) {
        convert_to_f(val, hint, va_arg(*ap, double*));
    }
    else if (strcmp(type, "v2") == 0) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 2);
        for (i = 0; i < 2; i++)
            args_get(val->u.array.values[i], NULL, 0, "f", NULL, &v[i]);
    }
    else if (strcmp(type, "v3") == 0) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 3);
        for (i = 0; i < 3; i++)
            args_get(val->u.array.values[i], NULL, 0, "f", NULL, &v[i]);
    }
    else if (strcmp(type, "v4") == 0) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 4);
        for (i = 0; i < 4; i++)
            args_get(val->u.array.values[i], NULL, 0, "f", NULL, &v[i]);
    }
    else if (strcmp(type, "p") == 0) {
        convert_to_p(val, hint, va_arg(*ap, void**));
    }
    else if (strcmp(type, "s") == 0 || strcmp(type, "S") == 0) {
        convert_to_s(val, hint, &str);
        strcpy(va_arg(*ap, char*), str);
        free(str);
    }
    else {
        LOG_E("Unknown type '%s'", type);
        assert(false);
    }
    return 0;
}

int args_get(const json_value *args, const char *name, int pos,
             const char *type, const char *hint, ...)
{
    va_list ap;
    int ret;
    va_start(ap, hint);
    ret = args_vget(args, name, pos, type, hint, &ap);
    va_end(ap);
    return ret;
}

json_value *args_vvalue_new(const char *type, const char *hint, va_list *ap)
{
    json_value *ret, *val;
    double f;
    double *v;
    char buf[16];

    ret = json_object_new(0);
    json_object_push(ret, "swe_", json_integer_new(1));
    json_object_push(ret, "type", json_string_new(type));
    if (hint) json_object_push(ret, "hint", json_string_new(hint));

    if (strcmp(type, "b") == 0)
        val = json_boolean_new(va_arg(*ap, int));
    else if (strcmp(type, "d") == 0)
        val = json_integer_new(va_arg(*ap, int));
    else if (strcmp(type, "f") == 0) {
        f = va_arg(*ap, double);
        if (isnan(f)) {
            val = json_null_new();
        } else if (isfinite(f)) {
            val = json_double_new(f);
        } else {
            sprintf(buf, "%f", f);
            val = json_string_new(buf);
        }
    }
    else if (strcmp(type, "s") == 0 || strcmp(type, "S") == 0)
        val = json_string_new(va_arg(*ap, char*) ?: "");
    else if (strcmp(type, "p") == 0)
        val = json_integer_new((uintptr_t)va_arg(*ap, void*));
    else if (strcmp(type, "v2") == 0) {
        v = va_arg(*ap, double*);
        val = json_array_new(2);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
    }
    else if (strcmp(type, "v3") == 0) {
        v = va_arg(*ap, double*);
        val = json_array_new(3);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
        json_array_push(val, json_double_new(v[2]));
    }
    else if (strcmp(type, "v4") == 0) {
        v = va_arg(*ap, double*);
        val = json_array_new(4);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
        json_array_push(val, json_double_new(v[2]));
        json_array_push(val, json_double_new(v[3]));
    }
    else {
        LOG_E("Unknown type '%s'", type);
        assert(false);
        return NULL;
    }
    json_object_push(ret, "v", val);
    return ret;
}

json_value *args_value_new(const char *type, const char *hint, ...)
{
    va_list ap;
    json_value *ret;
    va_start(ap, hint);
    ret = args_vvalue_new(type, hint, &ap);
    va_end(ap);
    return ret;
}
