/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: json_expression.h
 * Allow to evaluate simple expressions expressed as json object.
 *
 * Example:
 *
 *   ["==", ["get", "name"], "Guillaume"]
 */

#include "json.h"

#include <stdbool.h>

json_value *json_expression_eval(const json_value *ctx,
                                 const json_value *expr,
                                 bool *should_delete);

bool json_expression_eval_bool(const json_value* ctx, const json_value *expr);
