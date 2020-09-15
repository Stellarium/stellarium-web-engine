/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Struct: mesh_t
 * Represents a 3d triangle mesh, as used by geojson.
 */
typedef struct mesh mesh_t;
struct mesh {
    mesh_t      *next, *prev; // Can be used to put mesh inside geojson.
    double      bounding_cap[4];
    int         vertices_count;
    double      (*vertices)[3];

    int         triangles_count; // Number of triangles * 3.
    uint16_t    *triangles;

    int         lines_count; // Number of lines * 2.
    uint16_t    *lines;

    int         points_count; // Number of points.
    uint16_t    *points;
};

mesh_t *mesh_create(void);
void mesh_delete(mesh_t *mesh);
mesh_t *mesh_copy(const mesh_t *mesh);

int mesh_add_vertices(mesh_t *mesh, int count, double (*verts)[3]);
int mesh_add_vertices_lonlat(mesh_t *mesh, int count, double (*verts)[2]);
void mesh_add_line(mesh_t *mesh, int ofs, int size);
void mesh_add_poly(mesh_t *mesh, int nb_rings, const int ofs, const int *size);
void mesh_add_point(mesh_t *mesh, int ofs);

/*
 * Function: mesh_contains_vec3
 * Test if a 3d direction vector intersects a 3d mesh.
 */
bool mesh_contains_vec3(const mesh_t *mesh, const double pos[3]);

/*
 * Function: mesh_cut_antimeridian
 * Split the mesh so that no triangle intersects the YZ plan
 *
 * Experimental.  Probably going to change to something more generic.
 */
void mesh_cut_antimeridian(mesh_t *mesh);

/*
 * Function: mesh_subdivide
 * Subdivide edges that are larger than a given length.
 */
void mesh_subdivide(mesh_t *mesh, double max_length);

#endif // MESH_H
