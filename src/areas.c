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
    char id[128];
    uint64_t nsid;
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
                      const char id[128], uint64_t nsid)
{
    item_t item = {};

    assert(nsid || (id && strlen(id) < sizeof(item.id) - 1));
    memcpy(item.pos, pos, sizeof(item.pos));
    item.a = item.b = r;
    item.nsid = nsid;
    if (!nsid) strcpy(item.id, id);
    utarray_push_back(areas->items, &item);
}

void areas_add_ellipse(areas_t *areas, const double pos[2], double angle,
                       double a, double b,
                       const char id[128], uint64_t nsid)
{
    item_t item = {};
    assert(nsid || (id && strlen(id) < sizeof(item.id) - 1));
    memcpy(item.pos, pos, sizeof(item.pos));
    item.angle = angle;
    item.a = a;
    item.b = b;
    item.nsid = nsid;
    if (!nsid) strcpy(item.id, id);
    utarray_push_back(areas->items, &item);
}

void areas_clear_all(areas_t *areas)
{
    utarray_clear(areas->items);
}

int areas_lookup(const areas_t *areas, const double pos[2], double max_dist,
                 char id[128], uint64_t *nsid)
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
    *nsid = best->nsid;
    if (!best->nsid) {
        strcpy(id, best->id);
    }
    return 1;
}

