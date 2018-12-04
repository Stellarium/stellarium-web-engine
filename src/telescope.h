/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef TELESCOPE_H
#define TELESCOPE_H

typedef struct telescope {
    // Parameters values:
    double diameter;    // (mm)
    double focal_e;     // (mm)
    double focal_o;     // (mm)

    // Computed values:
    double magnification;
    double light_grasp;
    double scope_fov;   // (rad).
    double gain_mag;
    double limiting_mag;
} telescope_t;

/*
 * Function: telescope_auto
 * Automatically set the telescope value to get a given fov.
 *
 * The algo tries to pick values that represent a typical telescope setting.
 *
 * Parameters:
 *   fov    - The target scope fov (rad).
 */
void telescope_auto(telescope_t *tel, double fov);

#endif // TELESCOPE_H
