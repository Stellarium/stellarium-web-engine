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

// Test if a shape in clipping coordinates is clipped or not.
static bool is_clipped(int n, double (*pos)[4])
{
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    int i, p;
    for (p = 0; p < 6; p++) {
        for (i = 0; i < n; i++) {
            if (    P[p][0] * pos[i][0] +
                    P[p][1] * pos[i][1] +
                    P[p][2] * pos[i][2] +
                    P[p][3] * pos[i][3] <= 0) {
                break;
            }
        }
        if (i == n) // All the points are outside a clipping plane.
            return true;
    }
    return false;
}

static void line_get_normal(const double (*line)[3], int size, int i,
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

line_mesh_t *line_to_mesh(const double (*line)[3],
                          const double (*win)[3],
                          int size, double width)
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
        if (i > 0) length += vec2_dist(win[i - 1], win[i]);
        line_get_normal(win, size, i, n);
        vec2_addk(win[i], n, -width / 2, v);
        vec2_to_float(v, mesh->verts[i * 2 + 0].win);
        vec2_addk(win[i], n, width / 2, v);
        vec2_to_float(v, mesh->verts[i * 2 + 1].win);
        vec3_to_float(line[i], mesh->verts[i * 2 + 0].pos);
        vec3_to_float(line[i], mesh->verts[i * 2 + 1].pos);
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
    return fabs(vec2_cross(ap, u)) / vec2_norm(u);
}

static void line_push_point(double (**pos)[3], double (**win)[3],
                            const double p[3], const double w[3],
                            int *size, int *allocated)
{
    if (*size <= *allocated) {
        *allocated += 4;
        *pos = realloc(*pos, *allocated * sizeof(**pos));
        *win = realloc(*win, *allocated * sizeof(**win));
    }
    memcpy((*pos)[*size], p, sizeof(**pos));
    memcpy((*win)[*size], w, sizeof(**win));
    (*size)++;
}

static void clip_to_win(const projection_t *proj,
                        const double pos[4], double out[3])
{
    double p[4];
    vec4_copy(pos, p);
    vec3_mul(1.0 / p[3], p, p);
    out[0] = (+p[0] + 1) / 2 * proj->window_size[0];
    out[1] = (-p[1] + 1) / 2 * proj->window_size[1];
    out[2] = (p[2] + 1) / 2;
}

static void line_tesselate_(void (*func)(void *user, double t, double pos[3]),
                            const projection_t *proj,
                            void *user, double t0, double t1,
                            double (**out_pos)[3],
                            double (**out_win)[3],
                            int level, int min_level,
                            int *size, int *allocated)
{
    double p0[3], p1[3], pm[3], c[3][4], w0[3], w1[3], wm[3], tm;
    double max_dist = 0.5;
    const int max_level = 5 + min_level;
    bool clipped;

    tm = (t0 + t1) / 2;

    if (level < min_level) goto split;

    func(user, t0, p0);
    func(user, t1, p1);
    func(user, tm, pm);

    project_to_clip(proj, p0, c[0]);
    project_to_clip(proj, p1, c[1]);
    project_to_clip(proj, pm, c[2]);

    clipped = is_clipped(3, c);

    clip_to_win(proj, c[0], w0);
    clip_to_win(proj, c[1], w1);
    clip_to_win(proj, c[2], wm);

    if (    clipped || level > max_level ||
            line_point_dist(w0, w1, wm) < max_dist)
    {
        line_push_point(out_pos, out_win, p1, w1, size, allocated);
        return;
    }

split:
    line_tesselate_(func, proj, user, t0, tm, out_pos, out_win, level + 1,
                    min_level, size, allocated);
    line_tesselate_(func, proj, user, tm, t1, out_pos, out_win, level + 1,
                    min_level, size, allocated);
}


int line_tesselate(void (*func)(void *user, double t, double pos[3]),
                   const projection_t *proj,
                   void *user, int split,
                   double (**out_pos)[3],
                   double (**out_win)[3])
{
    int i, allocated = 0, size = 0, min_level;
    double pos[3], win[3];
    *out_pos = NULL;
    *out_win = NULL;

    if (split > 0) {
        size = split + 1;
        *out_pos = calloc(size, sizeof(**out_pos));
        *out_win = calloc(size, sizeof(**out_win));
        for (i = 0; i < size; i++) {
            func(user, (double)i / split, pos);
            project_to_win(proj, pos, win);
            vec3_copy(pos, (*out_pos)[i]);
            vec3_copy(win, (*out_win)[i]);
        }
    } else {
        min_level = -split;
        func(user, 0, pos);
        project_to_win(proj, pos, win);
        line_push_point(out_pos, out_win, pos, win, &size, &allocated);
        line_tesselate_(func, proj, user, 0, 1, out_pos, out_win, 0,
                        min_level, &size, &allocated);
    }
    return size;
}
