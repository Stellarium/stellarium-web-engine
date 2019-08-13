/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "areas.h"
#include "utarray.h"
#include "utils/vec.h"
#include "utils/utils.h"

#include <assert.h>
#include <math.h>

typedef struct item item_t;

struct item
{
    double pos[2];
    double a; // Semi-major axis.
    double b; // Semi-minor axis.
    double angle;
    uint64_t oid;
    uint64_t hint;

    // Optional triangle mesh.
    struct {
        int verts_count;
        float (*verts)[2];
        int indices_count;
        uint16_t *indices;
    } mesh;
};

struct areas
{
    UT_array *items;
};

/*
 * Compute the signed distance between a point and the closest point on an
 * ellipse.
 *
 * Return a negative value when we are inside the ellipse.
 */
static double ellipse_dist(const double center[2], double angle,
                           double a, double b, const double p_[2])
{
    double p[2], p2[2], t;

    // Circle: can use a faster algorithm.
    if (a == b)
        return vec2_dist(center, p_) - a;

    // Convert into ellipse frame.
    vec2_sub(p_, center, p);
    vec2_rotate(-angle, p, p);
    t = atan2(a * p[1], b * p[0]);
    p2[0] = a * cos(t);
    p2[1] = b * sin(t);
    return vec2_norm(p) - vec2_norm(p2);
}

areas_t *areas_create(void)
{
    static UT_icd item_icd = {sizeof(item_t), NULL, NULL, NULL};
    areas_t *areas;
    areas = calloc(1, sizeof(*areas));
    utarray_new(areas->items, &item_icd);
    return areas;
}

void areas_add_circle(areas_t *areas, const double pos[2], double r,
                      uint64_t oid, uint64_t hint)
{
    item_t item = {};
    memcpy(item.pos, pos, sizeof(item.pos));
    item.a = item.b = r;
    item.oid = oid;
    item.hint = hint;
    utarray_push_back(areas->items, &item);
}

void areas_add_ellipse(areas_t *areas, const double pos[2], double angle,
                       double a, double b,
                       uint64_t oid, uint64_t hint)
{
    item_t item = {};
    memcpy(item.pos, pos, sizeof(item.pos));
    item.angle = angle;
    item.a = a;
    item.b = b;
    item.oid = oid;
    item.hint = hint;
    utarray_push_back(areas->items, &item);
}

// Util function to return the center of a triangle in a mesh.
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

void areas_add_triangles_mesh(areas_t *areas, int verts_count,
                              const float verts[][2],
                              int indices_count,
                              const uint16_t indices[],
                              uint64_t oid, uint64_t hint)
{
    item_t item = {};
    double w, w_tot = 0, tri_pos[2], pos[2] = {}, r2 = 0;
    int i;

    // Compute bounding circle.
    for (i = 0; i < indices_count; i += 3) {
        w = triangle_center(verts, indices + i, tri_pos);
        pos[0] += w * tri_pos[0];
        pos[1] += w * tri_pos[1];
        w_tot += w;
    }
    pos[0] /= w_tot;
    pos[1] /= w_tot;
    for (i = 0; i < verts_count; i++) {
        r2 = max(r2, (verts[i][0] - pos[0]) * (verts[i][0] - pos[0]) +
                     (verts[i][1] - pos[1]) * (verts[i][1] - pos[1]));
    }

    memcpy(item.pos, pos, sizeof(item.pos));
    item.a = item.b = sqrt(r2);
    item.oid = oid;
    item.hint = hint;

    // Copy the mesh data.
    item.mesh.verts_count = verts_count;
    item.mesh.verts = calloc(verts_count, sizeof(*item.mesh.verts));
    memcpy(item.mesh.verts, verts, verts_count * sizeof(*item.mesh.verts));
    item.mesh.indices_count = indices_count;
    item.mesh.indices = calloc(indices_count, sizeof(*item.mesh.indices));
    memcpy(item.mesh.indices, indices,
           indices_count * sizeof(*item.mesh.indices));

    utarray_push_back(areas->items, &item);
}

void areas_clear_all(areas_t *areas)
{
    item_t *item = NULL;
    while ((item = (item_t*)utarray_next(areas->items, item))) {
        free(item->mesh.verts);
        free(item->mesh.indices);
    }
    utarray_clear(areas->items);
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

// Compute the signed distance of a point to an item.
static double item_dist(const item_t *item, const double pos[static 2])
{
    int i;
    double ret;
    ret =  ellipse_dist(item->pos, item->angle, item->a, item->b, pos);
    if (ret > 0 || !item->mesh.indices_count) return ret;

    // For mesh item, test if we are inside a triangle.
    for (i = 0; i < item->mesh.indices_count; i += 3) {
        if (triangle_contains(item->mesh.verts, item->mesh.indices + i, pos)) {
            return 0;
        }
    }
    return 100000.0;
}

/*
 * Function: lookup_score.
 * Weight function to decide what item to return during a lookup.
 *
 * Parameters:
 *   item       - An area item.
 *   pos        - The search position.
 *   max_dist   - The search area.
 *
 * Returns:
 *   The lookup score.  The item with the highest value is the one that
 *   should be selected.
 */
static double lookup_score(const item_t *item, const double pos[static 2],
                           double max_dist)
{
    double dist, area, ret;

    dist = item_dist(item, pos);
    area = item->a * item->b;

    // XXX: probably need to change this algo.
    if (dist > max_dist) return 0.0;
    // If we are inside a shape, the distance is clamped so that we still
    // have a chance to select a DSO when we click in the middle of it.
    dist = max(dist, -10);
    ret = max_dist - fabs(dist);
    ret += min(area, 10); // Up to 10 pixels advantage for larger objects.
    return ret;

}

int areas_lookup(const areas_t *areas, const double pos[2], double max_dist,
                 uint64_t *oid, uint64_t *hint)
{
    item_t *item = NULL, *best = NULL;
    double score, best_score = 0.0;

    while ( (item = (item_t*)utarray_next(areas->items, item)) ) {
        score = lookup_score(item, pos, max_dist);
        if (score > best_score) {
            best_score = score;
            best = item;
        }
    }
    if (!best) return 0;
    *oid = best->oid;
    *hint = best->hint;
    return 1;
}
