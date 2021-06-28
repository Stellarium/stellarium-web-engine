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

#include "frames.h"
#include "utils/mesh.h"
#include "projection.h"
#include "uv_map.h"

typedef struct obj obj_t;
typedef struct observer observer_t;
typedef struct painter painter_t;
typedef struct point point_t;
typedef struct point_3d point_3d_t;
typedef struct texture texture_t;
typedef struct renderer renderer_t;

// Base font size in pixels
#define FONT_SIZE_BASE 15

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

/*
 * Enum: TEXT_EFFECT_FLAGS
 * Effects that can be applied to the text.
 */
enum {
    TEXT_UPPERCASE = 1 << 0,
    TEXT_BOLD      = 1 << 1,
    TEXT_SMALL_CAP = 1 << 2,
    TEXT_DEMI_BOLD = 1 << 3,
    // Only used in the label manager.  If set, the text position or opacity
    // can be changed dynamically to avoid collisions.
    TEXT_FLOAT     = 1 << 5,
    TEXT_SPACED    = 1 << 6,
    TEXT_SEMI_SPACED = 1 << 7
};

enum {
    MODE_TRIANGLES = 0,
    MODE_LINES,
    MODE_POINTS,
};

struct point
{
    double  pos[2];
    double  size;       // Radius in window pixel (pixel with density scale).
    uint8_t color[4];
    obj_t   *obj;
};

struct point_3d
{
    double  pos[3];     // View position
    double  size;       // Radius in window pixel (pixel with density scale).
    uint8_t color[4];
    obj_t   *obj;
};

// Painter flags
enum {
    PAINTER_ADD                 = 1 << 0, // Use addition blending.
    PAINTER_HIDE_BELOW_HORIZON  = 1 << 2,
    PAINTER_PLANET_SHADER       = 1 << 4,
    PAINTER_RING_SHADER         = 1 << 5,
    PAINTER_IS_MOON             = 1 << 6, // Only for moon texture!
    PAINTER_ATMOSPHERE_SHADER   = 1 << 8,
    PAINTER_FOG_SHADER          = 1 << 9,
    PAINTER_ENABLE_DEPTH        = 1 << 10,

    // Passed to paint_lines.
    PAINTER_SKIP_DISCONTINUOUS  = 1 << 14,
    // Allow the renderer to reorder this item for batch optimiziation.
    PAINTER_ALLOW_REORDER       = 1 << 15,
};

enum {
    PAINTER_TEX_COLOR = 0,
    PAINTER_TEX_NORMAL = 1,
};

struct painter
{
    renderer_t      *rend;          // The render used.
    const observer_t *obs;

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
    // Manual hard limit for anything displayed - takes precedence over
    // above variables.
    double          hard_limit_mag;

    // Point halo / core ratio (zero for no halo).
    double          points_halo;

    struct {
        int type;
        texture_t *tex;
        double mat[3][3];
    } textures[2];

    struct {
        // Viewport caps for fast clipping test.
        double bounding_cap[4];

        // 4 caps representing the 4 sides of the viewport
        double viewport_caps[4][4];
        int nb_viewport_caps;

        // Sky above ground cap for fast clipping test.
        // The cap is pointing up, and has an angle of 91 deg (1 deg margin to
        // take refraction into account).
        double sky_cap[4];
    } clip_info[FRAMES_NB];

    union {
        // For planet rendering only.
        struct {
            double          (*sun)[4]; // pos + radius.
            double          (*light_emit)[3];
            // A list of spheres that will be used for shadow.
            int             shadow_spheres_nb;
            double          (*shadow_spheres)[4]; // pos + radius.
            texture_t       *shadow_color_tex; // Used for lunar eclipses.
            float           scale; // The fake scale we used.
            float           min_brightness;
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

