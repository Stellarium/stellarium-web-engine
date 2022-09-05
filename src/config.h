/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
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

#ifdef __cplusplus
extern "C" {
#endif

// Set the DEBUG macro if it wasn't done.
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

// Force imgui to only compile stb image for jpeg and png.
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

#ifndef SWE_GUI
#   ifdef __EMSCRIPTEN__
#      define SWE_GUI 0
#   else
#      define SWE_GUI 1
#   endif
#endif

// Use stb implementation of sprintf and snprinf
#ifndef __cplusplus
#   include <stdio.h>
#   include "../ext_src/stb/stb_sprintf.h"
#   define STB_SPRINTF_NOUNALIGNED
#   ifdef sprintf
#       undef sprintf
#   endif
#   define sprintf stbsp_sprintf
#   ifdef snprintf
#       undef snprintf
#   endif
#   define snprintf stbsp_snprintf
#endif

// Ini config.
#define INI_MAX_LINE 512

// Nanovg config.
#define NVG_NO_STB

// Force using erfa_wrap.h instead of erfa.h
#if defined(ERFA_WRAP) && !defined(ERFAHDEF)
#error Please include erfa_wrap.h instead of erfa.h
#endif

// Define the LOG macros, so that they get available in the utils files.
#include "log.h"

#ifdef __cplusplus
}
#endif
