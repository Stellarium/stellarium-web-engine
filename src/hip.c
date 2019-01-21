/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdint.h>

static const uint8_t PIX_ORDER_2[] = {
#include "hip.inl"
};

int hip_get_pix(int hip, int order)
{
    int ret;
    if (order > 2) return -1;
    if (hip >= sizeof(PIX_ORDER_2)) return -1;
    ret = PIX_ORDER_2[hip];
    if (ret == 255) return -1;
    ret /= (1 << (2 * (2 - order))); // For order 0 and 1.
    return ret;
}
