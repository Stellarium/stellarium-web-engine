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

#include <assert.h>
#include <math.h>

typedef struct item item_t;

struct item
{
    double pos[2];
    double r;
    char id[128];
    uint64_t nsid;
};

struct areas
{
    UT_array *items;
};

static double vec2_dist(const double a[2], const double b[2])
{
    double x, y;
    x = a[0] - b[0];
    y = a[1] - b[1];
    return sqrt(x * x + y * y);
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
    item.r = r;
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
        dist = vec2_dist(item->pos, pos) - item->r;
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

