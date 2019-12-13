/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: designations.h
 * Some functions to manipulate sky object designations.
 */

#include <stdbool.h>

/*
 * Function: designation_parse_bayer
 * Get the bayer info for a given designation.
 *
 * Params:
 *   dsgn   - A designation (eg: '* alf Aqr')
 *   cst    - Output constellation short name.
 *   bayer  - Output bayer number (1 -> α, 2 -> β, etc.)
 *
 * Return:
 *   False if the designation doesn't match a bayer name.
 */
bool designation_parse_bayer(const char *dsgn, char cst[5], int *bayer);

/*
 * Function: designation_parse_flamsteed
 * Get the Flamsteed info for a given designation.
 *
 * Params:
 *   dsgn       - A designation (eg: '* 49 Aqr')
 *   cst        - Output constellation short name.
 *   flamsteed  - Output Flamsteed number.
 *
 * Return:
 *   False if the designation doesn't match a flamsteed name
 */
bool designation_parse_flamsteed(const char *dsgn, char cst[5], int *flamsteed);

/*
 * Function: designation_cleanup
 * Create a printable version of a designation
 *
 * This can be used for example to compute the label to render for an object.
 */
void designation_cleanup(const char *dsgn, char *out, int size);
