/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: mesh2d.h
 * Some util functions for 2d triangles meshes
 *
 * A mesh is a set of triangles, passed as a list of vertices positions
 * and a list of indices referencing the triangles.
 */

#ifndef MESH2D_H
#define MESH2D_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Function: mesh2d_get_bounding_circle
 * Compute the bounding circle of a triangles mesh
 *
 * Parameters:
 *   verts      - Array of vertices of the mesh.
 *   indices    - Array of indices of the triangles.
 *   count      - Number of indices.
 *   pos        - Output circle position.
 *   radius     - Output circle radius.
 */
void mesh2d_get_bounding_circle(
        const float verts[][2], const uint16_t indices[], int count,
        double pos[2], double *radius);


/*
 * Function: mesh2d_contains_point
 * Check if a point is inside a triangles mesh
 *
 * Parameters:
 *   verts      - Array of vertices of the mesh.
 *   indices    - Array of indices of the triangles.
 *   count      - Number of indices.
 *   pos        - Position of the 2d point to test.
 *
 * Return:
 *   True if the point is in the mesh.
 */
bool mesh2d_contains_point(
        const float verts[][2], const uint16_t indices[], int count,
        const double pos[2]);

#endif // MESH2D_H
