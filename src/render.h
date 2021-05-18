/* Stellarium Web Engine - Copyright (c) 2021 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef RENDER_GL_H
#define RENDER_GL_H

#include <stdbool.h>
#include <stdint.h>
#include "json.h"


typedef struct renderer renderer_t;
typedef struct painter painter_t;
typedef struct point point_t;
typedef struct point_3d point_3d_t;
typedef struct uv_map uv_map_t;
typedef struct texture texture_t;
typedef struct projection projection_t;
typedef struct obj obj_t;

// TODO: document those functions.

renderer_t* render_create(void);

void render_prepare(renderer_t *rend,
                    const projection_t *proj,
                    double win_w, double win_h, double scale,
                    bool cull_flipped);

void render_finish(renderer_t *rend);

void render_points_2d(renderer_t *rend, const painter_t *painter,
                      int n, const point_t *points);

void render_points_3d(renderer_t *rend, const painter_t *painter,
                      int n, const point_3d_t *points);

void render_quad(renderer_t *rend, const painter_t *painter,
                 int frame, int grid_size, const uv_map_t *map);

void render_texture(renderer_t *rend, const texture_t  *tex,
                    double uv[4][2], const double pos[2], double size,
                    const double color[4], double angle);

void render_text(renderer_t *rend, const painter_t *painter,
                 const char *text, const double win_pos[2],
                 const double view_pos[3],
                 int align, int effects, double size,
                 const double color[4], double angle,
                 double bounds[4]);

void render_line(renderer_t *rend, const painter_t *painter,
                 const double (*pos)[3], const double (*win)[3], int size);

void render_mesh(renderer_t *rend, const painter_t *painter,
                 int frame, int mode, int verts_count,
                 const double verts[][3], int indices_count,
                 const uint16_t indices[], bool use_stencil);

void render_ellipse_2d(renderer_t *rend, const painter_t *painter,
                       const double pos[2], const double size[2],
                       double angle, double dashes);

void render_rect_2d(renderer_t *rend, const painter_t *painter,
                    const double pos[2], const double size[2],
                    double angle);

void render_line_2d(renderer_t *rend, const painter_t *painter,
                    const double p1[2], const double p2[2]);

void render_model_3d(renderer_t *rend, const painter_t *painter,
                     const char *model, const double model_mat[4][4],
                     const double view_mat[4][4], const double proj_mat[4][4],
                     const double light_dir[3], json_value *args);

#endif // RENDER_GL_H
