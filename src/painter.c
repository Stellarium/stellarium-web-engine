/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "line_mesh.h"
#include "render.h"

#include <float.h>

static bool g_debug = false;

// Test if a shape in clipping coordinates is clipped or not.
static bool is_clipped(int n, double (*pos)[4])
{
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    int i, p;
    for (p = 0; p < 6; p++) {
        for (i = 0; i < n; i++) {
            if (    P[p][0] * pos[i][0] +
                    P[p][1] * pos[i][1] +
                    P[p][2] * pos[i][2] +
                    P[p][3] * pos[i][3] <= 0) {
                break;
            }
        }
        if (i == n) // All the points are outside a clipping plane.
            return true;
    }
    return false;
}


static bool intersect_circle_rect(
        const double rect[4], const double c_center[2], double r)
{
    #define sqr(x) ((x)*(x))
    double circle_dist_x = fabs(c_center[0] - (rect[0] + rect[2] / 2));
    double circle_dist_y = fabs(c_center[1] - (rect[1] + rect[3] / 2));

    if (circle_dist_x > rect[2] / 2 + r) { return false; }
    if (circle_dist_y > rect[3] / 2 + r) { return false; }

    if (circle_dist_x <= rect[2] / 2) { return true; }
    if (circle_dist_y <= rect[3] / 2) { return true; }

    double corner_dist_sq = sqr(circle_dist_x - rect[2] / 2) +
                            sqr(circle_dist_y - rect[3] / 2);

    return corner_dist_sq <= r * r;
    #undef sqr
}


/*
 * Function: compute_viewport_cap
 * Compute the viewport cap (in given frame).
 */
static void compute_viewport_cap(painter_t *painter, int frame)
{
    int i;
    double p[4][3];
    double c[3];
    const double w = painter->proj->window_size[0];
    const double h = painter->proj->window_size[1];
    double max_sep = 0;
    double* cap = painter->clip_info[frame].bounding_cap;
    bool r;

    painter_unproject(painter, frame, VEC(w / 2, h / 2), cap);
    assert(vec3_is_normalized(cap));

    #define MARGIN 0
    r  = painter_unproject(painter, frame, VEC(MARGIN, MARGIN), p[0]);
    r &= painter_unproject(painter, frame, VEC(w - MARGIN, MARGIN), p[1]);
    r &= painter_unproject(painter, frame, VEC(w - MARGIN, h - MARGIN), p[2]);
    r &= painter_unproject(painter, frame, VEC(MARGIN, h - MARGIN), p[3]);
    if (!r) max_sep = M_PI;

    // Compute max separation from all corners.
    for (i = 0; i < 4; i++) {
        assert(vec3_is_normalized(p[i]));
        max_sep = max(max_sep, eraSepp(cap, p[i]));
    }
    cap[3] = cos(max_sep);

    // Compute side caps
    if (max_sep > M_PI_2)
        return;

    painter->clip_info[frame].nb_viewport_caps = 4;
    for (i = 0; i < 4; i++) {
        vec3_cross(p[i], p[(i + 1) % 4], c);
        vec3_normalize(c, c);
        vec3_copy(c, painter->clip_info[frame].viewport_caps[i]);
        if (!cap_contains_vec3(painter->clip_info[frame].viewport_caps[i], cap))
            vec3_mul(-1, painter->clip_info[frame].viewport_caps[i],
                         painter->clip_info[frame].viewport_caps[i]);
    }
}

static void compute_sky_cap(const observer_t *obs, int frame, double cap[4])
{
    double p[3] = {0, 0, 1};
    convert_frame(obs, FRAME_OBSERVED, frame, true, p, cap);
    cap[3] = cos(91.0 * M_PI / 180);
}

void painter_update_clip_info(painter_t *painter)
{
    int i;
    for (i = 0; i < FRAMES_NB ; ++i) {
        compute_viewport_cap(painter, i);
        compute_sky_cap(painter->obs, i, painter->clip_info[i].sky_cap);
    }
}

