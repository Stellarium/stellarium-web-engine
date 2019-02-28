/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef PAINTER_H
#define PAINTER_H

#include <stdint.h>

#include "projection.h"
#include "frames.h"

typedef struct observer observer_t;
typedef struct renderer renderer_t;
typedef struct painter painter_t;
typedef struct point point_t;
typedef struct texture texture_t;

/*
 * Enum: ALIGN_FLAGS
 * Alignment values that can be passed to paint_text.
 *
 * Same as nanovg.
 */
enum {
    // Horizontal align
    ALIGN_LEFT      = 1 << 0,   // Default, align text horizontally to left.
    ALIGN_CENTER    = 1 << 1,   // Align text horizontally to center.
    ALIGN_RIGHT     = 1 << 2,   // Align text horizontally to right.
    // Vertical align
    ALIGN_TOP       = 1 << 3,   // Align text vertically to top.
    ALIGN_MIDDLE    = 1 << 4,   // Align text vertically to middle.
    ALIGN_BOTTOM    = 1 << 5,   // Align text vertically to bottom.
    ALIGN_BASELINE  = 1 << 6,   // Default, align text vertically to baseline.
};

struct renderer
{
    void (*prepare)(renderer_t *rend,
                    double win_w, double win_h, double scale);
    void (*finish)(renderer_t *rend);
    void (*flush)(renderer_t *rend);

    void (*points_2d)(renderer_t        *rend,
                   const painter_t      *painter,
                   int                  n,
                   const point_t        *points);

    void (*quad)(renderer_t          *rend,
                 const painter_t     *painter,
                 int                 frame,
                 const double        mat[3][3],
                 int                 grid_size,
                 const projection_t  *tex_proj);

    void (*quad_wireframe)(renderer_t           *rend,
                           const painter_t      *painter,
                           int                  frame,
                           const double         mat[3][3],
                           int                  grid_size,
                           const projection_t   *tex_proj);

    void (*texture)(renderer_t       *rend,
                    const texture_t  *tex,
                    double           uv[4][2],
                    const double     pos[2],
                    double           size,
                    const double     color[4],
                    double           angle);

    void (*text)(renderer_t      *rend,
                 const char      *text,
                 const double    pos[2],
                 int             align,
                 double          size,
                 const double    color[4],
                 double          angle,
                 const char      *font,
                 double          bounds[4]    // Output, can be NULL.
                 );

    void (*line)(renderer_t           *rend,
                 const painter_t      *painter,
                 int                  frame,
                 double               line[2][4],
                 int                  nb_segs,
                 const projection_t   *line_proj);

    void (*ellipse_2d)(renderer_t       *rend,
                       const painter_t  *painter,
                       const double     pos[2],
                       const double     size[2],
                       double           angle);

    void (*rect_2d)(renderer_t          *rend,
                    const painter_t     *painter,
                    const double        pos[2],
                    const double        size[2],
                    double              angle);

    void (*line_2d)(renderer_t          *rend,
                    const painter_t     *painter,
                    const double        p1[2],
                    const double        p2[2]);
};

renderer_t* render_gl_create(void);
renderer_t* render_svg_create(const char *out);


struct point
{
    double  pos[2];
    double  size;       // Radius in window pixel (pixel with density scale).
    uint8_t color[4];
    uint64_t oid;       // Used instead of id if set.
    uint64_t hint;      // Oid hint.
};

// Painter flags
enum {
    PAINTER_ADD                 = 1 << 0, // Use addition blending.
    PAINTER_HIDE_BELOW_HORIZON  = 1 << 2,
    PAINTER_FAST_MODE           = 1 << 3,
    PAINTER_PLANET_SHADER       = 1 << 4,
    PAINTER_RING_SHADER         = 1 << 5,
    PAINTER_IS_MOON             = 1 << 6, // Only for moon texture!
    PAINTER_SHOW_BAYER_LABELS   = 1 << 7,
    PAINTER_ATMOSPHERE_SHADER   = 1 << 8,
    PAINTER_FOG_SHADER          = 1 << 9,
};

enum {
    PAINTER_TEX_COLOR = 0,
    PAINTER_TEX_NORMAL = 1,
};

struct painter
{
    renderer_t      *rend;          // The render used.
    const observer_t *obs;

    // Optional tranf matrix applied to the coordinates.
    // For example to set the planet position in the hips rendering.
    double          (*transform)[4][4];

