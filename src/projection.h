/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef PROJECTION_H
#define PROJECTION_H

#include <stdbool.h>

typedef struct projection projection_t;

/******** Section: Projection *******************************************/

// All the projections types.
enum {
    PROJ_NULL,
    PROJ_PERSPECTIVE,
    PROJ_STEREOGRAPHIC,
    PROJ_MERCATOR,
    PROJ_HEALPIX,
    PROJ_COUNT,
};

/* Enum: PROJ_FLAG
 * Modify the behavior of the <project> function.
 */
enum {
    PROJ_NO_CLIP            = 1 << 0,
    PROJ_BACKWARD           = 1 << 1,
    PROJ_TO_NDC_SPACE       = 1 << 2,
    PROJ_ALREADY_NORMALIZED = 1 << 3,
};

// Returns from intersect_discontinuity.
enum {
    PROJ_INTERSECT_DISCONTINUITY = 1 << 0,
    PROJ_CANNOT_SPLIT            = 1 << 1,
};

/* Type: projection_t
 * Represents a projection from the sphere into a 2d map.
 *
 * It's an opaque type, we initialize the structure with one of the
 * projection_init functions, and then use it with the <project> function.
 */
struct projection
{
    const char *name;
    double scaling[2];
    int flags;
    int type;

    // Matrices used by some projections.
    double mat[4][4];
    double mat3[3][3];
    // Use by some projs to define if we back project into the unit sphere
    // or into the sphere at infinity.
    bool   at_infinity;

    union {
        int shift;              // Only used by the mercator projection.
        struct {
            int level, x, y;    // Only used by toast projections.
        };
        struct {               // Only used by the healpix projection.
            int nside, pix;
        };
        void   *user;   // Can be used by custom projection.
    };

    void (*project)(const projection_t *proj, int flags,
                    const double *v, double *out);
    void (*backward)(const projection_t *proj, int flags,
                     const double *v, double *out);
    int (*intersect_discontinuity)(const projection_t *proj,
                                   const double a[3], const double b[3]);
    void (*split)(const projection_t *proj, projection_t *out);
};

/*
 * Function: projection_compute_fovs
 * Compute the fov in x and y given the minimum fov and screen aspect ratio.
 *
 * This can be used before calling <projection_init> in order to compute
 * fovx.
 *
 * Parameters:
 *   proj_type  - One of the PROJ_TYPE value.
 *   fov        - The minimum fov in x and y.
 *   aspect     - The aspect ratio of the screen.
 *   fovx       - Get the computed fov in x.
 *   fovy       - Get the computed fov in y.
 */
void projection_compute_fovs(int proj_type, double fov, double aspect,
                             double *fovx, double *fovy);

void projection_init(projection_t *proj, int type, double fovx, double aspect);
void projection_init_healpix(projection_t *proj, int nside, int pix,
                             bool swap, bool at_infinity);

/* Function: project
 * Apply a projection to coordinates
 *
 * If we project forward (without the PROJ_BACKWARD) flag, the projection
 * expects a 3d input, and return the coordinates in the plane clipping space.
 * To get normalized device space coordinates, we can use the
 * PROJ_TO_NDC_SPACE flag.
 *
 * Parameters:
 *  proj    - A projection.
 *  flags   - Union of <PROJ_FLAGS> values, to modify the behavior of the
 *            function.
 *  out_dim - Dimensions of the output argument (2 or 4).
 *  v       - Input coordinates.
 *  out     - Output coordinates.
 */
bool project(const projection_t *proj, int flags,
             int out_dim, const double *v, double *out);

// n can be 2 (for a line) or 4 (for a quad).
int projection_intersect_discontinuity(const projection_t *proj,
                                       double (*p)[4], int n);

#endif // PROJECTION_H
