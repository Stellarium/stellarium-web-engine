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
    free(mesh->points);
    free(mesh);
}

mesh_t *mesh_copy(const mesh_t *mesh)
{
    mesh_t *ret = calloc(1, sizeof(*ret));
    memcpy(ret->bounding_cap, mesh->bounding_cap, sizeof(mesh->bounding_cap));
    ret->vertices_count = mesh->vertices_count;
    ret->triangles_count = mesh->triangles_count;
    ret->lines_count = mesh->lines_count;
    ret->vertices = malloc(ret->vertices_count * sizeof(*ret->vertices));
    memcpy(ret->vertices, mesh->vertices,
           ret->vertices_count * sizeof(*ret->vertices));
    ret->triangles = malloc(ret->triangles_count * sizeof(*ret->triangles));
    memcpy(ret->triangles, mesh->triangles,
           ret->triangles_count * sizeof(*ret->triangles));
    ret->lines = malloc(ret->lines_count * sizeof(*ret->lines));
    memcpy(ret->lines, mesh->lines, ret->lines_count * sizeof(*ret->lines));
    return ret;
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


int mesh_add_vertices_lonlat(mesh_t *mesh, int count, double (*verts)[2])
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
    // XXX: shouldn't be done here!
    compute_bounding_cap(mesh->vertices_count, mesh->vertices,
                         mesh->bounding_cap);
    return ofs;
}

int mesh_add_vertices(mesh_t *mesh, int count, double (*verts)[3])
{
    int ofs;
    ofs = mesh->vertices_count;
    mesh->vertices = realloc(mesh->vertices,
            (mesh->vertices_count + count) * sizeof(*mesh->vertices));
    memcpy(mesh->vertices + mesh->vertices_count, verts,
           count * sizeof(*mesh->vertices));
    mesh->vertices_count += count;
    // XXX: shouldn't be done here!
    compute_bounding_cap(mesh->vertices_count, mesh->vertices,
                         mesh->bounding_cap);
    return ofs;
}

void mesh_add_line(mesh_t *mesh, int ofs, int size, bool loop)
{
    int i, nb_lines;
    nb_lines = (size - 1) + (loop ? 1 : 0);
    mesh->lines = realloc(mesh->lines, (mesh->lines_count + nb_lines * 2) *
                          sizeof(*mesh->lines));
    for (i = 0; i < nb_lines; i++) {
        mesh->lines[mesh->lines_count + i * 2 + 0] = ofs + (i + 0) % size;
        mesh->lines[mesh->lines_count + i * 2 + 1] = ofs + (i + 1) % size;
    }
    mesh->lines_count += nb_lines * 2;
}

