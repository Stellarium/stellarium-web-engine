/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef LINES_H
#define LINES_H

#include <stdint.h>

/*
 * File: line.h
 *
 * Some util functions to convert a line into a quad, for better opengl
 * rendering.
 */

/*
 * Struct: line_mesh_t
 * Contains the vertices and indices of a line mesh.
 */
typedef struct line_mesh
{
    struct {
        float pos[2];
        float uv[2];
    } *verts;
    uint16_t *indices;
    int indices_count;
    int verts_count;
} line_mesh_t;

/*
 * Function: line_to_mesh
 * Convert a line with a width into a quad mesh.
 *
 * Parameters:
 *   line   - Array of 2d coordinates.
 *   size   - Number of points in the line array.
 *   width  - width of the line.
 *
 * Return:
 *   A new <line_mesh_t> instance, that should be released with
 *   <line_mesh_delete>.
 */
line_mesh_t *line_to_mesh(const double (*line)[2], int size, double width);

/*
 * Function: line_mesh_delete
 * Delete a line mesh.
 */
void line_mesh_delete(line_mesh_t *mesh);

#endif // LINES_H
