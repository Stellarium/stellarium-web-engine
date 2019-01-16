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
 * Function: otypes_lookup
 * Get info about a given otype by condensed value.
 */
int otypes_lookup(const char *condensed,
                  const char **name,
                  const char **explanation,
                  int ns[4]);

/*
 * Function: otypes_get_parent
 * Return the parent condensed id of an otype.
 */
const char *otypes_get_parent(const char *condensed);