    projection_t    *proj;          // Project from view to NDC.

    double          color[4];       // Global color.
    int             fb_size[2];     // Size of the render buffer.
    double          pixel_scale;
                                    // rendered.
    int             flags;
    double          contrast;       // Contrast effect when rendering tex.

    double          stars_limit_mag;   // Actual stars visual magnitude limit.
    // Base magnitude limit for hints and labels
    double          hints_limit_mag;
    const char      *font;          // Set to NULL for default font.

    double          lines_width;
    double          lines_stripes;
    double          points_smoothness;
    double          (*depth_range)[2]; // If set use depth test.

    struct {
        int type;
        texture_t *tex;
        double mat[3][3];
    } textures[2];

    // Viewport caps for fast clipping test.
    // The cap is defined as the vector xyzw with xyz the observer viewing
    // direction in all frames and w the cosinus of the max separation between
    // a visible point and xyz.
    // To test if a pos in a given frame is clipped, we can use:
    //   vec3_dot(pos, painter.viewport_caps[frame]) <
    //       painter.viewport_caps[frame][3]
    double          viewport_caps[FRAMES_NB][4];

    // Sky above ground cap for fast clipping test.
    // The cap is pointing up, and has an angle of 91 deg (1 deg margin to take
    // refraction into account).
    double          sky_caps[FRAMES_NB][4];

    union {
        // For planet rendering only.
        struct {
            double          (*sun)[4]; // pos + radius.
            double          (*light_emit)[3];
            // A list of spheres that will be used for shadow.
            int             shadow_spheres_nb;
            double          (*shadow_spheres)[4]; // pos + radius.
            texture_t       *shadow_color_tex; // Used for lunar eclipses.
        } planet;

        // For atmosphere rendering only.
        struct {
            // All the factors for A. J. Preetham model:
            //   Ax, Bx, Cx, Dx, Ex, kx,
            //   Ay, By, Cy, Dy, Ey, ky,
            float p[12];
            float sun[3]; // Sun position.
            // Callback to compute the luminosity at a given point.
            float (*compute_lum)(void *user, const float pos[3]);
            void *user;
        } atm;
    };
};

int paint_prepare(painter_t *painter, double win_w, double win_h,
                  double scale);
int paint_finish(const painter_t *painter);
int paint_flush(const painter_t *painter);

/*
 * Set the current painter texture.
 *
 * Parameters:
 *   painter    - A painter struct.
 *   slot       - The texture slot we want to set.  Can be one of:
 *                PAINTER_TEX_COLOR or PAINTER_TEX_NORMAL.
 *   uv_mat     - The transformation to the uv coordinates to get the part
 *                of the texture we want to use.  NULL default to the
 *                identity matrix, that is the full texture.
 */
void painter_set_texture(painter_t *painter, int slot, texture_t *tex,
                         const double uv_mat[3][3]);

/* Function: paint_2d_points
 *
 * Render a list of star-like points in 2d.

 * Parameters:
 *  painter       - The painter.
 *  n             - The number of points in the array.
 *  points        - The array of points to draw.
 */
int paint_2d_points(const painter_t *painter, int n, const point_t *points);

/* Function: paint_quad
 *
 * Render a quad mapped into the 3d sphere
 *
 * The shape of the quad is defined by the projection `proj` called backward,
 * that is, a mapping of (u, v) => (x, y, z).  If unspecified, u and v range
 * from 0 to 1.
 *
 * Parameters:
 *  tex           - Optional texture.
 *  normalmap_tex - Normal map texture (NULL for no normal map).
 *  uv            - Tex UV coordinates (NULL for default to whole texture).
 *  proj          - Projection from sphere to uv (used backward).
 *  frame         - Referential frame of the inputs (<FRAME> values).
 *  grid_size     - how many sub vertices we use.
 */
int paint_quad(const painter_t *painter,
               int frame,
               const projection_t *proj,
               int grid_size);

/* Function: paint_quad_contour
 *
 * Draw the contour lines of a shape.
 * The shape is defined by the parametric function `proj`, that maps:
 * (u, v) -> (x, y, z) for u and v in the range [0, 1].
 *
 * border_mask is a 4 bits mask to decide what side of the uv rect has to be
 * rendered (should be all set for a rect).
 */
