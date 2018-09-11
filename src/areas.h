/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <stdint.h>

/*
 * File: areas.h
 * An area instance maintains a list of shapes with associated id/nsid for
 * fast mouse lookup.
 */

typedef struct areas areas_t;

/*
 * Function: areas_create
 * Return a new areas.
 */
areas_t *areas_create(void);

/*
 * Function: areas_add_circle
 * Add a circle shape to the areas.
 *
 * Parameters:
 *   areas  - an areas instance.
 *   pos    - a 2d position in screen space.
 *   r      - radius in screen space.
 *   id     - id associated with the circle.  Can be NULL if nsid is set.
 *   nsid   - an nsid associated with the circle.  Can be zero if id is set.
 */
void areas_add_circle(areas_t *areas, const double pos[2], double r,
                      const char id[128], uint64_t nsid,
                      uint64_t oid, uint64_t hint);

void areas_add_ellipse(areas_t *areas, const double pos[2], double angle,
                       double a, double b,
                       const char id[128], uint64_t nsid,
                       uint64_t oid, uint64_t hint);

/*
 * Function: areas_lookup
 * Return the closest shape at a given position in an areas.
 *
 * Parameters:
 *   area       - an areas instance.
 *   pos        - a 2d position in screen space.
 *   max_dist   - max distance to shapes to consider.
 *   id         - get the shape id (if there is one).
 *   nsid       - get the shape nsid (if there is one).
 *
 * Return:
 *   0 if no shape was found, otherwise 1.
 */
int areas_lookup(const areas_t *areas, const double pos[2], double max_dist,
                 char id[128], uint64_t *nsid, uint64_t *oid, uint64_t *hint);

/*
 * Function: areas_clear_all
 * Remove all the shapes in an areas instance.
 */
void areas_clear_all(areas_t *areas);
