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

// S macro for C99 static argument array size.
#ifndef __cplusplus
#define S static
#else
#define S
#endif

typedef struct projection projection_t;
typedef struct projection_klass projection_klass_t;

/******** Section: Projection *******************************************/

// All the projections types.
enum {
    PROJ_NULL,
    PROJ_PERSPECTIVE,
    PROJ_STEREOGRAPHIC,
    PROJ_MERCATOR,
    PROJ_HAMMER,
    PROJ_MOLLWEIDE,
    PROJ_COUNT,
};

/* Enum: PROJ_FLAG
 */
enum {
    // Set in the projection flags to flip the rendering.
    PROJ_FLIP_VERTICAL      = 1 << 5,
    PROJ_FLIP_HORIZONTAL    = 1 << 6,

    // Set for the projections that have a discontinuity at the z = 0, z < 1
    // semi circle.
    PROJ_HAS_DISCONTINUITY  = 1 << 7,
};

/* Type: projection_t
 * Represents a projection from the sphere into a 2d map.
 *
 * It's an opaque type, we initialize the structure with one of the
 * projection_init functions, and then use it with the <project> function.
 */
struct projection
{
    projection_klass_t *klass;
    double fovy;
    int flags;

    // Perspective projection part of the projection.
    double mat[4][4];
    // Window size (screen size / screen density).
    double window_size[2];
};

struct projection_klass
{
    int id;
    const char *name;
    // Maximum FOV value we can accept.
    double max_fov;
    // Maximum FOV that looks good for the UI.
    double max_ui_fov;
    void (*init)(projection_t *proj, double fovy, double aspect);

    /*
     * The project function projects into a vec3 that will then be multiplied
     * by the projection 4x4 matrix to get the clipping space coordinates.
     */
    bool (*project)(const projection_t *proj,
                    const double v[S 3], double out[S 3]);
    bool (*backward)(const projection_t *proj,
                     const double v[S 3], double out[S 3]);
    void (*compute_fovs)(int proj_type, double fov, double aspect,
                         double *fovx, double *fovy);
};

#define PROJECTION_REGISTER(klass) \
    static void proj_register_##klass##_(void) __attribute__((constructor)); \
    static void proj_register_##klass##_(void) { proj_register_(&klass); }
void proj_register_(projection_klass_t *klass);


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

/*
 * Function: projection_init
 * Init a standard project.
 *
 * Parameters:
 *   type   - One of the <PROJ_TYPE> value.
 *   fovy   - The fov in Y direction (rad).
 *   win_w  - Window size in X (not framebuffer size).
 *   win_h  - Window size in Y (not framebuffer size).
 */
void projection_init(projection_t *proj, int type, double fovy,
                     double win_w, double win_h);

/*
 * Function: project_to_win
 * Project from view coordinates to windows coordinates.
 *
 * Compared to project, this function properly sets the z output value
 * in the range [0, 1], depending on the projection depth range.  The
 * behavior is more aligned to what OpenGL does.
 *
 * Return false in case of error.  We return true even if the point is
 * not visible.
 */
bool project_to_win(const projection_t *proj, const double input[S 3],
                    double out[S 3]);

/*
 * Function: project_to_win_xy
 * Similar to project_to_win, but only returns the x and y coordinates.
 */
bool project_to_win_xy(const projection_t *proj, const double input[S 3],
                       double out[S 2]);
/*
 * Function: project_to_clip
 * Project from view coordinates to clip space.
 *
 * Return false in case of error.  Note: we return true even if the point is
 * not visible.
 */
bool project_to_clip(const projection_t *proj, const double input[S 3],
                     double out[S 4]);

/*
 * Function: unproject
 * Compute a backward projection
 *
 * Parameters:
 *   proj   - A projection.
 *   v      - Input xyz coordinates, in window space.
 *   out    - Output xyz coordinates in view space.
 *
 * Return:
 *   True for success.
 */
bool unproject(const projection_t *proj,
               const double v[S 3], double out[S 3]);

#undef S

#endif // PROJECTION_H
