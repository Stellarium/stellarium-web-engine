/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#if RMT_ENABLED

#include "profiler.h"
#include "Remotery.c"

static Remotery* rmt = NULL;

int profile_init(void)
{
    int ret;
    LOG_I("Start remotery http server");
    ret = rmt_CreateGlobalInstance(&rmt);
    if (ret) LOG_E("Cannot start server");
    return ret;
}

int profile_release(void)
{
    LOG_I("Stop remotery http server");
    rmt_DestroyGlobalInstance(rmt);
    return 0;
}

#else

int profile_init(void) { return 0; }

int profile_release(void) { return 0; }

#endif
