/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef UV_MAP_H
#define UV_MAP_H

/*
 * Section: UV map
 *
 * UV map are used to represent parametric functions that map 2D UV coordinates
 * into the 3D space.
 *
 * For example this is used to map an healpix pixel into the sphere.
 * The advantage of using a parametric form is that we can split a shape
 * into smaller parts, while if we only had the shape vertices positions
 * we couldn't do that properly.
 *
 */

#include <stdbool.h>

enum {
    UV_MAP_HEALPIX = 1,
};


typedef struct uv_map uv_map_t;
struct uv_map
{
    int type;
    int order;
    int pix;
    union {
        double mat[3][3];
        double mat4[4][4];
    };
    void *user;
    bool swapped;
    bool at_infinity;
    void (*map)(const uv_map_t *t, const double v[2], double out[4]);
};

/*
 * Function: uv_map
 * Map a 2D UV position into a 3D position.
 *
 * The returned position are in homogeneous coordinates (xyzw) so that
 * we can easily map to infinity.
 *
 * Parameters:
 *   map    - The mapping function used.
 *   uv     - Input UV coordinates.
 *   out    - Output 3d homogeneous coordinates.
 */
void uv_map(const uv_map_t *map, const double v[2], double out[4]);

/*
 * Function: uv_map_init_healpix
 * Init an UV mapping for an healpix pixel.
 *
 * Parameters:
 *   map    - A mapping struct that will be set.
 *   order  - Healpix pixel order.
 *   pix    - Healpix pixel pix.
 *   swap   - If set, swap the coordinates (for culling).
 *   at_infinity - If set, map to infinity, otherwise map to the unit sphere.
 */
void uv_map_init_healpix(uv_map_t *map, int order, int pix, bool swap,
                         bool at_infinity);


#endif // CELL_H
