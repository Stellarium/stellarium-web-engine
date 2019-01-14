/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "fader.h"
#include <assert.h>
#include <math.h>

// XXX: add easing curves support.


static double cmp(float x, float y)
{
    return x > y ? +1 : x < y ? -1 : 0;
}

/*
 * Function: move_toward
 * Move a value toward a target value.
 *
 * Return:
 *   True if the value changed.
 */
static bool move_toward(double *x,
                        double target,
                        int easing, // For the moment need to be 0.
                        double speed,
                        double dt)
{
    double d = speed * dt;
    assert(easing == 0);
    if (*x == target) return false;
    if (fabs(*x - target) <= d) {
        *x = target;
    } else {
        *x += d * cmp(target, *x);
    }
    return true;
}

void fader_init(fader_t *f, bool v)
{
    f->target = v;
    f->value = v ? 1.0 : 0.0;
}

bool fader_update(fader_t *f, double dt)
{
    const double speed = 15.0;
    return move_toward(&f->value, f->target, 0, speed, dt);
}
