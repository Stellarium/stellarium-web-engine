/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "line_mesh.h"

#include "utils/vec.h"

#include <float.h>
#include <stdlib.h>

static void line_get_normal(const double (*line)[2], int size, int i,
                            double n[2])
{
    double seg_before[2] = {}, seg_after[2] = {}, seg[2];
    int nb_segs = 0;
    if (i > 0) {
        vec2_sub(line[i], line[i - 1], seg_before);
        nb_segs++;
    }
    if (i < size - 1) {
        vec2_sub(line[i + 1], line[i], seg_after);
        nb_segs++;
    }
    vec2_add(seg_before, seg_after, seg);
    vec2_mul(1. / nb_segs, seg, seg);

    // 90deg rotation to get the normal.
    n[0] = -seg[1];
    n[1] = seg[0];

    if (vec2_norm2(n) > DBL_MIN)
        vec2_normalize(n, n);
}

line_mesh_t *line_to_mesh(const double (*line)[2], int size, double width)
{
    int i, k;
    double n[2], v[2], length = 0;
    line_mesh_t *mesh = calloc(1, sizeof(*mesh));

    assert(size >= 2);

    mesh->verts_count = size * 2;
    mesh->verts = calloc(mesh->verts_count, sizeof(*mesh->verts));
    mesh->indices_count = 6 * (size - 1);
    mesh->indices = calloc(mesh->indices_count, sizeof(*mesh->indices));

    // Compute all vertices.
    for (i = 0; i < size; i++) {
        if (i > 0) length += vec2_dist(line[i - 1], line[i]);
        line_get_normal(line, size, i, n);
        vec2_addk(line[i], n, -width / 2, v);
        vec2_to_float(v, mesh->verts[i * 2 + 0].pos);
        vec2_addk(line[i], n, width / 2, v);
        vec2_to_float(v, mesh->verts[i * 2 + 1].pos);
        vec2_set(mesh->verts[i * 2 + 0].uv, length, -width / 2);
        vec2_set(mesh->verts[i * 2 + 1].uv, length, +width / 2);
    }
    // Compute all indices.
    for (i = 0; i < size - 1; i++) {
        for (k = 0; k < 6; k++) {
            mesh->indices[i * 6 + k] = (int[]){0, 1, 2, 3, 2, 1}[k] + i * 2;
        }
    }

    return mesh;
}

void line_mesh_delete(line_mesh_t *mesh)
{
    free(mesh->indices);
    free(mesh->verts);
    free(mesh);
}

static double line_point_dist(const double a[2], const double b[2],
                               const double p[2])
{
    // XXX: could be optimized to avoid the norm.
    double ap[2], u[2];
    vec2_sub(p, a, ap);
    vec2_sub(b, a, u);
    return vec2_cross(ap, u) / vec2_norm(u);
}

static void line_push_point(double (**line)[2], const double p[2],
                            int *size, int *allocated)
{
    if (*size <= *allocated) {
        *allocated += 4;
        *line = realloc(*line, *allocated * sizeof(**line));
    }
    memcpy((*line)[(*size)++], p, sizeof(**line));
}

static void line_tesselate_(void (*func)(void *user, double t, double pos[4]),
                            const projection_t *proj,
                            void *user, double t0, double t1,
                            double (**out)[2],
                            int level, int *size, int *allocated)
{
    double p0[4], p1[4], pm[4], tm;
    const double max_dist = 1.0;
    const int max_level = 4;
    tm = (t0 + t1) / 2;

    func(user, t0, p0);
    func(user, t1, p1);
    func(user, tm, pm);

    project(proj, PROJ_TO_WINDOW_SPACE, p0, p0);
    project(proj, PROJ_TO_WINDOW_SPACE, p1, p1);
    project(proj, PROJ_TO_WINDOW_SPACE, pm, pm);

    if (level > max_level || line_point_dist(p0, p1, pm) < max_dist) {
        line_push_point(out, p1, size, allocated);
        return;
    }

    line_tesselate_(func, proj, user, t0, tm, out, level + 1, size, allocated);
    line_tesselate_(func, proj, user, tm, t1, out, level + 1, size, allocated);
}


int line_tesselate(void (*func)(void *user, double t, double pos[4]),
                   const projection_t *proj,
                   void *user, int split, double (**out)[2])
{
    int i, allocated = 0, size = 0;
    *out = NULL;
    double p[4];

    if (split) {
        size = split + 1;
        *out = calloc(size, sizeof(**out));
        for (i = 0; i < size; i++) {
            func(user, (double)i / split, p);
            project(proj, PROJ_TO_WINDOW_SPACE, p, p);
            vec2_copy(p, (*out)[i]);
        }
    } else {
        func(user, 0, p);
        project(proj, PROJ_TO_WINDOW_SPACE, p, p);
        line_push_point(out, p, &size, &allocated);
        line_tesselate_(func, proj, user, 0, 1, out, 0, &size, &allocated);
    }
    return size;
}
