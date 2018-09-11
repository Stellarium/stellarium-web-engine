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
};

struct areas
{
    UT_array *items;
};

static double ellipse_dist(const double center[2], double angle,
                           double a, double b, const double p_[2])
{
    double p[2], p2[2], t;
    // Convert into ellipse frame.
    vec2_sub(p_, center, p);
    vec2_rotate(-angle, p, p);
    t = atan2(p[1], p[0]);
    p2[0] = a * cos(t);
    p2[1] = b * sin(t);
    return max(0.0, vec2_norm(p) - vec2_norm(p2));
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

void areas_clear_all(areas_t *areas)
{
    utarray_clear(areas->items);
}

int areas_lookup(const areas_t *areas, const double pos[2], double max_dist,
                 uint64_t *oid, uint64_t *hint)
{
    item_t *item = NULL, *best = NULL;
    double dist, best_dist = max_dist;

    while ( (item = (item_t*)utarray_next(areas->items, item)) ) {
        if (item->a == item->b) { // Circle
            dist = max(0, vec2_dist(item->pos, pos) - item->a);
        } else { // Ellipse
            dist = ellipse_dist(item->pos, item->angle, item->a, item->b, pos);
        }
        // Simple heuristic: if we are totally inside several shapes, we
        // pick the one with the smallest area.
        if (dist == 0.0) dist = -1.0 / (item->a * item->b);

        if (dist < best_dist) {
            best_dist = dist;
            best = item;
        }
    }
    if (!best) return 0;
    *oid = best->oid;
    *hint = best->hint;
    return 1;
}
