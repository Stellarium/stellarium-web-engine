/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdint.h>
#include <stdbool.h>

/*
 * Function: oid_create
 * Create an oid from a catalog id and index.
 */
uint64_t oid_create(const char cat[4], uint32_t n);

/*
 * Function: oid_is_catalog
 * Return whether an oid is using a given catalog
 */
bool oid_is_catalog(uint64_t oid, const char cat[4]);

/*
 * Function: oid_is_gaia
 * Return whether an oid is actually a gaia id.
 */
bool oid_is_gaia(uint64_t oid);

/*
 * Function: oid_get_catalog
 * Get the four character catalog part of an oid
 */
const char *oid_get_catalog(uint64_t oid, char cat[4]);

/*
 * Function: oid_get_index
 * Get the index number part of a given oid
 *
 * If the oid is a gaia index, return the oid.
 */
uint64_t oid_get_index(uint64_t oid);

/*
 * Function: oid_to_str
 * Return a string representation of the oid
 *
 * Useful for debugging.
 */
const char *oid_to_str(uint64_t oid, char buf[128]);
