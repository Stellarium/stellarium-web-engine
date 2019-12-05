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

// Here is a list of typical telescope setups, which could be used
// for future improved simulation of instrument.

// Visual/binoculars view
// FOV    Diam  Focal Eyep-Foc Magnif  Name
//----------------------------------------------------
// 120     6.3    n/a      n/a   x0.5  Eye
// 60      6.3    n/a      n/a     x1  Eye
// 12       30    n/a      n/a     x5  5x30 Binoculars
//  8.5     35    n/a      n/a     x7  7x35 Binoculars
//  6       50    n/a      n/a    x10  10x50 Binoculars
//  3       80    n/a      n/a    x20  20x80 Astro Binoculars

// Cheap amateur telescopes
// FOV    Diam  Focal Eyep-Foc Magnif  Name
//----------------------------------------------------
// 1.71     70    700       20    x35  70/700 Refractor, 20 mm eyepiece
// 1.66    114    900       25    x36  114/900 Telescope, 25 mm eyepiece
// 0.89    203   2032       30    x67  C8 Telescope F/10, 30 mm eyepiece

// Largest visual telescopes
// FOV    Diam  Focal Eyep-Foc Magnif  Name
//----------------------------------------------------
// 1.71    356   1650       35    x35  Big Dobsonian F/4.6, 35 mm ep
// 0.71   1000   3000       35    x85  1m Dobsonian F/3, 35 mm ep

// Amateur DSLR photo on a C8
// 1 image = 3000 x 2000 px covering 2.1 x 1.4 deg, 30 min exposure
// For a 800 pixel screen, full res is 0.56 deg
// FOV    Diam  Focal Exp-time Magnif  Name
//----------------------------------------------------
// 1.68    203   2032     30    x36  DSLR on C8, 1/3 resolution
// 0.56    203   2032     30   x107  DSLR on C8, full resolution

// Typical DSS telescope photo setup (UK Schmidt telescope):
// 1 image = 23040 x 23040 px covering 6.4 x 6.4 deg, ~30 min exposure
// For a 800 pixel screen, full res is 0.22 deg
// FOV    Diam  Focal Exp-time Magnif  Name
//----------------------------------------------------
// 0.66   1830   3070     30    x90  DSS, 1/3 resolution
// 0.22   1830   3070     30   x272  DSS, full resolution



void telescope_auto(telescope_t *tel, double fov)
{
    // Magnification is given by the current zoom level
    tel->magnification = FOVeye / fov;

    // Fix the eyepiece focal
    tel->focal_eyepiece = 22;
    // Giving the primary mirror focal
    tel->focal = tel->magnification * tel->focal_eyepiece;
    // Pick a diameter according to
    // http://www.rocketmime.com/astronomy/Telescope/MinimumMagnification.html
    // [FC]: this one doesn't make sense really, for high magnifications
    // we need to adjust the exposure time rather than get huge mirror size
    tel->diameter = min(10000000, Deye * tel->magnification);

    // For FOV < 10 deg we start to slowly increase the exposure time
    // This is ad-hoc but required to match more closely what a user
    // expects when zooming when transitioning from visual observation
    // to photograpic exposure.
    tel->exposure = pow(max(1, 5.0 * M_PI / 180 / fov), 0.07);

    tel->light_grasp = pow(tel->diameter / Deye, 2) * tel->exposure;
    // Make sure we never simulate a too small eye pupil. This allows to remove
    // a number of hacks in different parts of the code
    tel->light_grasp = max(0.4, tel->light_grasp);
    tel->gain_mag = 2.5 * log10(tel->light_grasp);
    tel->limiting_mag = 2 + 5 * log10(tel->diameter);
}
