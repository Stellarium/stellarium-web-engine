/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "mesh.h"
#include "vec.h"
#include "erfa.h" // XXX: to remove, we barely use it here.

#include "../../ext_src/libtess2/tesselator.h"

#include <float.h>
#include <stdlib.h>

/* Radians to degrees */
#define DR2D (57.29577951308232087679815)
/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

#define SWAP(a, b) ({typeof(a) tmp_ = a; a = b; b = tmp_;})

static double min(double x, double y)
{
    return x < y ? x : y;
}

static double max(double x, double y)
{
    return x > y ? x : y;
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

void mesh_update_bounding_cap(mesh_t *mesh)
{
    // XXX: naive algo, could be improved.
    int i;
    double cap[4] = {0, 0, 0, 1};
    for (i = 0; i < mesh->vertices_count; i++) {
        vec3_add(cap, mesh->vertices[i], cap);
    }
    vec3_normalize(cap, cap);

    for (i = 0; i < mesh->vertices_count; i++) {
        cap[3] = min(cap[3], vec3_dot(cap, mesh->vertices[i]));
    }
    vec4_copy(cap, mesh->bounding_cap);
}

static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(lonlat[0] * DD2R, lonlat[1] * DD2R, c);
}


static int mesh_add_vertices_lonlat(mesh_t *mesh, int count,
                                    const double (*verts)[2])
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
    return ofs;
}

void mesh_add_line_lonlat(mesh_t *mesh, int size, const double (*verts)[2],
                          bool loop)
{
    int ofs, i, nb_lines;
    ofs = mesh_add_vertices_lonlat(mesh, size, verts);
    nb_lines = (size - 1) + (loop ? 1 : 0);
    mesh->lines = realloc(mesh->lines, (mesh->lines_count + nb_lines * 2) *
                          sizeof(*mesh->lines));
    for (i = 0; i < nb_lines; i++) {
        mesh->lines[mesh->lines_count + i * 2 + 0] = ofs + (i + 0) % size;
        mesh->lines[mesh->lines_count + i * 2 + 1] = ofs + (i + 1) % size;
    }
    mesh->lines_count += nb_lines * 2;
}

void mesh_add_point_lonlat(mesh_t *mesh, const double vert[2])
{
    int ofs;
    ofs = mesh_add_vertices_lonlat(mesh, 1, (void*)vert);
    mesh->points = realloc(mesh->points,
            (mesh->points_count + 1) * sizeof(*mesh->points));
    mesh->points[mesh->points_count] = ofs;
    mesh->points_count += 1;
}

// Ensure all the triangles culling is correct.
static void mesh_fix_triangles_culling(mesh_t *mesh)
{
    int i;
    double u[3];
    const double (*vs)[3] = mesh->vertices;
    for (i = 0; i < mesh->triangles_count; i += 3) {
        vec3_cross(vs[mesh->triangles[i]], vs[mesh->triangles[i + 1]], u);
        if (vec3_dot(u, vs[mesh->triangles[i + 2]]) > 0) {
            SWAP(mesh->triangles[i + 1], mesh->triangles[i + 2]);
        }
    }
}

