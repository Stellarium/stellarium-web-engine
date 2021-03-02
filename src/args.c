/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"


int args_vget(const json_value *args, int type, va_list* ap)
{
    const json_value *val;
    assert(args);
    int i;
    uint64_t ptr;
    double *v;
    type = type % 16;

    // An Swe object.
    if (args->type == json_object && json_get_attr(args, "swe_", 0)) {
        val = json_get_attr(args, "v", 0);
        return args_vget(val, type, ap);
    }
    val = args;

    if (type == TYPE_BOOL) {
        assert(val->type == json_boolean);
        *va_arg(*ap, bool*) = val->u.boolean;
    }
    else if (type == TYPE_INT) {
        assert(val->type == json_integer);
        *va_arg(*ap, int*) = val->u.integer;
    }
    else if (type == TYPE_FLOAT) {
        if (val->type == json_double)
            *va_arg(*ap, double*) = val->u.dbl;
        else if (val->type == json_integer)
            *va_arg(*ap, double*) = val->u.integer;
        else assert(false);
    }
    else if (type == TYPE_V2) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 2);
        for (i = 0; i < 2; i++)
            args_get(val->u.array.values[i], TYPE_FLOAT, &v[i]);
    }
    else if (type == TYPE_V3) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 3);
        for (i = 0; i < 3; i++)
            args_get(val->u.array.values[i], TYPE_FLOAT, &v[i]);
    }
    else if (type == TYPE_V4) {
        v = va_arg(*ap, double*);
        assert(val->type == json_array);
        assert(val->u.array.length == 4);
        for (i = 0; i < 4; i++)
            args_get(val->u.array.values[i], TYPE_FLOAT, &v[i]);
    }
    else if (type == TYPE_PTR) {
        switch(val->type) {
        case json_integer:
            *va_arg(*ap, void**) = (void*)val->u.integer;
            break;
        case json_string:
            sscanf(val->u.string.ptr, "0x%" PRIx64, &ptr);
            *va_arg(*ap, void**) = (void*)ptr;
            break;
        default:
            assert(false);
        }
    }
    else if (type == TYPE_STRING) {
        assert(val->type == json_string);
        strcpy(va_arg(*ap, char*), val->u.string.ptr);
    }
    else {
        LOG_E("Unknown type '%s'", obj_info_type_str(type));
        assert(false);
    }
    return 0;
}

int args_get(const json_value *args, int type, ...)
{
    va_list ap;
    int ret;
    va_start(ap, type);
    ret = args_vget(args, type, &ap);
    va_end(ap);
    return ret;
}

json_value *args_vvalue_new(int type, va_list *ap)
{
    json_value *ret, *val;
    double f;
    double *v;
    char buf[32];

    ret = json_object_new(0);
    json_object_push(ret, "swe_", json_integer_new(1));
    json_object_push(ret, "type", json_string_new(obj_info_type_str(type)));

    type = type % 16;
    if (type == TYPE_BOOL)
        val = json_boolean_new(va_arg(*ap, int));
    else if (type == TYPE_INT)
        val = json_integer_new(va_arg(*ap, int));
    else if (type == TYPE_FLOAT) {
        f = va_arg(*ap, double);
        if (isnan(f)) {
            val = json_null_new();
        } else if (isfinite(f)) {
            val = json_double_new(f);
        } else {
            snprintf(buf, sizeof(buf), "%f", f);
            val = json_string_new(buf);
        }
    }
    else if (type == TYPE_STRING)
        val = json_string_new(va_arg(*ap, char*) ?: "");
    else if (type == TYPE_PTR) {
        snprintf(buf, sizeof(buf), "0x%" PRIx64, (uint64_t)va_arg(*ap, void*));
        val = json_string_new(buf);
    } else if (type == TYPE_V2) {
        v = va_arg(*ap, double*);
        val = json_array_new(2);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
    }
    else if (type == TYPE_V3) {
        v = va_arg(*ap, double*);
        val = json_array_new(3);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
        json_array_push(val, json_double_new(v[2]));
    }
    else if (type == TYPE_V4) {
        v = va_arg(*ap, double*);
        val = json_array_new(4);
        json_array_push(val, json_double_new(v[0]));
        json_array_push(val, json_double_new(v[1]));
        json_array_push(val, json_double_new(v[2]));
        json_array_push(val, json_double_new(v[3]));
    }
    else {
        LOG_E("Unknown type '%s'", obj_info_type_str(type));
        assert(false);
        return NULL;
    }
    json_object_push(ret, "v", val);
    return ret;
}

json_value *args_value_new(int type, ...)
{
    va_list ap;
    json_value *ret;
    va_start(ap, type);
    ret = args_vvalue_new(type, &ap);
    va_end(ap);
    return ret;
}
