/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <assert.h>
#include <string.h>

#include "json-builder.h"
#include "json_expression.h"

static bool json_equals(const json_value *a, const json_value *b)
{
    if (a->type != b->type) return false;
    switch (a->type) {
    case json_string:
        return (strcmp(a->u.string.ptr, b->u.string.ptr) == 0);
    default:
        return false;
    }
}

static json_value *json_op_get(const json_value *ctx,
                               int nb_args, const json_value *args[2],
                               bool *should_delete)
{
    const char *key;
    const json_value *obj;
    int i;

    if (nb_args < 1 || nb_args > 2) return NULL;
    if (args[0]->type != json_string) return NULL;
    key = args[0]->u.string.ptr;
    obj = nb_args == 2 ? args[1] : ctx;
    if (!obj || obj->type != json_object) return NULL;

    *should_delete = false;
    for (i = 0; i < obj->u.object.length; i++) {
        if (strcmp(key, obj->u.object.values[i].name) == 0)
            return obj->u.object.values[i].value;
    }
    return NULL;
}

json_value *json_expression_eval(const json_value *ctx,
                                 const json_value *expr,
                                 bool *should_delete)
{
    json_value *op, *args[2] = {}, *ret = NULL;
    bool args_should_delete[2] = {};
    int nb_args, i;

    if (expr->type != json_array) {
        *should_delete = false;
        return expr;
    }

    if (expr->u.array.length == 0) goto error;
    op = expr->u.array.values[0];
    if (op->type != json_string) goto error;

    nb_args = expr->u.array.length - 1;
    if (nb_args > 2) goto error;
    for (i = 0; i < nb_args; i++) {
        args[i] = json_expression_eval(ctx, expr->u.array.values[i + 1],
                                       &args_should_delete[i]);
    }

    if (strcmp(op->u.string.ptr, "==") == 0) {
        if (nb_args != 2) goto error;
        *should_delete = true;
        ret = json_boolean_new(json_equals(args[0], args[1]));
        goto end;
    }

    if (strcmp(op->u.string.ptr, "get") == 0) {
        if (nb_args < 1) goto error;
        ret = json_op_get(ctx, nb_args, (const json_value**)args,
                          should_delete);
        goto end;
    }

error:
    ret = NULL;
end:
    for (i = 0; i < 2; i++) {
        if (args_should_delete[i])
            json_builder_free(args[i]);
    }

    if (!ret) {
        *should_delete = false;
        return json_null_new();
    }
    return ret;

}

bool json_expression_eval_bool(const json_value* ctx, const json_value *expr)
{
    json_value *val;
    bool ret, should_delete;
    val = json_expression_eval(ctx, expr, &should_delete);
    ret = val && val->type == json_boolean && val->u.boolean;
    if (should_delete) json_builder_free(val);
    return ret;
}
