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

void symbols_init(void);

texture_t *symbols_get(const char *id, double rect[2][2]);

int symbols_paint(const painter_t *painter,
                  const char *id,
                  const double pos[2], double size, const double color[4],
                  double angle);

#endif // SYMBOLS_H
