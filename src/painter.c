/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <float.h>

static bool g_debug = false;

#define REND(rend, f, ...) do { \
        if ((rend)->f) (rend)->f((rend), ##__VA_ARGS__); \
    } while (0)

int paint_prepare(const painter_t *painter, double win_w, double win_h,
                  double scale)
{
    areas_clear_all(core->areas);
    REND(painter->rend, prepare, win_w, win_h, scale);
    return 0;
}

int paint_finish(const painter_t *painter)
{
    REND(painter->rend, finish);
    return 0;
}

int paint_flush(const painter_t *painter)
{
    REND(painter->rend, flush);
    return 0;
}

int paint_points(const painter_t *painter, int n, const point_t *points,
                 int frame)
{
    REND(painter->rend, points, painter, frame, n, points);
    return 0;
}

static int paint_quad_visitor(int step, qtree_node_t *node,
                              const double uv[4][2], const double pos[4][4],
                              const painter_t *painter,
                              void *user,
                              int s[2])
{
    texture_t *tex = USER_GET(user, 0);
    texture_t *normalmap_tex = USER_GET(user, 1);
    projection_t *tex_proj = USER_GET(user, 2);
    int frame = *(int*)USER_GET(user, 3);
    int grid_size = *(int*)USER_GET(user, 4);

    if (step == 0) {
        if ((1 << node->level) > grid_size) return 0;
        return 2;
    }
    if (step == 1) return 2;
    REND(painter->rend, quad, painter, frame,
                        tex, normalmap_tex, uv,
                        grid_size >> node->level,
                        tex_proj);

    // For testing.  Render lines around the quad.
    if (g_debug) {
        double lines[2][4];
        const int idx[4][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}};
        int i;
        for (i = 0; i < 4; i++) {
            vec2_copy(uv[idx[i][0]], lines[0]);
            vec2_copy(uv[idx[i][1]], lines[1]);
            REND(painter->rend, line, painter, frame, lines, 32, tex_proj);
        }
    }

    return 0;
}

