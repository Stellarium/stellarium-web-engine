/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Most of the formula come from this website:
 * http://www.rocketmime.com/astronomy/Telescope/telescope_eqn.html
 */

#include "telescope.h"
#include "utils/utils.h"

#include <math.h>

// Eye pupil diameter (mm)
static const double Deye = 6.3;

// Naked eye or eyepiece Field Of View (rad)
static const double FOVeye = 60 / 180. * M_PI;

void telescope_auto(telescope_t *tel, double fov)
{
    // Magnification is given by the current zoom level
    tel->magnification = FOVeye / fov;
    if (tel->magnification < 1) tel->magnification = 1;

    // Fix the eyepiece focal
    tel->focal_eyepiece = 22;
    // Giving the primary mirror focal
    tel->focal = tel->magnification * tel->focal_eyepiece;
    // Pick a diameter according to
    // http://www.rocketmime.com/astronomy/Telescope/MinimumMagnification.html
    // [FC]: this one doesn't make sense really, for high magnifications
    // we need to adjust the exposure time rather than get huge mirror size
    // and cap to 10m
    tel->diameter = min(10000, Deye * tel->magnification);

    tel->light_grasp = pow(tel->diameter / Deye, 2);
    tel->gain_mag = 2.5 * log10(tel->light_grasp);
    tel->limiting_mag = 2 + 5 * log10(tel->diameter);
}