int paint_quad_contour(const painter_t *painter, int frame,
                       const projection_t *proj,
                       int split, int border_mask);

/*
 * Function: paint_tile_contour
 * Draw the contour lines of an healpix tile.
 *
 * This is mostly useful for debugging.
 *
 * Parameters:
 *   painter    - A painter.
 *   frame      - One the <FRAME> enum value.
 *   order      - Healpix order.
 *   pix        - Healpix pix.
 *   split      - Number or times we split the lines.
 */
int paint_tile_contour(const painter_t *painter, int frame,
                       int order, int pix, int split);

/*
 * discontinuity_mode:
 *      0: Do not check.
 *      1: Check and abort if intersect.
 *      2: Check and only fix if simple (we can split the projection).
 */
int paint_lines(const painter_t *painter,
                int frame,
                int nb, double (*lines)[4],
                const projection_t *line_proj,
                int split,
                int discontinuity_mode);


int paint_text_bounds(const painter_t *painter, const char *text,
                      const double pos[2], int align, double size,
                      double bounds[4]);

/*
 * Function: paint_text
 * Render text
 *
 * Parameters:
 *   text   - the text to render.
 *   pos    - text position in window coordinates.
 *   align  - union of <ALIGN_FLAGS>.
 *   size   - text size in window unit.
 */
int paint_text(const painter_t *painter,
               const char *text, const double pos[2], int align,
               double size, const double color[4], double angle);

/*
 * Function: paint_texture
 * Render a 2d texture
 *
 * Parameters:
 *   pos    - Center position in window coordinates.
 *   size   - Texture size in window unit.
 */
int paint_texture(const painter_t *painter,
                  texture_t *tex,
                  const double uv[4][2],
                  const double pos[2],
                  double size,
                  const double color[4],
                  double angle);

// Set painter debug mode on or off.
void paint_debug(bool value);

// Function: painter_is_tile_clipped
//
// Convenience function that checks if a healpix tile is not visible.
//
// Parameters:
//  painter   - The painter.
//  frame     - One of the <FRAME> enum frame.
//  order     - Healpix order.
//  pix       - Healpix pix.
//  outside   - Set whether the tile is an outside (not planet) tile.
//
// Returns:
//  True if the tile is clipped, false otherwise.
//
//  A clipped tile is guaranteed to be not visible, but it is not guaranteed
//  that a non visible tile is clipped.  So this function can return false
//  even though a tile is not actually visible.
bool painter_is_tile_clipped(const painter_t *painter, int frame,
                             int order, int pix,
                             bool outside);

// Function: painter_is_point_clipped_fast
//
// Convenience function that checks if a 3D point is visible.
// This function is fast, especially when is_normalized is true but not very
// accurate. To refine the clipping test, the point must be projected.
//
// Parameters:
//  painter       - The painter.
//  frame         - One of the <FRAME> enum frame.
//  pos           - the 3D point
//  is_normalized - true if p is already normalized
//
// Returns:
//  True if the point is clipped, false otherwise.
//
// When true is returned, the passed point is guaranteed to be outside the
// viewport. When false is returned, there is no guarantee that the point is
// visible.
bool painter_is_point_clipped_fast(const painter_t *painter, int frame,
                                   const double pos[3], bool is_normalized);

// Function: painter_is_2d_point_clipped
//
// Convenience function that checks if a 2D point is visible.
//
// Parameters:
//  painter       - The painter.
//  p             - the 2D point
//
// Returns:
//  True if the point is clipped, false otherwise.
bool painter_is_2d_point_clipped(const painter_t *painter, const double p[2]);

// Function: painter_is_2d_circle_clipped
//
// Convenience function that checks if a 2D circle is visible.
//
// Parameters:
//  painter       - The painter.
//  p             - the center point
//  radius        - the circle radius
//
// Returns:
//  True if the circle is clipped, false otherwise.
bool painter_is_2d_circle_clipped(const painter_t *painter, const double p[2],
                                 double radius);

// Function: painter_is_cap_clipped_fast
//
// Convenience function that checks if a cap is clipped.
// This function is fast but not very accurate.
// To refine the clipping test, the point must be projected.
//
// Parameters:
//  painter       - The painter.
//  frame         - One of the <FRAME> enum frame.
//  cap           - the cap
//
// Returns:
//  True if the cap is clipped, false otherwise.
//
// When true is returned, the passed cap is guaranteed to be outside the
// viewport. When false is returned, there is no guarantee that the cap is
// visible.
bool painter_is_cap_clipped_fast(const painter_t *painter, int frame,
                                   const double cap[4]);

