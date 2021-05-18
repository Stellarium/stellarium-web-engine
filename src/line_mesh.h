/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef LINE_MESH_H
#define LINE_MESH_H

#include <stdint.h>

#include "projection.h"

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
        float pos[3];
        float win[2];
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
 *   line   - Array of 3d coordinates.
 *   win    - Pre projected window coordinates.
 *   size   - Number of points in the line array.
 *   width  - width of the line.
 *
 * Return:
 *   A new <line_mesh_t> instance, that should be released with
 *   <line_mesh_delete>.
 */
line_mesh_t *line_to_mesh(const double (*line)[3],
                          const double (*win)[3],
                          int size, double width);

/*
 * Function: line_mesh_delete
 * Delete a line mesh.
 */
void line_mesh_delete(line_mesh_t *mesh);

/*
 * Function: line_tesselate
 * Cut a parametric line into a list of points.
 *
 * Parameters:
 *   func   - Parametric line function.  The t argument ranges from 0 to 1.
 *            Return a 3d position in view coordinates.
 *   proj   - Screen projection.
 *   user   - User data passed to the function.
 *   split  - Number of segments requested in the output.  If < 0 use
 *            an adaptive algorithm, where -split is the minimum level
 *            of split.
 *   out_pos - Allocated out line points in view coordinates.
 *   out_win - Allocated out line points in windows coordinates.
 *
 * Return:
 *   The number of points in the line, or -1 in case or error (and out is not
 *   set).
 */
int line_tesselate(void (*func)(void *user, double t, double pos[3]),
                   const projection_t *proj,
                   void *user, int split,
                   double (**out_pos)[3],
                   double (**out_win)[3]);

#endif // LINE_MESH_H
