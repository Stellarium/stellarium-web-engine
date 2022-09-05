/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// Compile all the libtess2 files as a single unit.

#pragma GCC diagnostic ignored "-Wempty-body"
#ifndef __clang__
    #pragma GCC diagnostic ignored "-Wclobbered"
#endif

#include "../ext_src/libtess2/bucketalloc.c"
#include "../ext_src/libtess2/dict.c"
#include "../ext_src/libtess2/geom.c"
#include "../ext_src/libtess2/mesh.c"
#undef Swap
#include "../ext_src/libtess2/priorityq.c"
#include "../ext_src/libtess2/sweep.c"
#include "../ext_src/libtess2/tess.c"
