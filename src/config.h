/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * This file gets included before any compiled file.  So we can use it
 * to set configuration macros that affect external libraries.
 */

// Do not issue warnings when passing const values to non const functions.
// This is so that we can use erfa libraries.
#ifndef __cplusplus
#   if __clang__
#       pragma clang diagnostic ignored \
                "-Wincompatible-pointer-types-discards-qualifiers"
#   else
#       pragma GCC diagnostic ignored "-Wdiscarded-array-qualifiers"
#       pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#   endif
#endif

// Set the DEBUG macro if it wasn't done.
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

// Imgui configs.
#define IMGUI_INCLUDE_IMGUI_USER_INL

#define SWE_SHOW_SET_TIME 1

#ifdef __EMSCRIPTEN__
    #define SWE_GUI 0
#else
    #define SWE_GUI 1
#endif


// Define the LOG macros, so that they get available in the utils files.
#include "log.h"