int paint_prepare(painter_t *painter, double win_w, double win_h,
                  double scale)
{
    int i;
    bool cull_flipped;

    for (i = 0; i < ARRAY_SIZE(painter->textures); i++)
        mat3_set_identity(painter->textures[i].mat);
    areas_clear_all(core->areas);

    cull_flipped = (bool)(painter->proj->flags & PROJ_FLIP_HORIZONTAL) !=
                   (bool)(painter->proj->flags & PROJ_FLIP_VERTICAL);
    render_prepare(painter->rend, painter->proj,
                   win_w, win_h, scale, cull_flipped);
    return 0;
}

int paint_finish(const painter_t *painter)
{
    render_finish(painter->rend);
    return 0;
}

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
                         const double uv_mat[3][3])
{
    assert(!painter->textures[slot].tex);
    painter->textures[slot].tex = tex;
    mat3_set_identity(painter->textures[slot].mat);
    mat3_copy(uv_mat ?: mat3_identity, painter->textures[slot].mat);
}


int paint_2d_points(const painter_t *painter, int n, const point_t *points)
{
    render_points_2d(painter->rend, painter, n, points);
    return 0;
}

int paint_3d_points(const painter_t *painter, int n, const point_3d_t *points)
{
    render_points_3d(painter->rend, painter, n, points);
    return 0;
}

int paint_quad(const painter_t *painter,
               int frame,
               const uv_map_t *map,
               int grid_size)
{
    if (painter->textures[PAINTER_TEX_COLOR].tex) {
        if (!texture_load(painter->textures[PAINTER_TEX_COLOR].tex, NULL))
            return 0;
    }
    if (painter->color[3] == 0.0) return 0;

    // XXX: need to check if we intersect discontinuity, and if so split
    // the painter projection.
    render_quad(painter->rend, painter, frame, grid_size, map);
    return 0;
}

int paint_text_bounds(const painter_t *painter, const char *text,
                      const double pos[2], int align, int effects,
                      double size, double bounds[4])
{
    render_text(painter->rend, painter, text, pos, NULL, align, effects, size,
                painter->color, 0, bounds);
    return 0;
}

int paint_text(const painter_t *painter, const char *text,
               const double win_pos[2], const double view_pos[3],
               int align, int effects, double size,
               double angle)
{
    render_text(painter->rend, painter, text, win_pos, view_pos, align, effects,
                size, painter->color, angle, NULL);
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
    render_texture(painter->rend, tex, uv, pos, size, c, angle);
    return 0;
}


static void line_func(void *user, double t, double out[3])
{
    double pos[4];
    const painter_t *painter = USER_GET(user, 0);
    int frame = *(int*)USER_GET(user, 1);
    const double (*line)[4] = USER_GET(user, 2);
    const uv_map_t *map = USER_GET(user, 3);

    vec4_mix(line[0], line[1], t, pos);
    if (map) uv_map(map, pos, pos, NULL);
    convert_framev4(painter->obs, frame, FRAME_VIEW, pos, pos);
    vec3_copy(pos, out);
}

/*
 * Function: intersect_discontinuity_line
 * Check if a segment intersect the azimuth = 180° line
 *
 * We assume the segment is smaller than 180°.
 */
static bool segment_intersects_discontinuity_line(
    const double a[3], const double b[3])
{
    double x0, x1;
    if (a[2] < 0 && b[2] < 0) return false; // Both in front of us.
    if (a[0] * b[0] > 0) return false; // Both on same side of the line.
    if (a[2] > 0 && b[2] > 0) return true;
    x0 = atan2(a[0], -a[2]);
    x1 = atan2(b[0], -b[2]);
    if (fabs(x0) + fabs(x1) >= M_PI) {
        return true;
    } else {
        return false;
    }
}

/*
 * Function: cap_intersects_discontinuity_line
 * Check if a cap intersects the azimuth = 180° line
 */
static bool cap_intersects_discontinuity_line(
        const double cap_[4], const observer_t *obs, int frame)
{
    // Not totally sure about the algo.
    double cap[4];
    convert_frame(obs, frame, FRAME_VIEW, true, cap_, cap);
    cap[3] = cap_[3];
    if (cap_contains_cap(VEC(0, 0, -1, 0), cap)) return false;
    if (cap_contains_cap(VEC(+1, 0, 0, 0), cap)) return false;
    if (cap_contains_cap(VEC(-1, 0, 0, 0), cap)) return false;
    return true;
}


