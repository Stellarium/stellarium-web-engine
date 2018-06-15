/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "fader.h"
#include "utils.h"

// XXX: add easing curves support.

void fader_init(fader_t *f, bool v)
{
    f->target = v;
    f->value = v ? 1.0 : 0.0;
}

bool fader_update(fader_t *f, double dt)
{
    const double speed = 4.0;
    return move_toward(&f->value, f->target, 0, speed, dt);
}
