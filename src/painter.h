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

typedef struct observer observer_t;
typedef struct renderer renderer_t;
typedef struct painter painter_t;
typedef struct point point_t;
typedef struct texture texture_t;

struct renderer
{
    void (*prepare)(renderer_t *rend,
                    double win_w, double win_h, double scale);
    void (*finish)(renderer_t *rend);
    void (*flush)(renderer_t *rend);

    void (*points)(renderer_t           *rend,
                   const painter_t      *painter,
                   int                  frame,
                   int                  n,
                   const point_t        *points);

    void (*quad)(renderer_t          *rend,
                 const painter_t     *painter,
                 int                 frame,
                 texture_t           *tex,
                 texture_t           *normalmap,
                 double              uv[4][2],
                 int                 grid_size,
                 const projection_t  *tex_proj);

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
                 double          size,
                 const double    color[4],
                 double          angle,
                 int             tex_size[2]    // Output, can be NULL.
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
    double  pos[4];
    double  size;       // Radius in window pixel (pixel with density scale).
    double  color[4];
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

    double          mag_max;        // Max visual magnitude.
    double          label_mag_max;  // After this mag, don't show the labels.
    double          hint_mag_max;   // After this mag, don't show the hints.

    double          lines_width;
    double          lines_stripes;
    double          points_smoothness;
    double          (*depth_range)[2]; // If set use depth test.

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
            double (*compute_lum)(void *user, const double pos[3]);
            void *user;
        } atm;
    };
};

int paint_prepare(const painter_t *painter, double win_w, double win_h,
                  double scale);
int paint_finish(const painter_t *painter);
int paint_flush(const painter_t *painter);
int paint_points(const painter_t *painter, int n, const point_t *points,
                 int frame);

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
               texture_t *tex,
               texture_t *normalmap_tex,
               const double uv[4][2],
               const projection_t *proj,
               int grid_size);

// Estimate the number of pixels covered by a quad.
double paint_quad_area(const painter_t *painter,
                       const double uv[4][2],
                       const projection_t *proj);

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

int paint_text_size(const painter_t *painter, const char *text, double size,
                    int out[2]);

/*
 * Function: paint_text
 * Render text
 *
 * Parameters:
 *   text   - the text to render.
 *   pos    - text position in window coordinates.
 *   size   - text size in window unit.
 */
int paint_text(const painter_t *painter,
               const char *text, const double pos[2],
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
//  outside   - Set whether the tile is an outside (planet) tile.
//
// Returns:
//  True if the tile is clipped, false otherwise.
//
//  A clipped tile is guarantied to be not visible, but it is not guarantied
//  that a non visible tile is clipped.  So this function can return false
//  even though a tile is not actually visible.
bool painter_is_tile_clipped(const painter_t *painter, int frame,
                             int order, int pix,
                             bool outside);

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
 *   transf     - Transformation from unit into window space that defines
 *                the shape position, orientation and scale.
 *   dashes     - Size of the dashes (0 for a plain line).
 *   label_pos  - Output the position that could be used for a label.  Can
 *                be NULL.
 */
int paint_2d_ellipse(const painter_t *painter,
                     const double transf[4][4], double dashes,
                     double label_pos[2]);

/*
 * Function: paint_2d_rect
 * Paint a rect in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation from unit into window space that defines
 *                the shape position, orientation and scale.
 */
int paint_2d_rect(const painter_t *painter, const double transf[4][4]);

/*
 * Function: paint_2d_line
 * Paint a line in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation from unit into window space that defines
 *                the shape position, orientation and scale.
 *   p1         - First pos, in unit coordinates (-1 to 1).
 *   p2         - Second pos, in unit coordinates (-1 to 1).
 */
int paint_2d_line(const painter_t *painter, const double transf[4][4],
                  const double p1[2], const double p2[2]);

#endif // PAINTER_H