int paint_quad(const painter_t *painter,
               int frame,
               texture_t *tex,
               texture_t *normalmap_tex,
               const double uv[4][2],
               const projection_t *tex_proj,
               int grid_size)
{
    // The OBSERVED frame (azalt) is left handed, so if we didn't specify
    // the uv values, we use inverted uv so that things work by default.
    const double UV1[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const double UV2[4][2] = {{1, 0}, {0, 0}, {1, 1}, {0, 1}};
    qtree_node_t nodes[128];
    if (tex && !texture_load(tex, NULL)) return 0;
    if (painter->color[3] == 0.0) return 0;
    if (!uv) uv = frame == FRAME_OBSERVED ? UV2 : UV1;
    traverse_surface(nodes, ARRAY_SIZE(nodes), uv, tex_proj,
                     painter, frame, 0,
                     USER_PASS(tex, normalmap_tex, tex_proj,
                               &frame, &grid_size),
                     paint_quad_visitor);
    return 0;
}

// Estimate the number of pixels covered by a quad.
double paint_quad_area(const painter_t *painter,
                       const double uv[4][2],
                       const projection_t *proj)
{
    const double DEFAULT_UV[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    double p[4][4] = {};
    double u[2], v[2];
    int i;
    uv = uv ?: DEFAULT_UV;
    double view_mat[4][4];

    // Clipping space coordinates.
    for (i = 0; i < 4; i++) {
        vec4_set(p[i], uv[i][0], uv[i][1], 0, 1);
        project(proj, PROJ_BACKWARD, 4, p[i], p[i]);
        mat4_mul_vec3(view_mat, p[i], p[i]);
        project(painter->proj, 0, 4, p[i], p[i]);
    }
    if (is_clipped(4, p)) return NAN;
    // Screen coordinates.
    for (i = 0; i < 4; i++) {
        vec2_mul(1.0 / p[i][3], p[i], p[i]);
        p[i][0] = (p[i][0] + 1.0) * painter->fb_size[0] / 2;
        p[i][1] = (p[i][1] + 1.0) * painter->fb_size[1] / 2;
    }
    // Return the cross product.
    vec2_sub(p[1], p[0], u);
    vec2_sub(p[2], p[0], v);
    return fabs(u[0] * v[1] - u[1] * v[0]);
}

int paint_text_size(const painter_t *painter, const char *text, double size,
                    int out[2])
{
    REND(painter->rend, text, text, NULL, size, NULL, 0, out);
    return 0;
}

int paint_text(const painter_t *painter, const char *text,
               const double pos[2], double size,
               const double color[4], double angle)
{
    REND(painter->rend, text, text, pos, size, color, angle, NULL);
    return 0;
}

int paint_texture(const painter_t *painter,
                  texture_t *tex,
                  const double uv[4][2],
                  const double pos[2],
                  double size,
                  const double color[4],
                  double angle)
{
    double c[4];
    const double white[4] = {1, 1, 1, 1};
    const double uv_full[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    if (!texture_load(tex, NULL)) return 0;
    if (!color) color = white;
    if (!uv) uv = uv_full;
    vec4_emul(painter->color, color, c);
    REND(painter->rend, texture, tex, uv, pos, size, c, angle);
    return 0;
}

static int paint_line(const painter_t *painter,
                      int frame,
                      double line[2][4], const projection_t *line_proj,
                      int split,
                      int intersect_discontinuity_mode)
{
    int mode = intersect_discontinuity_mode;
    int r, i;
    double view_pos[2][4];

    if (mode > 0 && painter->proj->intersect_discontinuity) {
        for (i = 0; i < 2; i++) {
            if (line_proj)
                project(line_proj, PROJ_BACKWARD, 4, line[i], view_pos[i]);
            else
                memcpy(view_pos[i], line[i], sizeof(view_pos[i]));
        }
        r = painter->proj->intersect_discontinuity(
                            painter->proj, view_pos[0], view_pos[1]);
        if (r & PROJ_INTERSECT_DISCONTINUITY) {
            return r;
        }
    }

    REND(painter->rend, line, painter, frame, line, split, line_proj);
    return 0;
}

int paint_lines(const painter_t *painter,
                int frame,
                int nb, double (*lines)[4],
                const projection_t *line_proj,
                int split,
                int intersect_discontinuity_mode)
{
    int i, ret = 0;
    assert(nb % 2 == 0);
    // XXX: we should check for discontinutiy before we cann paint_line.
    // So that we don't abort in the middle of the rendering.
    for (i = 0; i < nb; i += 2)
        ret |= paint_line(painter, frame, lines ? (void*)lines[i] : NULL,
                          line_proj, split, intersect_discontinuity_mode);
    return ret;
}

void paint_debug(bool value)
{
    g_debug = value;
}

bool painter_is_tile_clipped(const painter_t *painter, int frame,
                             int order, int pix, bool outside)
{
    double quad[4][4] = { {0, 0, 1, 1},
                          {1, 0, 1, 1},
                          {0, 1, 1, 1},
                          {1, 1, 1, 1} };
    double mat3[3][3];
    int i;

    // At order zero, the tiles are too big and it can give false positive,
    // so in that case is check the four tiles of level one.
    if (outside && order == 0) {
        for (i = 0; i < 4; i++) {
            if (!painter_is_tile_clipped(painter, frame, 1, pix * 4 + i,
                                         outside))
                return false;
        }
        return true;
    }

    healpix_get_mat3(1 << order, pix, mat3);
    for (i = 0; i < 4; i++) {
        mat3_mul_vec3(mat3, quad[i], quad[i]);
        healpix_xy2vec(quad[i], quad[i]);
        mat4_mul_vec4(*painter->transform, quad[i], quad[i]);
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0,
                            quad[i], quad[i]);
        project(painter->proj, 0, 4, quad[i], quad[i]);
    }
    return is_clipped(4, quad);
}

/* Draw the contour lines of a shape.
 * The shape is defined by the parametric function `proj`, that maps:
 * (u, v) -> (x, y, z) for u and v in the range [0, 1].
 *
 * borders_mask is a 4 bits mask to decide what side of the uv rect has to be
 * rendered (should be all set for a rect).
 */
int paint_quad_contour(const painter_t *painter, int frame,
                       const projection_t *proj,
                       int split, int borders_mask)
{
    const double lines[4][2][4] = {
        {{0, 0}, {1, 0}},
        {{1, 0}, {1, 1}},
        {{1, 1}, {0, 1}},
        {{0, 1}, {0, 0}}
    };
    int i;
    for (i = 0; i < 4; i++) {
        if (!((1 << i) & borders_mask)) continue;
        paint_line(painter, frame, lines[i], proj, split, 0);
    }
    return 0;
}

static void orbit_project(const projection_t *proj, int flags,
                          const double *v, double *out)
{
    const double *o = proj->user;
    double pos[4];
    double period = 2 * M_PI / o[5]; // Period in day.
    double mjd = o[0] + period * v[0];
    orbit_compute_pv(0, mjd, pos, NULL,
                     o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], 0.0, 0.0);
    vec3_copy(pos, out);
    out[3] = 1.0; // AU.
}

/*
 * Function: paint_orbit
 * Draw an orbit from it's elements.
 *
 * Parameters:
 *   painter    - The painter.
 *   frame      - Need to be FRAME_ICRS.
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
                double k_jd,      // date (MJD).
                double k_in,      // inclination (rad).
                double k_om,      // Longitude of the Ascending Node (rad).
                double k_w,       // Argument of Perihelion (rad).
                double k_a,       // Mean distance (Semi major axis).
                double k_n,       // Daily motion (rad/day).
                double k_ec,      // Eccentricity.
                double k_ma)      // Mean Anomaly (rad).
{
    const double orbit[8] = {k_jd, k_in, k_om, k_w, k_a, k_n, k_ec, k_ma};
    projection_t orbit_proj = {
        .backward   = orbit_project,
        .user       = (void*)orbit,
    };
    double line[2][4] = {{0}, {1}};
    // We only support ICRS for the moment to make things simpler.
    assert(frame == FRAME_ICRS);
    paint_line(painter, frame, line, &orbit_proj, 128, 1);
    return 0;
}

/*
 * Function: paint_2d_ellipse
 * Paint an ellipse in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation from unit into window space that defines
 *                the shape position, orientation and scale.
 *   width      - Line width.
 *   dashes     - Size of the dashes (0 for a plain line).
 *   label_pos  - Output the position that could be used for a label.  Can
 *                be NULL.
 */
int paint_2d_ellipse(const painter_t *painter_,
                     const double transf[4][4],
                     double width, double dashes,
                     double label_pos[2])
{
    double a2, b2, perimeter, pos[4], size[2], angle, a;
    painter_t painter = *painter_;

    a2 = vec2_norm2(transf[0]);
    b2 = vec2_norm2(transf[1]);

    // Estimate the number of dashes.
    painter.lines_stripes = 0;
    if (dashes) {
        perimeter = 2 * M_PI * sqrt((a2 + b2) / 2);
        painter.lines_stripes = perimeter / dashes;
    }

    vec2_copy(transf[3], pos);
    size[0] = sqrt(a2);
    size[1] = sqrt(b2);
    angle = atan2(transf[0][1], transf[0][0]);
    REND(painter.rend, ellipse_2d, &painter, pos, size, angle);

    if (label_pos) {
        label_pos[1] = DBL_MAX;
        for (a = 0; a < 2 * M_PI / 2; a += 2 * M_PI / 16) {
            vec4_set(pos, cos(a), sin(a), 0, 1);
            mat4_mul_vec4(transf, pos, pos);
            if (pos[1] < label_pos[1]) vec2_copy(pos, label_pos);
        }
    }
    return 0;
}

/*
 * Function: paint_2d_rect
 * Paint a rect in 2d.
 *
 * Parameters:
 *   painter    - The painter.
 *   transf     - Transformation from unit into window space that defines
 *                the shape position, orientation and scale.
 */
int paint_2d_rect(const painter_t *painter, const double transf[4][4])
{
    double pos[4], size[2], angle;

    vec2_copy(transf[3], pos);
    size[0] = vec2_norm(transf[0]);
    size[1] = vec2_norm(transf[1]);
    angle = atan2(transf[0][1], transf[0][0]);
    REND(painter->rend, rect_2d, painter, pos, size, angle);
    return 0;
}
