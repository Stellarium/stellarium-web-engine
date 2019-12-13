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
 * Enum: BAYER_FLAGS
 * Flags that specify how to format a Bayer or Flamsteed name.
 *
 * Values:
 *   BAYER_LATIN_SHORT  - Use latin format for the greek letter, e.g. will
 *                        return 'Bet Cen'. Default is to use greek format like
 *                        in 'ß Cen'. Flag has no effect on non-greek names.
 *   BAYER_LATIN_LONG   - Use latin long format for the greek letter, e.g. will
 *                        return 'Beta Cen'. Default is to use greek format like
 *                        in 'ß Cen'. Flag has no effect on non-greek names.
 *   BAYER_CONST_SHORT  - Append short constellation name, e.g. 'ß Cen'.
 *                        Default is to display no constellation name.
 *   BAYER_CONST_LONG   - Append long constellation name, e.g. 'ß Centauri'.
 *                        Default is to display no constellation name.
 */
enum {
    BAYER_LATIN_SHORT       = 1 << 0,
    BAYER_LATIN_LONG        = 1 << 1,
    BAYER_CONST_SHORT       = 1 << 2,
    BAYER_CONST_LONG        = 1 << 3,
};

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
void designation_cleanup(const char *dsgn, char *out, int size, int flags);
