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
    double      bounding_cap[4]; // Not automatically updated.
    int         vertices_count;
    double      (*vertices)[3];

    int         triangles_count; // Number of triangles * 3.
    uint16_t    *triangles;

    int         lines_count; // Number of lines * 2.
    uint16_t    *lines;

    int         points_count; // Number of points.
    uint16_t    *points;

    bool        subdivided; // Set if the mesh was subdivided.
};

mesh_t *mesh_create(void);
void mesh_delete(mesh_t *mesh);
mesh_t *mesh_copy(const mesh_t *mesh);

void mesh_add_line_lonlat(mesh_t *mesh, int size, const double (*verts)[2],
                          bool loop);
void mesh_add_point_lonlat(mesh_t *mesh, const double vert[2]);
void mesh_add_poly_lonlat(mesh_t *mesh, int nbrings, const int *rings_size,
                          const double (**verts)[2]);

/*
 * Function: mesh_update_bounding_cap
 * Recompute the mesh bounding_cap value.
 *
 * Should be called only after we know we will not add anymore vertices
 * to the mesh.
 */
void mesh_update_bounding_cap(mesh_t *mesh);

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
 *
 * Return the number of edges that got cut.
 */
int mesh_subdivide(mesh_t *mesh, double max_length);

bool mesh_intersects_2d_box(const mesh_t *mesh, const double box[2][2]);

#endif // MESH_H
