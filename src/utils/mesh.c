/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "mesh.h"
#include "earcut.h" // XXX: move into utils.
#include "vec.h"
#include "erfa.h" // XXX: to remove, we barely use it here.

#include <float.h>
#include <stdlib.h>

/* Radians to degrees */
#define DR2D (57.29577951308232087679815)
/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

static double min(double x, double y)
{
    return x < y ? x : y;
}

mesh_t *mesh_create(void)
{
    return calloc(1, sizeof(mesh_t));
}

void mesh_delete(mesh_t *mesh)
{
    free(mesh->vertices);
    free(mesh->triangles);
    free(mesh->lines);
    free(mesh);
}

// XXX: naive algo.
static void compute_bounding_cap(int size, const double (*verts)[3],
                                 double cap[4])
{
    int i;
    vec4_set(cap, 0, 0, 0, 1);
    for (i = 0; i < size; i++) {
        vec3_add(cap, verts[i], cap);
    }
    vec3_normalize(cap, cap);

    for (i = 0; i < size; i++) {
        cap[3] = min(cap[3], vec3_dot(cap, verts[i]));
    }
}

static void c2lonlat(const double c[3], double lonlat[2])
{
    double lon, lat;
    eraC2s(c, &lon, &lat);
    lonlat[0] = -lon * DR2D;
    lonlat[1] = lat * DR2D;
}

static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(lonlat[0] * DD2R, lonlat[1] * DD2R, c);
}


int mesh_add_vertices(mesh_t *mesh, int count, double (*verts)[2])
{
    int i, ofs;
    ofs = mesh->vertices_count;
    mesh->vertices = realloc(mesh->vertices,
            (mesh->vertices_count + count) * sizeof(*mesh->vertices));
    for (i = 0; i < count; i++) {
        assert(!isnan(verts[i][0]));
        lonlat2c(verts[i], mesh->vertices[mesh->vertices_count + i]);
        assert(!isnan(mesh->vertices[mesh->vertices_count + i][0]));
    }
    mesh->vertices_count += count;
    compute_bounding_cap(mesh->vertices_count, mesh->vertices,
                         mesh->bounding_cap);
    return ofs;
}

void mesh_add_line(mesh_t *mesh, int ofs, int size)
{
    int i;
    mesh->lines = realloc(mesh->lines, (mesh->lines_count + (size - 1) * 2) *
                          sizeof(*mesh->lines));
    for (i = 0; i < size - 1; i++) {
        mesh->lines[mesh->lines_count + i * 2 + 0] = ofs + i;
        mesh->lines[mesh->lines_count + i * 2 + 1] = ofs + i + 1;
    }
    mesh->lines_count += (size - 1) * 2;
}

// Should be in vec.h I guess, but we use eraSepp, so it's not conveniant.
static void create_rotation_between_vecs(
        double rot[3][3], const double a[3], const double b[3])
{
    double angle = eraSepp(a, b);
    double axis[3];
    double quat[4];
    if (angle < FLT_EPSILON) {
        mat3_set_identity(rot);
        return;
    }
    if (fabs(angle - M_PI) > FLT_EPSILON) {
        vec3_cross(a, b, axis);
    } else {
        vec3_get_ortho(a, axis);
    }
    quat_from_axis(quat, angle, axis[0], axis[1], axis[2]);
    quat_to_mat3(quat, rot);
}


void mesh_add_poly(mesh_t *mesh, int nb_rings, const int ofs, const int *size)
{
    int r, i, j = 0, triangles_size;
    double rot[3][3], p[3];
    double (*centered_lonlat)[2];
    const uint16_t *triangles;
    earcut_t *earcut;

    earcut = earcut_new();
    // Triangulate the shape.
    // First we rotate the points so that they are centered around the
    // origin.
    create_rotation_between_vecs(rot, mesh->bounding_cap, VEC(1, 0, 0));

    for (r = 0; r < nb_rings; r++) {
        centered_lonlat = calloc(size[r], sizeof(*centered_lonlat));
        for (i = 0; i < size[r]; i++) {
            mat3_mul_vec3(rot, mesh->vertices[j++], p);
            c2lonlat(p, centered_lonlat[i]);
        }
        earcut_add_poly(earcut, size[r], centered_lonlat);
        free(centered_lonlat);
    }

    triangles = earcut_triangulate(earcut, &triangles_size);
    mesh->triangles = realloc(mesh->triangles,
            (mesh->triangles_count + triangles_size) *
            sizeof(*mesh->triangles));
    for (i = 0; i < triangles_size; i++) {
        mesh->triangles[mesh->triangles_count + i] = ofs + triangles[i];
    }
    mesh->triangles_count += triangles_size;
    earcut_delete(earcut);
}


// Spherical triangle / point intersection.
static bool triangle_contains_vec3(const double verts[][3],
                                   const uint16_t indices[],
                                   const double pos[3])
{
    int i;
    double u[3];
    for (i = 0; i < 3; i++) {
        vec3_cross(verts[indices[i]], verts[indices[(i + 1) % 3]], u);
        if (vec3_dot(u, pos) > 0) return false;
    }
    return true;
}

/*
 * Function: mesh_contains_vec3
 * Test if a 3d direction vector intersects a 3d mesh.
 */
bool mesh_contains_vec3(const mesh_t *mesh, const double pos[3])
{
    int i;
    if (!cap_contains_vec3(mesh->bounding_cap, pos))
        return false;
    for (i = 0; i < mesh->triangles_count; i += 3) {
        if (triangle_contains_vec3(mesh->vertices, mesh->triangles + i, pos))
            return true;
    }
    return false;
}