int paint_line(const painter_t *painter,
               int frame,
               double line[2][4], const uv_map_t *map,
               int split, int flags)
{
    int i, size;
    double view_pos[2][4];
    double (*win_line)[3] = NULL;
    double (*pos_line)[3] = NULL;
    bool discontinuous = false;
    double splits[2][2][4];

    if (!map) assert(flags & PAINTER_SKIP_DISCONTINUOUS); // For the moment.

    if (painter->proj->flags & PROJ_HAS_DISCONTINUITY) {
        for (i = 0; i < 2; i++) {
            if (map)
                uv_map(map, line[i], view_pos[i], NULL);
            else
                memcpy(view_pos[i], line[i], sizeof(view_pos[i]));
            vec3_normalize(view_pos[i], view_pos[i]);
            convert_frame(painter->obs, frame, FRAME_VIEW, true,
                          view_pos[i], view_pos[i]);
        }
        if (segment_intersects_discontinuity_line(view_pos[0], view_pos[1]))
            discontinuous = true;
    }

    if (discontinuous)
        goto split;

    size = line_tesselate(line_func, painter->proj,
                          USER_PASS(painter, &frame, line, map),
                          split, &pos_line, &win_line);
    if (size < 0) goto split;
    render_line(painter->rend, painter, pos_line, win_line, size);
    free(win_line);
    free(pos_line);
    return 0;

split:
    if (flags & PAINTER_SKIP_DISCONTINUOUS || split <= 3)
        return 0;
    assert(map);
    vec4_copy(line[0], splits[0][0]);
    vec4_mix(line[0], line[1], 0.5, splits[0][1]);
    vec4_copy(splits[0][1], splits[1][0]);
    vec4_copy(line[1], splits[1][1]);
    paint_line(painter, frame, splits[0], map, split / 2, flags);
    paint_line(painter, frame, splits[1], map, split / 2, flags);
    return 0;
}

int paint_linestring(const painter_t *painter, int frame,
                     int size, const double (*points)[3])
{
    double (*win_line)[3];
    double (*pos_line)[3];
    int i;
    win_line = calloc(size, sizeof(*win_line));
    pos_line = calloc(size, sizeof(*pos_line));
    for (i = 0; i < size; i++) {
        convert_frame(painter->obs, frame, FRAME_VIEW, true,
                      points[i], pos_line[i]);
        project_to_win(painter->proj, pos_line[i], win_line[i]);
    }
    render_line(painter->rend, painter, pos_line, win_line, size);
    free(win_line);
    free(pos_line);
    return 0;
}

/*
 * Function: paint_mesh
 * Render a 3d mesh
 *
 * Parameters:
 *   painter        - A painter instance.
 *   frame          - Frame of the vertex coordinates.
 *   mode           - MODE_TRIANGLES or MODE_LINES or MODE_POINTS.
 *   mesh           - A 3d triangle mesh.
 */
int paint_mesh(const painter_t *painter_, int frame, int mode,
               const mesh_t *mesh)
{
    painter_t painter = *painter_;
    int i;
    mesh_t *mesh2;
    bool use_stencil = (mode == MODE_TRIANGLES && mesh->subdivided);

    if (mode == MODE_TRIANGLES && mesh->triangles_count == 0) return 0;
    if (mode == MODE_LINES && mesh->lines_count == 0) return 0;
    if (mode == MODE_POINTS && mesh->points_count == 0) return 0;

    if (painter_is_cap_clipped(&painter, frame, mesh->bounding_cap))
        return 0;

    // Skip meshes that intersect a discontinuty.
    if (mode != MODE_POINTS && (painter.proj->flags & PROJ_HAS_DISCONTINUITY)) {
        if (cap_intersects_discontinuity_line(
                    mesh->bounding_cap, painter.obs, frame)) {
            goto subdivide;
        }
    }

    switch (mode) {
    case MODE_TRIANGLES:
        render_mesh(painter.rend, &painter, frame, mode,
             mesh->vertices_count, mesh->vertices,
             mesh->triangles_count, mesh->triangles, use_stencil);
        break;
    case MODE_LINES:
        render_mesh(painter.rend, &painter, frame, mode,
             mesh->vertices_count, mesh->vertices,
             mesh->lines_count, mesh->lines, false);
        break;
    case MODE_POINTS:
        render_mesh(painter.rend, &painter, frame, mode,
             mesh->vertices_count, mesh->vertices,
             mesh->points_count, mesh->points, false);
        break;
    }
    return 0;

subdivide:
    // Case where we need to split the mesh into smaller parts.
    mesh2 = mesh_copy(mesh);
    // Convert the positions to view frame.
    for (i = 0; i < mesh->vertices_count; i++) {
        vec3_normalize(mesh->vertices[i], mesh2->vertices[i]);
        convert_frame(painter.obs, frame, FRAME_VIEW, true,
                      mesh2->vertices[i], mesh2->vertices[i]);
    }
    mesh_cut_antimeridian(mesh2);

    // XXX: can clean up this.
    switch (mode) {
    case MODE_TRIANGLES:
        render_mesh(painter.rend, &painter, FRAME_VIEW, mode,
                 mesh2->vertices_count, mesh2->vertices,
                 mesh2->triangles_count, mesh2->triangles, use_stencil);
        break;
    case MODE_LINES:
        render_mesh(painter.rend, &painter, FRAME_VIEW, mode,
                 mesh2->vertices_count, mesh2->vertices,
                 mesh2->lines_count, mesh2->lines, false);
        break;
    }

    mesh_delete(mesh2);
    return 0;
}

