/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "grid_cache.h"
#include "swe.h"
#include "utils/cache.h"

#include <stdlib.h>
#include <string.h>

#define CACHE_SIZE (2 * (1 << 20))

static cache_t *g_cache = NULL;

/*
 * Function: grid_cache_get
 * Return a pointer to an healpix grid for fast texture projection
 *
 * Parameters:
 *   order      - Healpix pixel order argument.
 *   pix        - Healpix pix.
 *   mat        - UV coordinates of a quad inside the healpix pixel.
 *   split      - Number of splits to use for the grid.
 *
 * Return:
 *   A (split + 1)^2 grid of 3d positions.
 */
const double (*grid_cache_get(int order, int pix,
                              const double mat[3][3],
                              int split))[3]
{
    int n = split + 1;
    int i, j;
    double (*grid)[3];
    struct {
        int nside;
        int pix;
        int split;
        int pad_;
        double mat[3][3];
    } key = { order, pix, split };
    uv_map_t map;
    double p[4];

    uv_map_init_healpix(&map, order, pix, true, true);

    _Static_assert(sizeof(key) == 88, "");
    memcpy(key.mat, mat, sizeof(key.mat));
    if (!g_cache) g_cache = cache_create(CACHE_SIZE);
    grid = cache_get(g_cache, &key, sizeof(key));
    if (grid) return grid;
    grid = calloc(n * n, sizeof(*grid));
    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec3_set(p, (double)j / split, (double)i / split, 1.0);
        mat3_mul_vec3(mat, p, p);
        uv_map(&map, p, p);
        grid[i * n + j][0] = p[0];
        grid[i * n + j][1] = p[1];
        grid[i * n + j][2] = p[2];
    }
    cache_add(g_cache, &key, sizeof(key), grid, sizeof(*grid) * n * n, NULL);
    return grid;
}
