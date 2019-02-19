/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * Function: grid_cache_get
 * Return a pointer to an healpix grid for fast texture projection
 *
 * Parameters:
 *   nside      - Healpix pixel nside argument.
 *   pix        - Healpix pix.
 *   mat        - UV coordinates of a quad inside the healpix pixel.
 *   split      - Number of splits to use for the grid.
 *
 * Return:
 *   A (split + 1)^2 grid of 3d positions.
 */
const double (*grid_cache_get(int nside, int pix,
                              const double mat[3][3],
                              int split))[3];
