/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: profiler.h
 * Some profiling macros to use remotery:
 * https://github.com/Celtoys/Remotery
 *
 * To use it, compile with 'make remotery', then run the program and
 * launch vis/index.html in remotery sources from the browser.
 */

// Disable by default.
#ifndef RMT_ENABLED
#define RMT_ENABLED 0
#endif

int profile_init(void);
int profile_release(void);

#if RMT_ENABLED

#include "Remotery.h"

/*
 * enum: PROFILE_FLAGS
 * Same meaning as in remotery.
 */
enum {
    PROFILE_AGGREGATE = 1,
    PROFILE_RECURSIVE = 2,
};

static inline void _profile_cleanup(int *v)
{
    rmt_EndCPUSample();
}

/*
 * Macro: PROFILE
 * Put this at the top of a function to profile it.
 *
 * Parameters:
 *   name   - the name of the sample.
 *   flags  - union of <PROFILE_FLAGS> values.
 */
#define PROFILE(name, flags) \
    rmt_BeginCPUSample(name, flags); \
    int _profile __attribute__((__cleanup__(_profile_cleanup))) = 0; \
    (void)_profile;

#else

#define PROFILE(name, flags)

#endif
