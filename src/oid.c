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
#include <string.h>

uint64_t oid_create(const char cat[4], uint32_t n)
{
    uint64_t oid;
    // Make sure we passed a 4 char catalog name.
    assert(cat[0] && cat[1] && cat[2] && cat[3]);
    memcpy(((uint32_t*)&oid) + 0, &n, 4);
    memcpy(((uint32_t*)&oid) + 1, cat, 4);
    // Set first bit to 1 so that we can differentiate from Gaia.
    oid |= 0x8000000000000000UL;
    return oid;
}

bool oid_is_catalog(uint64_t oid, const char cat[4])
{
    uint32_t cat_n;
    // Make sure we passed a 4 char catalog name.
    assert(cat[0] && cat[1] && cat[2] && cat[3]);
    cat_n = oid >> 32;
    if (!(cat_n & 0x80000000)) return false;
    cat_n &= ~0x80000000;
    return memcmp(&cat_n, cat, 4) == 0;
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
