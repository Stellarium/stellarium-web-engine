/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef FPS_H
#define FPS_H

/*
 * Type: fps_t
 * Structure that represents an FPS counter
 *
 * We keep track of the current average fps value, as well at the instantaneous
 * fps at each frame in a given window so that we can render an histogram.
 */
typedef struct
{
    int         avg;
    int         hist[64];
    double      last_time;
    double      window_start_time;
    int         window_count;
} fps_t;

/*
 * function: fps_tick
 * Update the fps counter
 *
 * Parameters:
 *   fps    - An <fps_t> struct.
 *   ts     - Current frame time (seconds).
 */
void fps_tick(fps_t *fps, double ts);

#endif // FPS_H