        // For line rendering only.
        struct {
            float width;
            float glow;
            float dash_length;  // Dash length in pixel.
            float dash_ratio;   // 0.5 for equal dash / space.
            float fade_dist_min;
            float fade_dist_max;
        } lines;
    };
};

int paint_prepare(painter_t *painter, double win_w, double win_h,
                  double scale);
int paint_finish(const painter_t *painter);

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

int paint_3d_points(const painter_t *painter, int n, const point_3d_t *points);

/*
 * Function: paint_quad
 *
 * Render a quad mapped into the 3d sphere
 *
 * Parameters:
 *   painter        - A painter.
 *   frame          - Referential frame of the inputs (<FRAME> values).
 *   map            - The uv mapping of the quad into the 3d space.
 *   grid_size      - how many sub vertices we use.
 */
int paint_quad(const painter_t *painter,
               int frame,
               const uv_map_t *map,
               int grid_size);

/* Function: paint_quad_contour
 *
 * Draw the contour lines of a quad.
 *
 * border_mask is a 4 bits mask to decide what side of the uv rect has to be
 * rendered (should be all set for a rect).
 */
int paint_quad_contour(const painter_t *painter, int frame,
                       const uv_map_t *map,
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
 * Function: paint_line
 * Render a 3d line.
 *
 * Warning: using the mapping function, it is in theory possible to render
 * a very large line with any shape in a single call.  However the current
 * implementation doesn't work well in that case.
 *
 * Parameters:
 *   painter    - A painter instance.
 *   frame      - Frame of the inputs.
 *   lines      - Vertices of the lines.
 *   map        - Optional function that can be used to represent lines as
 *                parametric function.  If set then the actual coordinates
 *                of the lines are the mapping of the point through this
 *                function.
 *   split      - Number of segments requested in the output.  If < 0 use
 *                an adaptive algorithm, where -split is the minimum level
 *                of split.  In that case -split = log2(min number of points).
 *   flags      - Supported flags:
 *                  PAINTER_SKIP_DISCONTINUOUS - if set, any line that
 *                  intersects a discontinuity is ignored.
 */
int paint_line(const painter_t *painter,
               int frame,
               double line[2][4], const uv_map_t *map,
               int split, int flags);

int paint_linestring(const painter_t *painter, int frame,
                     int size, const double (*points)[3]);

/*
 * Function: paint_mesh
 * Render a 3d mesh
 *
 * Parameters:
 *   painter        - A painter instance.
 *   frame          - Frame of the vertex coordinates.
 *   mode           - MODE_TRIANGLES or MODE_LINES.
 *   mesh           - A 3d triangle mesh.
 */
int paint_mesh(const painter_t *painter, int frame, int mode,
               const mesh_t *mesh);


int paint_text_bounds(const painter_t *painter, const char *text,
                      const double win_pos[2],
                      int align, int effects,
                      double size, double bounds[4]);

/*
 * Function: paint_text
 * Render text
 *
 * Parameters:
 *   text       - The text to render.
 *   win_pos    - Text position in window coordinates.
 *   view_pos   - Optional text position in view coordinates (for depth).
 *   align      - Union of <ALIGN_FLAGS>.
 *   effects    - Union of <TEXT_EFFECT_FLAGS>.
 *   size       - Text size in window unit.
 *   angle      - Angle in radian.
 */
int paint_text(const painter_t *painter,
               const char *text, const double win_pos[2],
               const double view_pos[3], int align,
               int effects, double size, double angle);

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


/*
 * Function: painter_is_quad_clipped
 * Test whether the 3d mapping of a quad is clipped.
 *
 * Parameters:
 *   painter    - A painter.
 *   frame      - One of the <FRAME> enum frame.
 *   map        - The mapping function from UV to the 3D space.
 *
 * Returns:
 *   True if the quad is clipped, false otherwise.
 *
 *   A clipped quad is guaranteed to be not visible, but it is not guaranteed
 *   that a non visible quad is clipped.  So this function can return false
 *   even though a quad is not actually visible.
 */
bool painter_is_quad_clipped(const painter_t *painter, int frame,
                             const uv_map_t *map);


// Function: painter_is_healpix_clipped
//
// Convenience function that checks if a healpix pixel is not visible.
//
// Parameters:
//  painter   - The painter.
//  frame     - One of the <FRAME> enum frame.
//  order     - Healpix order.
//  pix       - Healpix pix.
//
// Returns:
//  True if the tile is clipped, false otherwise.
//
//  A clipped tile is guaranteed to be not visible, but it is not guaranteed
//  that a non visible tile is clipped.  So this function can return false
//  even though a tile is not actually visible.
bool painter_is_healpix_clipped(const painter_t *painter, int frame,
                                int order, int pix);

/*
 * Function: painter_is_planet_healpix_clipped
 * Check if a healpix pixel on the surface of a planet is clipped.
 *
 * Parameters:
 *   painter    - A painter.
 *   transf     - 4x4 matrix providing the planet position / scale.
 *   order      - Healpix order.
 *   pix        - Healpix pix.
 *
 * Return:
 *   True if the pixel is guarantied to be clipped.
 */
bool painter_is_planet_healpix_clipped(const painter_t *painter,
                                       const double transf[4][4],
                                       int order, int pix);

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

/*
 * Function: painter_is_cap_clipped
 * Test if a spherical cap is clipped.
 *
 * Convenience function that checks if a cap is clipped.
 *
 * When true is returned, the passed cap is guaranteed to be outside the
 * viewport. When false is returned, there is no guarantee that the cap is
 * visible.
 *
 * Parameters:
 *   painter       - The painter.
 *   frame         - One of the <FRAME> enum frame.
 *   cap           - The cap
 *
 * Returns:
 *   True if the cap is clipped, false otherwise.
 */
bool painter_is_cap_clipped(const painter_t *painter, int frame,
                            const double cap[4]);

// Function: painter_update_caps
//
// Update the bounding caps for each reference frames.
// Must be called after painter creation to allow for fast clipping tests.
//
// Parameters:
//  painter       - The painter.
void painter_update_clip_info(painter_t *painter);

/*
 * Function: paint_orbit
 * Draw an orbit from it's elements.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - Need to be FRAME_ICRF.
 *   transf     - Parent body position transformation.
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
                const double transf[4][4],
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
 * Function: paint_cap
 * Paint a spherical cap.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - The frame in which the cap is defined
 *   cap        - The spherical cap.
 */
void paint_cap(const painter_t *painter, int frame, double cap[4]);

int painter_get_3d_model_bounds(const painter_t *painter, const char *model,
                                double bounds[2][3]);

void paint_3d_model(const painter_t *painter, const char *model,
                    const double mat[4][4], const json_value *args);

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
