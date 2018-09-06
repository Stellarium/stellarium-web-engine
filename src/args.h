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
 * Extract a value from a json arguments document.
 *
 * It works with both dict arguments and array arguments.
 *
 * Example:
 *
 *   // With the json document 'args' that can either be a dict:
 *   // {
 *   //   "x": 10,
 *   //   "vect": [10, 20, 30],
 *   // }
 *   //
 *   // or an array:
 *   // [10, [10, 20, 30]]
 *   //
 *   // We can extract the values:
 *   int x;
 *   double v[3];
 *   args_get(args, "x", 1, "d", NULL, &x);
 *   args_get(args, "vect", 2, "vect", NULL, &v);
 *
 *
 * Parameters:
 *   args   - A json document that contains the arguments passed.
 *   name   - Name of the argument we want to extrace.  Can be NULL.
 *   pos    - Position of the argument (for array input).  Starts at 1.
 *            Zero if we want a named argument only.
 *   type   - Type of the argument ("f", "v4", etc...).  The function will
 *            try to convert to the type if it doesn't match.
 *   hint   - Optional hint string that will be used in the conversion.
 *   ...    - A pointer to the returned value, whose type must match the
 *            given type.
 */
int args_get(const json_value *args, const char *name, int pos,
             const char *type, const char *hint, ...);
int args_vget(const json_value *args, const char *name, int pos,
              const char *type, const char *hint, va_list ap);

/*
 * Function: args_value_new
 * Create a json dict that represents a returned value.
 *
 * {
 *      "swe_": 1,
 *      "type": <type>,
 *      "hint": <hint>,
 *      "v": <value>
 *  }
 */
json_value *args_value_new(const char *type, const char *hint, ...);
json_value *args_vvalue_new(const char *type, const char *hint, va_list ap);

