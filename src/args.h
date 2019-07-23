/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "json.h"
#include <stdarg.h>

/*
 * File: args.h
 * Some convenience functions to parse and generate json arguments.
 *
 * This is used for calling object functions and attributes.
 */

/*
 * Function: args_get
 * Convert a json value to a given info type value.
 *
 * We support either direct json value, either dict with the special
 * 'swe_' attribute set and the value as the 'v' attribute.
 *
 * Examples:
 *
 *  json_value *json = json_parse("true");
 *  bool v;
 *  args_get(args, TYPE_BOOL, json, &v);
 *
 *  json_value *json = json_parse('{"swe_": 1, "v": true}');
 *  bool v;
 *  args_get(args, TYPE_BOOL, json, &v);
 *
 * Parameters:
 *   args   - A json document that contains the arguments passed.
 *   type   - Type of the argument ("f", "v4", etc...).  The function will
 *            try to convert to the type if it doesn't match.
 *   ...    - A pointer to the returned value, whose type must match the
 *            given type.
 */
int args_get(const json_value *args, int type, ...);
int args_vget(const json_value *args, int type, va_list* ap);

/*
 * Function: args_value_new
 * Create a json dict that represents a returned value.
 *
 * {
 *      "swe_": 1,
 *      "type": <type>,
 *      "v": <value>
 *  }
 */
json_value *args_value_new(int type, ...);
json_value *args_vvalue_new(int type, va_list *ap);
