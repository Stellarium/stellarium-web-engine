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
#include "utils/mesh2d.h"
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

void areas_add_triangles_mesh(areas_t *areas, int verts_count,
                              const float verts[][2],
                              int indices_count,
                              const uint16_t indices[],
                              uint64_t oid, uint64_t hint)
{
    item_t item = {};
    double r;

    mesh2d_get_bounding_circle(verts, indices, indices_count, item.pos, &r);
    item.a = item.b = r;
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

// Compute the signed distance of a point to an item.
static double item_dist(const item_t *item, const double pos[static 2])
{
    double ret;
    ret =  ellipse_dist(item->pos, item->angle, item->a, item->b, pos);
    if (ret > 0 || !item->mesh.indices_count) return ret;

    if (mesh2d_contains_point(item->mesh.verts, item->mesh.indices,
                              item->mesh.indices_count, pos))
        return 0;

    return 100000.0; // Default value at quasi infinity.
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
