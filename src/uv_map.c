/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


#include "uv_map.h"

#include "algos/algos.h"
#include "utils/vec.h"

#include <assert.h>

void uv_map(const uv_map_t *map, const double v[2], double out[4])
{
    map->map(map, v, out);
}

/*
 * Function: uv_map_grid
 * Compute the mapped position of a 2d grid covering the mapping.
 *
 * Parameters:
 *   map    - The mapping function used.
 *   size   - Size of the side of the grid.  The number of vertices computed
 *            is (size + 1)^2.
 *   out    - Output of all the mapped vertices.
 */
void uv_map_grid(const uv_map_t *map, int size, double (*out)[4])
{
    int i, j;
    double uv[2];

    for (i = 0; i < size + 1; i++)
    for (j = 0; j < size + 1; j++) {
        uv[0] = (double)j / size;
        uv[1] = (double)i / size;
        uv_map(map, uv, out[i * (size + 1) + j]);
    }
}

static void healpix_map(const uv_map_t *map, const double v[2], double out[4])
{
    double p[3] = {v[0], v[1], 1};
    mat3_mul_vec3(map->mat, p, p);
    healpix_xy2vec(p, out);
    out[3] = map->at_infinity ? 0.0 : 1.0;
}

static void healpix_map_update_mat(uv_map_t *map)
{
    double tmp[3];
    healpix_get_mat3(1 << map->order, map->pix, map->mat);
    if (map->swapped) {
        vec3_copy(map->mat[0], tmp);
        vec3_copy(map->mat[1], map->mat[0]);
        vec3_copy(tmp, map->mat[1]);
    }
}

void uv_map_init_healpix(uv_map_t *map, int order, int pix, bool swap,
                         bool at_infinity)
{
    memset(map, 0, sizeof(*map));
    map->type = UV_MAP_HEALPIX;
    map->order = order;
    map->pix = pix;
    map->swapped = swap;
    map->at_infinity = at_infinity;
    map->map = healpix_map;
    healpix_map_update_mat(map);
}
