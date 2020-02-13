/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "fps.h"

#include <math.h>
#include <string.h>

void fps_tick(fps_t *fps, double ts)
{
    double v;
    if (fps->last_time == 0) {
        fps->last_time = ts;
        fps->window_start_time = ts;
        return;
    }
    fps->window_count++;
    v = 1.0 / (ts - fps->last_time);
    fps->last_time = ts;

    memmove(fps->hist + 1, fps->hist, sizeof(fps->hist) - sizeof(fps->hist[0]));
    fps->hist[0] = round(v);

    // Update average value every seconds.
    if (ts - fps->window_start_time > 1) {
        fps->avg = round(fps->window_count / (ts - fps->window_start_time));
        fps->window_start_time = ts;
        fps->window_count = 0;
    }
}