void paint_debug(bool value)
{
    g_debug = value;
}

bool painter_is_cap_clipped(const painter_t *painter, int frame,
                            const double cap[4])
{
    int i;

    if (!cap_intersects_cap(painter->clip_info[frame].bounding_cap, cap))
        return true;

    // Skip if below horizon.
    if (painter->flags & PAINTER_HIDE_BELOW_HORIZON &&
            !cap_intersects_cap(painter->clip_info[frame].sky_cap, cap))
        return true;

    const typeof (painter->clip_info[frame]) *clipinfo =
            &painter->clip_info[frame];
    if (clipinfo->nb_viewport_caps > 0) {
        for (i = 0; i < clipinfo->nb_viewport_caps; ++i) {
            if (!cap_intersects_cap(clipinfo->viewport_caps[i], cap)) {
                // LOG_E("clipped cap!");
                return true;
            }
        }
    }
    return false;
}

bool painter_is_point_clipped_fast(const painter_t *painter, int frame,
                                   const double pos[3], bool is_normalized)
{
    double v[3];
    int i;
    vec3_copy(pos, v);
    if (!is_normalized)
        vec3_normalize(v, v);
    if (!cap_contains_vec3(painter->clip_info[frame].bounding_cap, v))
        return true;
    if ((painter->flags & PAINTER_HIDE_BELOW_HORIZON) &&
         !cap_contains_vec3(painter->clip_info[frame].sky_cap, v))
        return true;
    const typeof (painter->clip_info[frame]) *clipinfo =
            &painter->clip_info[frame];
    if (clipinfo->nb_viewport_caps > 0) {
        for (i = 0; i < clipinfo->nb_viewport_caps; ++i) {
            if (!cap_contains_vec3(clipinfo->viewport_caps[i], v)) {
                return true;
            }
        }
    }
    return false;
}

bool painter_is_2d_point_clipped(const painter_t *painter, const double p[2])
{
    return p[0] >= 0 && p[0] <= painter->proj->window_size[0] &&
           p[1] >= 0 && p[1] <= painter->proj->window_size[1];
}

bool painter_is_2d_circle_clipped(const painter_t *painter, const double p[2],
                                 double radius)
{
    const double rect[4] = {0, 0, painter->proj->window_size[0],
                            painter->proj->window_size[1]};
    return !intersect_circle_rect(rect, p, radius);
}

bool painter_is_quad_clipped(const painter_t *painter, int frame,
                             const uv_map_t *map)
{
    double corners[4][4];
    double quad[4][4], normals[4][3];
    double p[4][4];
    double bounding_cap[4];
    int i;
    int order = map->order;

    uv_map_get_bounding_cap(map, bounding_cap);
    assert(vec3_is_normalized(bounding_cap));
    if (painter_is_cap_clipped(painter, frame, bounding_cap))
        return true;
    if (order < 2)
        return false;

    uv_map_grid(map, 1, corners, normals);
    for (i = 0; i < 4; i++) {
        vec3_copy(corners[i], quad[i]);
        quad[i][3] = 1.0;
        convert_framev4(painter->obs, frame, FRAME_VIEW, quad[i], quad[i]);
        project_to_clip(painter->proj, quad[i], p[i]);
        assert(!isnan(p[i][0]));
    }
    return is_clipped(4, p);
}

