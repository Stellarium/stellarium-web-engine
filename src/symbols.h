/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "painter.h"

/*
 * Module: symbols
 * Support for rendering 2d symbols on screen.
 */

/*
 * Enum: SYMBOL_ENUM
 * List of all the supported symbols.
 */
enum {
    // All the otype symbols.
    SYMBOL_ARTIFICIAL_SATELLITE = 1,
    SYMBOL_OPEN_GALACTIC_CLUSTER,
    SYMBOL_GLOBULAR_CLUSTER,
    SYMBOL_GALAXY,
    SYMBOL_INTERACTING_GALAXIES,
    SYMBOL_PLANETARY_NEBULA,
    SYMBOL_INTERSTELLAR_MATTER,
    SYMBOL_BRIGHT_NEBULA,
    SYMBOL_CLUSTER_OF_STARS,
    SYMBOL_MULTIPLE_DEFAULT,
    SYMBOL_UNKNOWN,
    SYMBOL_METEOR_SHOWER,
};

void symbols_init(void);

/*
 * Function: symbols_get_for_otype
 * Return the best available symbol we can use for a given object type.
 *
 * Parameters:
 *   type   - A simbad type name.
 *
 * Return:
 *   One of the <SYMBOL_ENUM> value, or zero if no symbol matches the type.
 */
int symbols_get_for_otype(const char *type);

/*
 * Function: symbols_paint
 * Render a given symbol
 *
 * Parameters:
 *   painter    - A painter.
 *   symbol     - One of the <SYMBOL_ENUM> value.
 *   pos        - Position in window coordinates.
 *   size       - Size in window coordinates.
 *   color      - Color to use.  If NULL we use the default defined color
 *                for the symbol.
 *   angle      - Angle, clockwise (rad).
 */
int symbols_paint(const painter_t *painter, int symbol,
                  const double pos[2], const double size[2],
                  const double color[4], double angle);

#endif // SYMBOLS_H
