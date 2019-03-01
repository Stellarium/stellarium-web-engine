/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "oid.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

uint64_t oid_create(const char *cat, uint32_t n)
{
    uint64_t oid;
    assert(cat[strlen(cat) - 1] != ' ');
    memcpy(((uint32_t*)&oid) + 0, &n, 4);
    strncpy(((char*)&oid) + 4, cat, 4);
    // Set first bit to 1 so that we can differentiate from Gaia.
    oid |= 0x8000000000000000UL;
    return oid;
}

bool oid_is_catalog(uint64_t oid, const char *cat)
{
    uint32_t cat_n;
    assert(cat[strlen(cat) - 1] != ' ');
    cat_n = oid >> 32;
    if (!(cat_n & 0x80000000)) return false;
    cat_n &= ~0x80000000;
    return strncmp((char*)&cat_n, cat, 4) == 0;
}

bool oid_is_gaia(uint64_t oid)
{
    return ((oid >> 32) & 0x80000000) == 0;
}

const char *oid_get_catalog(uint64_t oid, char cat[4])
{
    uint32_t cat_n;
    if (oid_is_gaia(oid)) {
        memcpy(cat, "GAIA", 4);
    } else {
        cat_n = oid >> 32;
        cat_n &= ~0x80000000;
        memcpy(cat, &cat_n, 4);
    }
    return cat;
}

/*
 * Function: oid_get_index
 * Get the index number part of a given oid
 *
 * If the oid is a gaia index, return the oid.
 */
uint64_t oid_get_index(uint64_t oid)
{
    if (oid_is_gaia(oid)) return oid;
    return (uint32_t)oid;
}

const char *oid_to_str(uint64_t oid, char buf[128])
{
    char cat[4];
    if (oid_is_gaia(oid)) {
        snprintf(buf, 128, "Gaia DR2 %" PRIx64, oid);
    } else {
        snprintf(buf, 128, "%.4s %u",
                 oid_get_catalog(oid, cat), (uint32_t)oid);
    }
    return buf;
}
