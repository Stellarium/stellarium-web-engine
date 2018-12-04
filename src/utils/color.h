/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdint.h>

#define RGBA(r, g, b, a) {r / 255., g / 255., b / 255., a / 255.}

#define HEX_RGBA(v) { \
    ((v >> 24) & 0xff) / 255.0f, \
    ((v >> 16) & 0xff) / 255.0f, \
    ((v >>  8) & 0xff) / 255.0f, \
    ((v >>  0) & 0xff) / 255.0f}

/*
 * Function: xyY_to_srgb
 * Convert color from xyY to srbg
 */
void xyY_to_srgb(const double xyY[3], double srgb[3]);

/*
 * Function: xyY_to_rgb
 * Convert color from xyY to rbg
 */
void xyY_to_rgb(const double xyY[3], double rgb[3]);

/*
 * Function: hex_to_rgba
 * Convert an hexadecial encoded color to an rgba value
 */
void hex_to_rgba(uint32_t v, double rgba[4]);