void mesh_add_poly_lonlat(mesh_t *mesh, int nbrings, const int *rings_size,
                          const double (**verts)[2])
{
    int r, i, j, ofs, verts_count, nb_triangles;
    double (*ring)[3];
    double (*new_verts)[3];
    const int *triangles;
    TESStesselator *tess;

    tess = tessNewTess(NULL);
    tessSetOption(tess, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);
    for (r = 0; r < nbrings; r++) {
        ring = calloc(rings_size[r], sizeof(*ring));
        for (i = 0; i < rings_size[r]; i++) {
            lonlat2c(verts[r][i], ring[i]);
        }
        tessAddContour(tess, 3, ring, 24, rings_size[r]);
        free(ring);
    }
    r = tessTesselate(tess, TESS_WINDING_NONZERO, TESS_CONNECTED_POLYGONS,
                      3, 3, NULL);
    if (r == 0) {
        LOG_E("Tesselation error");
        assert(false);
        return;
    }

    verts_count = tessGetVertexCount(tess);
    new_verts = (void*)tessGetVertices(tess);
    nb_triangles = tessGetElementCount(tess);
    triangles = tessGetElements(tess);

    ofs = mesh_add_vertices(mesh, verts_count, new_verts);

    // Add the triangles and lines.
    mesh->triangles = realloc(mesh->triangles,
            (mesh->triangles_count + nb_triangles * 3) *
            sizeof(*mesh->triangles));
    for (i = 0; i < nb_triangles; i++) {
        for (j = 0; j < 3; j++) {
            mesh->triangles[mesh->triangles_count + i * 3 + j] =
                triangles[i * 6 + j] + ofs;

            if (triangles[i * 6 + 3 + j] == TESS_UNDEF) {
                mesh->lines = realloc(mesh->lines, (mesh->lines_count + 2) *
                                      sizeof(*mesh->lines));
                mesh->lines[mesh->lines_count + 0] =
                    triangles[i * 6 + (j + 0) % 3] + ofs;
                mesh->lines[mesh->lines_count + 1] =
                    triangles[i * 6 + (j + 1) % 3] + ofs;
                mesh->lines_count += 2;
            }
        }
    }
    mesh->triangles_count += nb_triangles * 3;

    tessDeleteTess(tess);

    // For testing.  We want to avoid meshes with too long edges
    // for the distortion.
    r = mesh_subdivide(mesh, M_PI / 8);
    if (r) mesh->subdivided = true;

    // Not sure if we should instead assume the culling is always correct.
    mesh_fix_triangles_culling(mesh);
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

static void mesh_cut_segment_antimeridian(mesh_t *mesh, int idx)
{
    const double (*vs)[3] = mesh->vertices;
    double ab[3], new_points[2][3];
    int a, b, ofs;
    a = mesh->lines[idx];
    b = mesh->lines[idx + 1];
    if (!segment_intersects_antimeridian(vs[a], vs[b], ab))
        return;
    // We add a small gap around the cut, to avoid rendering problems.
    vec3_mix(vs[a], ab, 0.99, new_points[0]);
    vec3_mix(vs[b], ab, 0.99, new_points[1]);
    ofs = mesh_add_vertices(mesh, 2, new_points);
    mesh->lines[idx + 1] = ofs;
    mesh_add_segment(mesh, ofs + 1, b);
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
    count = mesh->lines_count;
    for (i = 0; i < count; i += 2) {
        mesh_cut_segment_antimeridian(mesh, i);
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

static int mesh_subdivide_triangle(mesh_t *mesh, int idx, double max_length)
{
    double sides[3];
    const double (*vs)[3];
    int i, ret = 0;

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
            break;

        mesh_subdivide_edge(mesh, mesh->triangles[idx + (i + 1) % 3],
                                  mesh->triangles[idx + (i + 2) % 3]);
        ret++;
    }
    return ret;
}

/*
 * Function: mesh_subdivide
 * Subdivide edges that are larger than a given length.
 *
 * Return the number of edges that got cut.
 */
int mesh_subdivide(mesh_t *mesh, double max_length)
{
    int i, ret = 0;
    for (i = 0; i < mesh->triangles_count; i += 3) {
        ret += mesh_subdivide_triangle(mesh, i, max_length);
    }
    return ret;
}

static bool segment_intersects_2d_box(const double a[2], const double b[2],
                                      const double box[2][2])
{
    const double n[2] = {b[0] - a[0], b[1] - a[1]};
    double tx1, tx2, ty1, ty2;
    double txmin = -DBL_MAX, txmax = +DBL_MAX;
    double tymin = -DBL_MAX, tymax = +DBL_MAX;
    double vmin, vmax;

    if (n[0] != 0.0) {
        tx1 = (box[0][0] - a[0]) / n[0];
        tx2 = (box[1][0] - a[0]) / n[0];
        txmin = min(tx1, tx2);
        txmax = max(tx1, tx2);
    }

    if (n[1] != 0.0) {
        ty1 = (box[0][1] - a[1]) / n[1];
        ty2 = (box[0][1] - a[1]) / n[1];
        tymin = min(ty1, ty2);
        tymax = max(ty1, ty2);
    }
    if (tymin <= txmax && txmin <= tymax) {
        vmin = max(txmin, tymin);
        vmax = min(txmax, tymax);
        if (0.0 <= vmax && vmin <= 1.0) {
            return true;
        }
    }
    return false;
}

static bool triangle_intersects_2d_box(const double tri[3][2],
                                       const double box[2][2])
{
    double a[2], b[2];
    int i;

    // Test if any point is inside.
    for (i = 0; i < 3; i++) {
        if (    (tri[i][0] >= box[0][0] && tri[i][0] < box[1][0] &&
                 tri[i][1] >= box[0][1] && tri[i][1] < box[1][1]))
            return true;
    }

    // Return whether a segment intersects the box.
    for (i = 0; i < 3; i++) {
        vec2_copy(tri[i], a);
        vec2_copy(tri[(i + 1) % 3], b);
        if (segment_intersects_2d_box(a, b, box))
            return true;
    }
    return false;
}

bool mesh_intersects_2d_box(const mesh_t *mesh, const double box[2][2])
{
    int i, j;
    double tri[3][2];
    for (i = 0; i < mesh->triangles_count; i += 3) {
        for (j = 0; j < 3; j++)
            vec2_copy(mesh->vertices[mesh->triangles[i + j]], tri[j]);
        if (triangle_intersects_2d_box(tri, box))
            return true;
    }
    return false;
}
