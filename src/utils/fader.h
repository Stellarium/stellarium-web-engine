/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdbool.h>

#ifndef FADER_H
#define FADER_H

#define FADER_DEFAULT_DURATION 0.3

/*
 * Type: fader_t
 * Smoothly change a value from 0 to 1.
 *
 * This is useful for animations effect, like fading in and out alpha values.
 *
 * The 'target' attribute contains the value we are moving to, and so can be
 * used in place of a normal boolean (for example in a GUI).
 *
 * The 'value' attribute contains the current value, and so can be used
 * when we render the effect.
 */
typedef struct {
    bool    target;    // The value we are moving to (true for 1, false for 0).
    double  value;     // Current fade value between 0 and 1.
    double  duration;  // Fade duration (s)
} fader_t;

/*
 * Function: fader_init
 * Initialize a fader using default duration.
 *
 * Parameters:
 *   f        - the fader to initialize.
 *   v        - the initial value to use.
 */
void fader_init(fader_t *f, bool v);

/*
 * Function: fader_init
 * Initialize a fader.
 *
 * Parameters:
 *   f        - the fader to initialize.
 *   v        - the initial value to use.
 *   duration - the fade duration in seconds.
 */
void fader_init2(fader_t *f, bool v, double duration);

/*
 * Function: fader_update
 * Update a fader value.
 *
 * Parameters:
 *   f        - the fader.
 *   dt       - the time increment (s).
 *
 * Return:
 *   true if the fader value has changed.
 */
bool fader_update(fader_t *f, double dt);

#endif // FADER_H