static bool painter_is_planet_quad_clipped(const painter_t *painter, int frame,
                                           const uv_map_t *map)
{
    double corners[4][4];
    double quad[4][4], normals[4][3], normal[4];
    double p[4][4], direction[3];
    uv_map_t children[4];
    int i;
    int order = map->order;

    // At too low orders, the tiles are too distorted which can give false
    // positive, so we check the children in that case.
    if (order < 2) {
        uv_map_subdivide(map, children);
        for (i = 0; i < 4; i++) {
            if (!painter_is_planet_quad_clipped(painter, frame, &children[i]))
                return false;
        }
        return true;
    }

    uv_map_grid(map, 1, corners, normals);
    for (i = 0; i < 4; i++) {
        vec3_copy(corners[i], quad[i]);
        quad[i][3] = 1.0;
        convert_framev4(painter->obs, frame, FRAME_VIEW, quad[i], quad[i]);
        project_to_clip(painter->proj, quad[i], p[i]);
        assert(!isnan(p[i][0]));
    }
    if (is_clipped(4, p)) return true;

    /*
     * For planet tiles, we also do culling test.  Since the quad is not
     * plane, to prevent error, we only do it at level > 1, and we check the
     * normal of the four corners.
     *
     * Because of the projection distortion, we test not by checking
     * the view z value, but with the dot product of the normal and the
     * direction vector to the middle of the planet.
     */
    if (order > 1) {
        vec3_copy((*map->transf)[3], direction);
        vec3_normalize(direction, direction);
        convert_frame(painter->obs, frame, FRAME_VIEW, true,
                      direction, direction);
        for (i = 0; i < 4; i++) {
            vec3_copy(normals[i], normal);
            normal[3] = 0.0;
            vec3_normalize(normal, normal);
            convert_frame(painter->obs, frame, FRAME_VIEW, true,
                          normal, normal);
            if (vec3_dot(normal, direction) < 0) return false;
        }
        return true;
    }

    return false;
}

bool painter_is_healpix_clipped(const painter_t *painter, int frame,
                                int order, int pix)
{
    uv_map_t map;
    uv_map_init_healpix(&map, order, pix, false, false);
    return painter_is_quad_clipped(painter, frame, &map);
}

bool painter_is_planet_healpix_clipped(const painter_t *painter,
                                       const double transf[4][4],
                                       int order, int pix)
{
    uv_map_t map;
    uv_map_init_healpix(&map, order, pix, false, false);
    map.transf = (void*)transf;
    return painter_is_planet_quad_clipped(painter, FRAME_ICRF, &map);
}


/* Draw the contour lines of a shape.
 *
 * borders_mask is a 4 bits mask to decide what side of the uv rect has to be
 * rendered (should be all set for a rect).
 */
int paint_quad_contour(const painter_t *painter, int frame,
                       const uv_map_t *map,
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
        paint_line(painter, frame, lines[i], map, split, 0);
    }
    return 0;
}

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
                       int order, int pix, int split)
{
    uv_map_t map;
    uv_map_init_healpix(&map, order, pix, false, false);
    return paint_quad_contour(painter, frame, &map, split, 15);
}

