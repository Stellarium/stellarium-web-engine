/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Simad otype helper functions.
 */

/*
 * Function: otype_get_str
 * Get long name of a given otype.
 *
 * Parameters:
 *   otype  - An otype condensed id string (e.g '**').  Can be shorter than
 *            4 bytes.  Doesn't have to be NULL terminated if exactly 4 bytes.
 *
 * Return:
 *   A null terminated string, or NULL if the otype doesn't exists.
 *
 */
const char *otype_get_str(const char *otype);

/*
 * Function: otype_get_parent
 * Return the parent condensed id of an otype.
 *
 * Parameters:
 *   otype  - An otype condensed id string (e.g '**').  Can be shorter than
 *            4 bytes.  Doesn't have to be NULL terminated if exactly 4 bytes.
 *
 * Return:
 *   A null padded 4 bytes condensed id string, or NULL if no parent was found.
 */
const char *otype_get_parent(const char *otype);
