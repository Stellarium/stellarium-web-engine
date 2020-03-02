/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "worker.h"
#include <string.h>

#ifndef HAVE_PTHREAD

void worker_init(worker_t *w, int (*fn)(worker_t *w))
{
    memset(w, 0, sizeof(*w));
    w->fn = fn;
}

int worker_iter(worker_t *w)
{
    if (w->state) return 1;
    w->fn(w);
    w->state = 1;
    return 1;
}

bool worker_is_running(worker_t *w)
{
    return false;
}

#endif