static void orbit_map(const uv_map_t *map, const double v[2], double out[4])
{
    const double *o = map->user;
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
int paint_orbit(const painter_t *painter_, int frame,
                const double transf[4][4],
                double k_jd,      // date (MJD).
                double k_in,      // inclination (rad).
                double k_om,      // Longitude of the Ascending Node (rad).
                double k_w,       // Argument of Perihelion (rad).
                double k_a,       // Mean distance (Semi major axis).
                double k_n,       // Daily motion (rad/day).
                double k_ec,      // Eccentricity.
                double k_ma)      // Mean Anomaly (rad).
{
    painter_t painter = *painter_;
    const double orbit[8] = {k_jd, k_in, k_om, k_w, k_a, k_n, k_ec, k_ma};
    uv_map_t map = {
        .map        = orbit_map,
        .transf     = (void*)transf,
        .user       = (void*)orbit,
    };
    double line[2][4] = {{0}, {1}};
    double center[3];
    // We only support ICRF for the moment to make things simpler.
    assert(frame == FRAME_ICRF);
    convert_frame(painter.obs, frame, FRAME_VIEW, false, transf[3], center);
    painter.lines.fade_dist_min = -center[2] - k_a;
    painter.lines.fade_dist_max = -center[2] + k_a * 2;
    painter.flags |= PAINTER_ENABLE_DEPTH;
    paint_line(&painter, frame, line, &map, -5, 0);
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
 *   pos        - The ellipse position in window space.
 *   size       - The ellipse size in window space.
 *   dashes     - Size of the dashes (0 for a plain line).
 *   label_pos  - Output the position that could be used for a label.  Can
 *                be NULL.
 */
int paint_2d_ellipse(const painter_t *painter_,
                     const double transf[3][3], double dashes,
                     const double pos[2],
                     const double size[2],
                     double label_pos[2])
{
    double a2, b2, perimeter, p[3], s[2], a, m[3][3], angle, nb_dashes = 0;
    painter_t painter = *painter_;

    // Apply the pos, size and angle.
    mat3_set_identity(m);
    if (pos) mat3_itranslate(m, pos[0], pos[1]);
    if (size) mat3_iscale(m, size[0], size[1], 1);
    if (transf) mat3_mul(m, transf, m);

    a2 = vec2_norm2(m[0]);
    b2 = vec2_norm2(m[1]);

    // Estimate the number of dashes.
    if (dashes) {
        perimeter = 2 * M_PI * sqrt((a2 + b2) / 2);
        nb_dashes = perimeter / dashes;
    }

    vec2_copy(m[2], p);
    s[0] = sqrt(a2);
    s[1] = sqrt(b2);
    angle = atan2(m[0][1], m[0][0]);
    render_ellipse_2d(painter.rend, &painter, p, s, angle, nb_dashes);

    if (label_pos) {
        label_pos[1] = DBL_MAX;
        for (a = 0; a < 2 * M_PI / 2; a += 2 * M_PI / 16) {
            vec3_set(p, cos(a), sin(a), 1);
            mat3_mul_vec3(m, p, p);
            if (p[1] < label_pos[1]) vec2_copy(p, label_pos);
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
 *   pos        - Top left position in window space.  If set to NULL, center
 *                the rect at the origin.
 *   size       - Size in window space.  Default value to a rect of size 1.
 */
int paint_2d_rect(const painter_t *painter, const double transf[3][3],
                  const double pos[2], const double size[2])
{
    double p[3], s[2], angle, m[3][3];

    mat3_set_identity(m);
    if (pos) mat3_itranslate(m, pos[0] + size[0] / 2, pos[1] + size[1] / 2);
    if (size) mat3_iscale(m, size[0] / 2, size[1] / 2, 1);
    if (transf) mat3_mul(m, transf, m);

    vec2_copy(m[2], p);
    s[0] = vec2_norm(m[0]);
    s[1] = vec2_norm(m[1]);
    angle = atan2(m[0][1], m[0][0]);
    render_rect_2d(painter->rend, painter, p, s, angle);
    return 0;
}

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
                  const double p1[2], const double p2[2])
{
    double p1_win[3] = {p1[0], p1[1], 1};
    double p2_win[3] = {p2[0], p2[1], 1};
    if (transf) {
        mat3_mul_vec3(transf, p1_win, p1_win);
        mat3_mul_vec3(transf, p2_win, p2_win);
    }
    render_line_2d(painter->rend, painter, p1_win, p2_win);
    return 0;
}

void paint_cap(const painter_t *painter, int frame, double cap[4])
{
    double r;
    double p[4];

    if (cap[3] >= 1.0) return;
    if (!cap_intersects_cap(painter->clip_info[frame].bounding_cap, cap))
        return;

    vec3_copy(cap, p);
    p[3] = 0;
    r = acos(cap[3]) * 2;
    obj_t* obj = obj_create("circle", NULL);
    obj_set_attr(obj, "pos", p);
    obj_set_attr(obj, "frame", frame);
    double size[2] = {r, r};
    obj_set_attr(obj, "size", size);
    obj_render(obj, painter);
    obj_release(obj);
}

__attribute__((weak))
int painter_get_3d_model_bounds(const painter_t *painter, const char *model,
                                double bounds[2][3])
{
    return 1;
}

__attribute__((weak))
void paint_3d_model(const painter_t *painter, const char *model,
                    const double model_mat_[4][4], const json_value *args)
{
}


void painter_project_ellipse(const painter_t *painter, int frame,
        float ra, float de, float angle, float size_x, float size_y,
        double win_pos[2], double win_size[2], double *win_angle)
{
    double p[4], c[4], a[4], b[4], mat[3][3];

    assert(!isnan(ra));
    assert(!isnan(de));
    assert(!isnan(size_x));

    if (isnan(size_y)) {
        size_y = size_x;
    } else {
        if (isnan(angle))
            angle = 0;
    }

    // 1. Center.
    vec4_set(p, 1, 0, 0, 0);
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-de, mat, mat);
    mat3_mul_vec3(mat, p, p);
    convert_frame(painter->obs, frame, FRAME_VIEW, true, p, p);
    project_to_win(painter->proj, p, c);

    // Point ellipse.
    if (size_x == 0) {
        vec2_copy(c, win_pos);
        vec2_set(win_size, 0, 0);
        *win_angle = 0;
        return;
    }

    // 2. Semi major.
    vec4_set(p, 1, 0, 0, 0);
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-de, mat, mat);
    if (!isnan(angle)) mat3_rx(-angle, mat, mat);
    mat3_iscale(mat, 1.0, size_y / size_x, 1.0);
    mat3_rz(size_x / 2.0, mat, mat);
    mat3_mul_vec3(mat, p, p);
    vec3_normalize(p, p);
    convert_frame(painter->obs, frame, FRAME_VIEW, true, p, p);
    project_to_win(painter->proj, p, a);

    // 3. Semi minor.
    vec4_set(p, 1, 0, 0, 0);
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-de, mat, mat);
    if (!isnan(angle)) mat3_rx(-angle, mat, mat);
    mat3_iscale(mat, 1.0, size_y / size_x, 1.0);
    mat3_rx(-M_PI / 2, mat, mat);
    mat3_rz(size_x / 2.0, mat, mat);
    mat3_mul_vec3(mat, p, p);
    vec3_normalize(p, p);
    convert_frame(painter->obs, frame, FRAME_VIEW, true, p, p);
    project_to_win(painter->proj, p, b);

    vec2_copy(c, win_pos);
    vec2_sub(a, c, a);
    vec2_sub(b, c, b);
    *win_angle = isnan(angle) ? 0 : atan2(a[1], a[0]);
    win_size[0] = 2 * vec2_norm(a);
    win_size[1] = 2 * vec2_norm(b);
}

/*
 * Check if a position in windows coordinates is visible or not.
 */
static bool is_visible_win(const double pos[3], const double win_size[2])
{
    return pos[0] >= 0 && pos[0] < win_size[0] &&
           pos[1] >= 0 && pos[1] < win_size[1] &&
           pos[2] >= 0 && pos[2] <= 1;
}

bool painter_project(const painter_t *painter, int frame,
                     const double pos[3], bool at_inf, bool clip_first,
                     double win_pos[2]) {
    double v[3];
    if (clip_first) {
        if (painter_is_point_clipped_fast(painter, frame, pos, at_inf))
            return false;
    }
    convert_frame(painter->obs, frame, FRAME_VIEW, at_inf, pos, v);
    if (!project_to_win(painter->proj, v, v))
        return false;
    vec2_copy(v, win_pos);
    return is_visible_win(v, painter->proj->window_size);
}

bool painter_unproject(const painter_t *painter, int frame,
                     const double win_pos[2], double pos[3]) {
    double p[4] = {win_pos[0], win_pos[1], 0};
    bool ret;
    ret = unproject(painter->proj, p, p);
    vec3_normalize(p, p);
    convert_frame(painter->obs, FRAME_VIEW, frame, true, p, pos);
    return ret;
}
