/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// File: swe.h

#ifndef SWE_H
#define SWE_H

#include <assert.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utarray.h"
#include "uthash.h"
#include "utlist.h"
#include "utstring.h"
#include "json.h"
#include "json-builder.h"
#include "eph-file.h"
#include "erfa.h"
#include "log.h"
#include "tests.h"

#include "utils/cache.h"
#include "utils/color.h"
#include "utils/fader.h"
#include "utils/gesture.h"
#include "utils/progressbar.h"
#include "utils/texture.h"
#include "utils/utils.h"
#include "utils/utils_json.h"
#include "utils/utf8.h"
#include "utils/request.h"
#include "utils/vec.h"
#include "utils/worker.h"

#include "algos/algos.h"
#include "args.h"
#include "assets.h"
#include "constants.h"

#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#else
#   define EMSCRIPTEN_KEEPALIVE
#endif

#ifndef SWE_VERSION_STR
#define SWE_VERSION_STR "0.1.0"
#endif

#if __clang__
#   define SWE_COMPILER_STR __clang_version__
#else
#   define SWE_COMPILER_STR __VERSION__
#endif

/********* Logging macros *************************************************/

#define ASSERT(c, msg, ...) do { \
        if (!(c)) {LOG_E(msg, ##__VA_ARGS__); assert(false);} \
    } while (0)

// I redefine asprintf so that we can ignore the return value.
// XXX: we should probably remove that and just check the return values
// or don't use asprintf.
#ifndef __cplusplus
#define asprintf(...) ({int r = asprintf(__VA_ARGS__); r;})
#define vasprintf(...) ({int r = vasprintf(__VA_ARGS__); r;})
#endif

const char *get_compiler_str(void);

/*
 * Function: swe_gen_doc
 * Print out generated documentation about the defined classes.
 */
void swe_gen_doc(void);



/**************************************************************************/

#include "painter.h"
#include "obj.h"
#include "core.h"
#include "gui.h"
#include "symbols.h"
#include "system.h"

#endif // SWE_H
