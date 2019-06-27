/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// C wrapper for earcut.

#ifndef EARCUT_H
#define EARCUT_H

#include <stdint.h>

typedef struct earcut earcut_t;

earcut_t *earcut_new(void);
void earcut_delete(earcut_t *earcut);
void earcut_set_poly(earcut_t *earcut, int size, double vertices[][2]);
void earcut_add_hole(earcut_t *earcut, int size, double vertices[][2]);
const uint16_t *earcut_triangulate(earcut_t *earcut, int *size);

#endif // EARCUT_H
