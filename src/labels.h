/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdint.h>

/***** Labels manager *****************************************************/

enum {
    LABEL_AROUND    = 1 << 8, // Auto place label around position.
    LABEL_UPPERCASE = 1 << 10,
    LABEL_BOLD      = 1 << 11,
};

void labels_reset(void);

/*
 * Function: labels_add
 * Render a label on screen.
 *
 * Parameters:
 *   text       - The text to render.
 *   win_pow    - Position of the text in windows coordinates.
 *   radius     - Radius of the point the label is linked to.  Zero for
 *                independent label.
 *   size       - Height of the text in pixel.
 *   color      - Color of the text.
 *   angle      - Rotation angle (rad).
 *   flags      - Union of <ALIGN_FLAGS> and <LABEL_FLAGS>.
 *                Used to specify anchor position and text effects.
 *   oid        - Optional unique id for the label.
 */
void labels_add(const char *text, const double win_pos[2],
                double radius, double size, const double color[4],
                double angle, int flags, double priority, uint64_t oid);
