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

void uv_map(const uv_map_t *map, const double v[2], double out[4],
            double normal[3])
{
    map->map(map, v, out);
    if (normal) vec3_copy(out, normal);
    if (map->transf) {
        mat4_mul_vec4(*map->transf, out, out);
        if (normal) mat4_mul_dir3(*map->transf, normal, normal);
    }
    if (normal) vec3_normalize(normal, normal);
}

/*
 * Function: uv_map_grid
 * Compute the mapped position of a 2d grid covering the mapping.
 *
 * Parameters:
 *   map        - The mapping function used.
 *   size       - Size of the side of the grid.  The number of vertices
 *                computed is (size + 1)^2.
 *   out        - Output of all the mapped vertices.
 *   normals    - If set, output of all the mapped vertices normals.
 */
void uv_map_grid(const uv_map_t *map, int size,
                 double (*out)[4], double (*normals)[3])
{
    int i, j;
    double uv[2];

    for (i = 0; i < size + 1; i++)
    for (j = 0; j < size + 1; j++) {
        uv[0] = (double)j / size;
        uv[1] = (double)i / size;
        uv_map(map, uv, out[i * (size + 1) + j],
               normals ? normals[i * (size + 1) + j] : NULL);
    }
}

void uv_map_get_bounding_cap(const uv_map_t *map, double out[4])
{
    double corners[4][4], d;
    int i;

    uv_map_grid(map, 1, corners, NULL);
    vec4_set(out, 0, 0, 0, 1);
    for (i = 0; i < 4; i++) {
        vec3_add(out, corners[i], out);
    }
    vec3_normalize(out, out);
    for (i = 0; i < 4; i++) {
        d = vec3_dot(out, corners[i]);
        if (d < out[3])
            out[3] = d;
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

/*
 * Function: uv_map_subdivide
 * Split the mapped shape into four smaller parts.
 *
 * Each child will map the full UV quad into 1/4th of the original mapping.
 */
void uv_map_subdivide(const uv_map_t *map, uv_map_t children[4])
{
    int i;
    // For the moment we only support it for healpix mapping!
    assert(map->type == UV_MAP_HEALPIX);
    for (i = 0; i < 4; i++) {
        uv_map_init_healpix(&children[i], map->order + 1, map->pix * 4 + i,
                            map->swapped, map->at_infinity);
        children[i].transf = map->transf;
    }
}
