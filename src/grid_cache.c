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
 *   nside      - Healpix pixel nside argument.
 *   pix        - Healpix pix.
 *   uv         - UV coordinates of a quad inside the healpix pixel.
 *   split      - Number of splits to use for the grid.
 *
 * Return:
 *   A (split + 1)^2 grid of 3d positions.
 */
const double (*grid_cache_get(int nside, int pix,
                              const double uv[4][2],
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
        double uv[4][2];
    } key = { nside, pix, split };
    projection_t proj;
    double p[4], duvx[2], duvy[2];

    projection_init_healpix(&proj, nside, pix, true, true);

    vec2_sub(uv[1], uv[0], duvx);
    vec2_sub(uv[2], uv[0], duvy);
    _Static_assert(sizeof(key) == 80, "");
    memcpy(key.uv, uv, sizeof(key.uv));
    if (!g_cache) g_cache = cache_create(CACHE_SIZE);
    grid = cache_get(g_cache, &key, sizeof(key));
    if (grid) return grid;
    grid = calloc(n * n, sizeof(*grid));
    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec4_set(p, uv[0][0], uv[0][1], 0, 1);
        vec2_addk(p, duvx, (double)j / split, p);
        vec2_addk(p, duvy, (double)i / split, p);
        project(&proj, PROJ_BACKWARD, 4, p, p);
        grid[i * n + j][0] = p[0];
        grid[i * n + j][1] = p[1];
        grid[i * n + j][2] = p[2];
    }
    cache_add(g_cache, &key, sizeof(key), grid, sizeof(*grid) * n * n, NULL);
    return grid;
}