// Function: painter_update_caps
//
// Update the bounding caps for each reference frames.
// Must be called after painter creation to allow for fast clipping tests.
//
// Parameters:
//  painter       - The painter.
void painter_update_caps(const painter_t *painter);

/*
 * Function: paint_orbit
 * Draw an orbit from it's elements.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - Need to be FRAME_ICRF.
 *   k_jd       - Orbit epoch date (MJD).
 *   k_in       - Inclination (rad).
 *   k_om       - Longitude of the Ascending Node (rad).
 *   k_w        - Argument of Perihelion (rad).
 *   k_a        - Mean distance (Semi major axis).
 *   k_n        - Daily motion (rad/day).
 *   k_ec       - Eccentricity.
 *   k_ma       - Mean Anomaly (rad).
 */
int paint_orbit(const painter_t *painter, int frame,
                double k_jd, double k_in, double k_om, double k_w,
                double k_a, double k_n, double k_ec, double k_ma);

/*
 * Function: paint_2d_ellipse
 * Paint an ellipse in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation matrix applied to the coordinates.
 *                Can be NULL for the identity.
 *   pos        - The ellipse position in window space.
 *   size       - The ellipse size in window space.
 *   dashes     - Size of the dashes (0 for a plain line).
 *   label_pos  - Output the position that could be used for a label.  Can
 *                be NULL.
 */
int paint_2d_ellipse(const painter_t *painter,
                     const double transf[3][3],
                     double dashes,
                     const double pos[2],
                     const double size[2],
                     double label_pos[2]);

/*
 * Function: paint_2d_rect
 * Paint a rect in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation applied to the coordinates.
 *                Can be NULL for the identity.
 *   pos        - Top left position in window space.
 *   size       - Size in window space.
 */
int paint_2d_rect(const painter_t *painter, const double transf[3][3],
                  const double pos[2], const double size[2]);

/*
 * Function: paint_2d_line
 * Paint a line in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation applied to the coordinates.
 *                Can be NULL for the identity.
 *   p1         - First pos, in window coordinates.
 *   p2         - Second pos, in window coordinates.
 */
int paint_2d_line(const painter_t *painter, const double transf[3][3],
                  const double p1[2], const double p2[2]);

/*
 * Function: painter_project_ellipse
 * Project an ellipse defined on the sphere to the screen.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - The frame in which the ellipse is defined
 *   ra         - First spherical pos (rad).
 *   de         - Second spherical pos (rad).
 *   angle      - The ellipse angle w.r.t ra axis (rad).
 *   size_x     - The ellipse large size (rad).
 *   size_y     - The ellipse small size (rad).
 *   win_pos    - The ellipse center in screen coordinates (px).
 *   win_size   - The ellipse small and large sizes in screen coordinates (px).
 *   win_angle  - The ellipse angle in screen coordinates (radian).
 */
void painter_project_ellipse(const painter_t *painter, int frame,
        float ra, float de, float angle, float size_x, float size_y,
        double win_pos[2], double win_size[2], double *win_angle);

/*
 * Function: painter_project
 * Project a point defined on the sphere to the screen.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - The frame in which the point is defined.
 *   pos        - The point 3D coordinates.
 *   at_inf     - true for fixed objects (far away from the solar system).
 *                For such objects, pos is assumed to be normalized.
 *   clip_first - If the point is identified as clipped, skip projection and
 *                return false. win_pos content is then undefined.
 *   win_pos    - The point position in screen coordinates (px).
 *
 * Returns:
 *   False if the point is clipped, true otherwise.
 */
bool painter_project(const painter_t *painter, int frame, const double pos[3],
                     bool at_inf, bool clip_first, double win_pos[2]);


/*
 * Function: painter_unproject
 * Unproject a 2D point defined on the screen to the passed frame.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - The frame in which the point needs to be unprojected.
 *   win_pos    - The point 2D coordinates on screen (px).
 *   pos        - The corresponding 3D unit vector in the given frame.
 *
 * Returns:
 *   False if the point cannot be unprojected, true otherwise.
 */
bool painter_unproject(const painter_t *painter, int frame,
                     const double win_pos[2], double pos[3]);

#endif // PAINTER_H
