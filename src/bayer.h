/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdbool.h>

/*
 * Function: bayer_get
 * Get the bayer designation for a given star.
 *
 * Params:
 *   hip    - A star HIP number.
 *   cons   - Get the star constellation short name.
 *   bayer  - Get the bayer number (1 -> α, 2 -> β, etc.)
 *   n      - Get the bayer extra number.
 *
 * Return:
 *   False if the star does not have a bayer designation.
 */
bool bayer_get(int hip, char cons[4], int *bayer, int *n);

/*
 * Function: bayer_iter
 * Iter all the bayer designated stars.
 *
 * Parameters:
 *   i      - An incrementing index starting from 0.
 *   hip    - Output of the star HIP number.
 *   cons   - Output of the constellation code.
 *   bayer  - Output of bayer number (1 -> α, 2 -> β, etc.).
 *   n      - Output of bayer extra number.
 *   greek  - Output of Greek letter for the number, given in three forms:
 *            utf-8, ascii abreviation, ascii full. e.g.:
 *            ("α", "alf", "Alpha").
 *
 * Return:
 *   true until there are no more stars to iter.
 */
bool bayer_iter(int i, int *hip, char cons[4], int *bayer, int *n,
                const char *greek[3]);


