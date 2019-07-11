/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "../ext_src/earcut/earcut.hpp"

#include <array>
#include <string.h>

extern "C" {
#include "earcut.h"
}

using Point = std::array<double, 2>;

struct earcut {
    std::vector<std::vector<Point>> polygon;
    std::vector<uint16_t> triangles;
};


earcut_t *earcut_new(void)
{
    return new earcut_t();
}

void earcut_delete(earcut_t *earcut)
{
    delete earcut;
}

void earcut_add_poly(earcut_t *earcut, int size, double vertices[][2])
{
    int i;
    std::vector<Point> poly;
    poly.reserve(size);
    for (i = 0; i < size; i++) {
        poly.push_back({vertices[i][0], vertices[i][1]});
    }
    earcut->polygon.push_back(poly);
}

const uint16_t *earcut_triangulate(earcut_t *earcut, int *size)
{
    earcut->triangles = mapbox::earcut<uint16_t>(earcut->polygon);
    *size = earcut->triangles.size();
    return earcut->triangles.data();
}
