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

#include <math.h>

static const double Deye = 6.3; // Eye pupil diameter (mm).
static const double FOVeye = 60 / 180. * M_PI; // (rad)

static void telescope_update(telescope_t *tel)
{
    double Gl, Do, Gmag, Lmag, fo, fe, M;
    Do = tel->diameter;
    fo = tel->focal_o;
    fe = tel->focal_e;

    Gl = pow(Do / Deye, 2);
    Gmag = 2.5 * log10(Gl);
    Lmag = 2 + 5 * log10(Do);
    M = fo / fe;

    tel->magnification = M;
    tel->light_grasp = Gl;
    tel->gain_mag = Gmag;
    tel->limiting_mag = Lmag;
    tel->scope_fov = FOVeye / M;
}

void telescope_auto(telescope_t *tel, double fov)
{
    /*
     * fix the focal of the eyepiece, and pick a diameter such that M is the
     * minimum magnification:
     * http://www.rocketmime.com/astronomy/Telescope/MinimumMagnification.html
     */
    double fe, fo, Do, M;

    M = FOVeye / fov;
    if (M < 1) M = 1;

    fe = 22;
    fo = M * fe;
    Do = Deye * M;

    tel->diameter = Do;
    tel->focal_e = fe;
    tel->focal_o = fo;

    telescope_update(tel);
}