void mesh_add_point(mesh_t *mesh, int ofs)
{
    mesh->points = realloc(mesh->points,
            (mesh->points_count + 1) * sizeof(*mesh->points));
    mesh->points[mesh->points_count] = ofs;
    mesh->points_count += 1;
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
            mat3_mul_vec3(rot, mesh->vertices[ofs + j++], p);
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

static void mesh_add_triangle(mesh_t *mesh, int a, int b, int c)
{
    mesh->triangles = realloc(mesh->triangles,
                                (mesh->triangles_count + 3) *
                                sizeof(*mesh->triangles));
    mesh->triangles[mesh->triangles_count + 0] = a;
    mesh->triangles[mesh->triangles_count + 1] = b;
    mesh->triangles[mesh->triangles_count + 2] = c;
    mesh->triangles_count += 3;
}

static void mesh_add_segment(mesh_t *mesh, int a, int b)
{
    mesh->lines = realloc(mesh->lines,
                          (mesh->lines_count + 2) *
                          sizeof(*mesh->lines));
    mesh->lines[mesh->lines_count + 0] = a;
    mesh->lines[mesh->lines_count + 1] = b;
    mesh->lines_count += 2;
}

static bool segment_intersects_antimeridian(
        const double a[3], const double b[3], double o[3])
{
    if (a[2] < 0 && b[2] < 0) return false; // Both in front of us.
    if (a[0] * b[0] >= 0) return false;
    vec3_mix(a, b, a[0] / (a[0] - b[0]), o);
    vec3_normalize(o, o);
    return true;
}

static void mesh_cut_triangle_antimeridian(mesh_t *mesh, int idx)
{
    int i, a, b, c, ofs, ab1, ab2, ac1, ac2;
    const double (*vs)[3] = mesh->vertices;
    double ab[3], ac[3], new_points[4][3];
    for (i = 0; i < 3; i++) {
        a = mesh->triangles[idx + i];
        b = mesh->triangles[idx + (i + 1) % 3];
        c = mesh->triangles[idx + (i + 2) % 3];
        if (    segment_intersects_antimeridian(vs[a], vs[b], ab) &&
                segment_intersects_antimeridian(vs[a], vs[c], ac))
            break;
    }
    if (i == 3) return;

    // We add a small gap around the cut, to avoid rendering problems.
    vec3_mix(vs[a], ab, 0.99, new_points[0]); // AB1
    vec3_mix(vs[b], ab, 0.99, new_points[1]); // AB2
    vec3_mix(vs[a], ac, 0.99, new_points[2]); // AC1
    vec3_mix(vs[c], ac, 0.99, new_points[3]); // AC2

    ofs = mesh_add_vertices(mesh, 4, new_points);
    ab1 = ofs + 0;
    ab2 = ofs + 1;
    ac1 = ofs + 2;
    ac2 = ofs + 3;

    // A,B,C -> A, AB1, AC1
    mesh->triangles[idx + (i + 1) % 3] = ab1;
    mesh->triangles[idx + (i + 2) % 3] = ac1;
    mesh_add_triangle(mesh, ab2, b, ac2);
    mesh_add_triangle(mesh, b, c, ac2);
}

/*
 * Function: mesh_cut_antimeridian
 * Split the mesh so that no triangle intersects the YZ plan
 *
 * Experimental.  Probably going to change to something more generic.
 */
void mesh_cut_antimeridian(mesh_t *mesh)
{
    int i, count;
    count = mesh->triangles_count;
    for (i = 0; i < count; i += 3) {
        mesh_cut_triangle_antimeridian(mesh, i);
    }
}

static void mesh_subdivide_edge(mesh_t *mesh, int e1, int e2)
{
    const double (*vs)[3];
    double new_point[3];
    int count, i, j, a, b, c, o;

    vs = mesh->vertices;
    vec3_mix(vs[e1], vs[e2], 0.5, new_point);
    // vec3_normalize(new_point, new_point);
    o = mesh_add_vertices(mesh, 1, &new_point);

    count = mesh->triangles_count;
    for (i = 0; i < count; i += 3) {
        for (j = 0; j < 3; j++) {
            a = mesh->triangles[i + (j + 0) % 3];
            b = mesh->triangles[i + (j + 1) % 3];
            c = mesh->triangles[i + (j + 2) % 3];
            if ((b == e1 && c == e2) || (b == e2 && c == e1)) {
                mesh->triangles[i + (j + 2) % 3] = o;
                assert(!(a == e2 && c == e1));
                mesh_add_triangle(mesh, a, o, c);
                break;
            }
        }
    }

    count = mesh->lines_count;
    for (i = 0; i < count; i += 2) {
        for (j = 0; j < 2; j++) {
            a = mesh->lines[i + (j + 0) % 2];
            b = mesh->lines[i + (j + 1) % 2];
            if (a == e1 && b == e2) {
                mesh->lines[i + (j + 1) % 2] = o;
                mesh_add_segment(mesh, o, b);
                break;
            }
        }
    }
}

static void mesh_subdivide_triangle(mesh_t *mesh, int idx, double max_length)
{
    double sides[3];
    const double (*vs)[3];
    int i;

    while (true) {
        // Compute all sides lengths (squared).
        vs = mesh->vertices;
        for (i = 0; i < 3; i++) {
            sides[i] = vec3_dist2(vs[mesh->triangles[idx + (i + 1) % 3]],
                                  vs[mesh->triangles[idx + (i + 2) % 3]]);
        }
        // Get largest side.
        for (i = 0; i < 3; i++) {
            if (    sides[i] >= sides[(i + 1) % 3] &&
                    sides[i] >= sides[(i + 2) % 3])
                break;
        }
        assert(i < 3);
        if (sides[i] < max_length * max_length)
            return;

        mesh_subdivide_edge(mesh, mesh->triangles[idx + (i + 1) % 3],
                                  mesh->triangles[idx + (i + 2) % 3]);
    }
}

/*
 * Function: mesh_subdivide
 * Subdivide edges that are larger than a given length.
 */
void mesh_subdivide(mesh_t *mesh, double max_length)
{
    int i;
    for (i = 0; i < mesh->triangles_count; i += 3) {
        mesh_subdivide_triangle(mesh, i, max_length);
    }
}
