/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "mesh2d.h"

#include <math.h>
#include <string.h>

static double max(double x, double y)
{
    return x > y ? x : y;
}

static double sqr(double x)
{
    return x * x;
}

// Util function to compute the barycenter of a triangle.
// Return the area of the triangle.
static double triangle_center(const float verts[][2],
                              const uint16_t indices[],
                              double out[2])
{
    int i;
    double a = 0;
    #define I(i) indices[(i) % 3]

    out[0] = out[1] = 0;
    for (i = 0; i < 3; i++) {
        out[0] += verts[I(i)][0];
        out[1] += verts[I(i)][1];
    }
    out[0] /= 3;
    out[1] /= 3;

    for (i = 0; i < 3; i++)
        a += 0.5 * verts[I(i)][0] * (verts[I(i + 1)][1] - verts[I(i + 2)][1]);

    #undef I
    return a;
}


/*
 * Function: mesh2d_get_bounding_circle
 * Compute the bounding circle of a mesh
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
        double pos[2], double *radius)
{
    int i;
    double r2 = 0, w, w_tot, tri_pos[2];

    pos[0] = pos[1] = 0;
    // Compute bounding circle.
    for (i = 0; i < count; i += 3) {
        w = triangle_center(verts, indices + i, tri_pos);
        pos[0] += w * tri_pos[0];
        pos[1] += w * tri_pos[1];
        w_tot += w;
    }
    pos[0] /= w_tot;
    pos[1] /= w_tot;
    for (i = 0; i < count; i++) {
        r2 = max(r2, sqr(verts[indices[i]][0] - pos[0]) +
                     sqr(verts[indices[i]][1] - pos[1]));
    }
    *radius = sqrt(r2);
}

static bool triangle_contains(const float verts[][2],
                              const uint16_t indices[],
                              const double pos[2])
{
    // Algo from:
    // https://stackoverflow.com/questions/2049582/
    //              how-to-determine-if-a-point-is-in-a-2d-triangle
    // There is probably a simpler way.
    float d1, d2, d3, v1[2], v2[2], v3[2];
    bool has_neg, has_pos;

    memcpy(v1, verts[indices[0]], sizeof(v1));
    memcpy(v2, verts[indices[1]], sizeof(v2));
    memcpy(v3, verts[indices[2]], sizeof(v3));

    #define sign(p1, p2, p3) \
        ((p1[0] - p3[0]) * (p2[1] - p3[1]) - (p2[0] - p3[0]) * (p1[1] - p3[1]))
    d1 = sign(pos, v1, v2);
    d2 = sign(pos, v2, v3);
    d3 = sign(pos, v3, v1);
    #undef sign

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}


/*
 * Function: mesh2d_contains_point
 * Check if a point is inside a triangle mesh
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
        const double pos[2])
{
    int i;
    for (i = 0; i < count; i += 3) {
        if (triangle_contains(verts, indices + i, pos)) {
            return true;
        }
    }
    return false;
}
