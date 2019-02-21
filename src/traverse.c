/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static int enqueue(qtree_node_t *nodes, int n,
                   int *start, int *size,
                   qtree_node_t *node)
{
    if (n - 1 < *size) return -1;
    int i = (*start + *size) % n;
    nodes[i] = *node;
    (*size)++;
    return 0;
}

static void dequeue(qtree_node_t *nodes, int n,
                    int *start, int *size, qtree_node_t *node)
{
    (*size)--;
    *node = nodes[*start];
    *start = ((*start) + 1) % n;
}

static int qtree_traverse(qtree_node_t *nodes, int n, void *user,
                          int (*f)(qtree_node_t *node, void *user))
{
    int max_size = 0; // For debuging.
    int r, start = 0, size = 0;
    int s[2];
    qtree_node_t node = {0};
    int x, y, i, j;
    r = enqueue(nodes, n, &start, &size, &node);
    assert(size);
    if (r) return r;
    while (size) {
        dequeue(nodes, n, &start, &size, &node);
        s[0] = s[1] = 2;
        r = f(&node, user);
        if (r == 3) return 0;
        if (r) {
            x = node.x * 2;
            y = node.y * 2;
            node.level++;
            for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
                node.x = x + j;
                node.y = y + i;
                r = enqueue(nodes, n, &start, &size, &node);
                if (r) return r;
            }
        }
        max_size = max(max_size, size);
    }
    return 0;
}

// To simplify the code, I put all the constant attributes of split_quad
// into a struct.
typedef struct {
    double uv[4][2];
    void *user;
    const projection_t *proj;
    const painter_t *painter;
    int frame;
    int (*f)(int step,
             qtree_node_t *node,
             const double uv[4][2],
             const double pos[4][4],
             const double mat[3][3],
             const painter_t *painter,
             void *user);
} d_t;

static int on_node(qtree_node_t *node, void *user)
{
    d_t *d = user;
    double mat[3][3], uv[4][2];
    double pos[4][4], clip[4][4];
    double mid_pos[4], sep = 0;
    int i, r;

    // Compute mat and uv.
    mat3_set_identity(mat);
    mat3_iscale(mat, 1.0 / (1 << node->level), 1.0 / (1 << node->level), 1.0);
    mat3_itranslate(mat, node->x, node->y);
    for (i = 0; i < 4; i++) mat3_mul_vec2(mat, d->uv[i], uv[i]);

    r = d->f(0, node, d->uv, NULL, mat, d->painter, d->user);
    if (r != 2) return r;

    assert(d->painter);

    for (i = 0; i < 4; i++) {
        project(d->proj, PROJ_BACKWARD, 4, uv[i], pos[i]);
        mat4_mul_vec4(*d->painter->transform, pos[i], pos[i]);
        convert_framev4(d->painter->obs, d->frame, FRAME_VIEW,
                            pos[i], pos[i]);
        project(d->painter->proj, 0, 4, pos[i], clip[i]);
    }

    // Compute the angle of the quad.  We could optimize this, we don't
    // need to compute it for children of small quads.
    vec2_mix(uv[0], uv[3], 0.5, mid_pos);
    project(d->proj, PROJ_BACKWARD, 4, mid_pos, mid_pos);
    vec3_normalize(mid_pos, mid_pos);
    mat4_mul_vec4(*d->painter->transform, mid_pos, mid_pos);
    convert_framev4(d->painter->obs, d->frame, FRAME_VIEW,
                        mid_pos, mid_pos);
    sep = eraSepp(mid_pos, pos[0]) * 2;

    // For large angles, we just go down.  I think we could optimize this if
    // needed.
    if (sep < M_PI && is_clipped(4, clip)) return 0;

    r = d->f(1, node, d->uv, pos, mat, d->painter, d->user);
    if (r != 2) return r;

    // Check if we intersect a projection discontinuity, in which case we
    // split the painter if possible, otherwise we keep going.
    if (d->painter->proj->intersect_discontinuity && sep >= M_PI / 2)
        return 1;
    r = projection_intersect_discontinuity(d->painter->proj, pos, 4);
    if (r & PROJ_INTERSECT_DISCONTINUITY) {
        painter_t painter2 = *d->painter;
        projection_t projs[2];
        if (r & PROJ_CANNOT_SPLIT) return 1;
        d->painter->proj->split(d->painter->proj, projs);
        for (i = 0; i < 2; i++) {
            painter2.proj = &projs[i];
            r = d->f(2, node, d->uv, pos, mat, &painter2, d->user);
        }
        return r;
    }

    return d->f(2, node, d->uv, pos, mat, d->painter, d->user);
}

int traverse_surface(qtree_node_t *nodes, int nb_nodes,
                     const double uv[4][2],
                     const projection_t *proj,
                     const painter_t *painter,
                     int frame,
                     void *user,
                     int (*f)(int step,
                              qtree_node_t *node,
                              const double uv[4][2],
                              const double pos[4][4],
                              const double mat[3][3],
                              const painter_t *painter,
                              void *user))
{
    const double DEFAULT_UV[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    uv = uv ?: DEFAULT_UV;
    d_t d = {
        .proj = proj,
        .painter = painter,
        .frame = frame,
        .user = user,
        .f = f,
    };
    memcpy(d.uv, uv, sizeof(d.uv));
    return qtree_traverse(nodes, nb_nodes, &d, on_node);
}

