/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "utils/gl.h"

// We keep all the text textures in a cache so that we don't have to recreate
// them each time.
typedef struct tex_cache tex_cache_t;
struct tex_cache {
    tex_cache_t *next, *prev;
    double      size;
    char        *text;
    bool        in_use;
    texture_t   *tex;
};

typedef struct {
    GLuint prog;

    GLuint a_pos_l;
    GLuint a_vpos_l;
    GLuint a_tex_pos_l;
    GLuint a_normal_l;
    GLuint a_tangent_l;
    GLuint a_color_l;
    GLuint a_shift_l;

    GLuint u_tex_l;
    GLuint u_normal_tex_l;
    GLuint u_has_normal_tex_l;
    GLuint u_color_l;
    GLuint u_smooth_l;
    GLuint u_mv_l;
    GLuint u_stripes_l;

    GLuint u_light_dir_l;
    GLuint u_light_emit_l;
    GLuint u_shadow_brightness_l; // For planets.
} prog_t;

// The buffer struct contains the opengl arrays used for a render call,
// and the offset for each attribute used.
typedef struct buffer_t {
    GLuint  array_buffer;
    GLuint  index_buffer;
    int     nb_vertices;
    GLenum  mode;
    int     offset;
    int     offset_pos;
    int     offset_vpos;
    int     offset_tex_pos;
    int     offset_normal;
    int     offset_tangent;
    int     offset_color;
    int     offset_shift;
} buffer_t;

typedef struct renderer_gl {
    renderer_t  rend;

    int     fb_size[2];

    struct {
        prog_t  points;
        prog_t  simple_planet;
        prog_t  blit_proj;
        prog_t  blit_tag;
        prog_t  planet;
    } progs;

    int     projection_type;    // The current shader projection.

    texture_t   *white_tex;
    tex_cache_t *tex_cache;
} renderer_gl_t;

// Hold all the possible attributes for the render_buffer function.
typedef struct render_buffer_args {
    const double *color;
    const texture_t *tex;
    const texture_t *normalmap;
    const double (*mv)[4][4];
    double smooth;
    const double *light_dir;
    const double *light_emit;
    double shadow_brightness;
    int flags;
    double stripes; // Number of stripes for lines (0 for no stripes).
} render_buffer_args_t;

static void render_buffer(renderer_gl_t *rend,
                          const buffer_t *buff, int n,
                          const prog_t *prog,
                          const render_buffer_args_t *args);

static void init_prog(prog_t *prog, const char *vert, const char *frag,
                      const char *include);

// XXX: we shouldn't use a struct for the vertex buffers, since the
// arrays used depend of the call (for example we might not want to have
// any tangent data).
typedef struct vertex_gl {
    GLfloat pos[4]       __attribute__((aligned(4)));
    GLfloat vpos[4]      __attribute__((aligned(4)));
    GLfloat tex_pos[2]   __attribute__((aligned(4)));
    GLubyte color[4]     __attribute__((aligned(4)));
    GLfloat normal[3]    __attribute__((aligned(4)));
    GLfloat tangent[3]   __attribute__((aligned(4)));
} vertex_gl_t;

typedef struct {
    GLfloat pos[4]       __attribute__((aligned(4)));
    GLfloat tex_pos[2]   __attribute__((aligned(4)));
    GLfloat shift[2]     __attribute__((aligned(4)));
    GLubyte color[4]     __attribute__((aligned(4)));
} point_vertex_gl_t;

static bool color_is_white(const double c[4])
{
    return c[0] == 1.0 && c[1] == 1.0 && c[2] == 1.0 && c[3] == 1.0;
}

static void render_free_buffer(void *ptr)
{
    buffer_t *buffer = ptr;
    GL(glDeleteBuffers(1, &buffer->array_buffer));
    GL(glDeleteBuffers(1, &buffer->index_buffer));
    free(ptr);
}

static void prepare(renderer_t *rend_, int w, int h)
{
    renderer_gl_t *rend = (void*)rend_;
    tex_cache_t *ctex, *tmp;

    DL_FOREACH_SAFE(rend->tex_cache, ctex, tmp) {
        if (!ctex->in_use) {
            DL_DELETE(rend->tex_cache, ctex);
            texture_delete(ctex->tex);
            free(ctex->text);
            free(ctex);
        }
    }

    GL(glClearColor(0.0, 0.0, 0.0, 1.0));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    GL(glViewport(0, 0, w, h));
    rend->fb_size[0] = w;
    rend->fb_size[1] = h;

    DL_FOREACH(rend->tex_cache, ctex)
        ctex->in_use = false;

}

static void points(renderer_t *rend_,
                   const painter_t *painter,
                   int frame,
                   int n,
                   const point_t *points)
{
    renderer_gl_t *rend = (void*)rend_;
    int i, j;
    point_vertex_gl_t (*quads)[4];
    uint16_t *indices;
    point_t p;
    buffer_t *buffer = NULL;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };
    const double *s = painter->proj->scaling;

    quads = calloc(n, sizeof(*quads));
    indices = calloc(n * 6, sizeof(*indices));
    buffer = calloc(1, sizeof(*buffer));
    buffer->nb_vertices = n * 4;
    for (i = 0; i < n; i++) {
        p = points[i];
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0, p.pos, p.pos);
        project(painter->proj, PROJ_TO_NDC_SPACE, 3, p.pos, p.pos);
        for (j = 0; j < 4; j++) {
            quads[i][j].pos[3] = 1;
            vec3_to_float(p.pos, quads[i][j].pos);
            quads[i][j].shift[0] = (j % 2 - 0.5) * 2 * tan(p.size / 2) / s[0];
            quads[i][j].shift[1] = (j / 2 - 0.5) * 2 * tan(p.size / 2) / s[1];

            quads[i][j].tex_pos[0] = j % 2;
            quads[i][j].tex_pos[1] = j / 2;
            quads[i][j].color[0] = p.color[0] * 255;
            quads[i][j].color[1] = p.color[1] * 255;
            quads[i][j].color[2] = p.color[2] * 255;
            quads[i][j].color[3] = p.color[3] * 255;
        }
        // Add the point int the global list of rendered points.
        // XXX: could be done in the painter.
        if (p.id[0]) {
            p.pos[0] = (+p.pos[0] + 1) / 2 * core->win_size[0];
            p.pos[1] = (-p.pos[1] + 1) / 2 * core->win_size[1];
            p.size = tan(p.size / 2) / s[0] * core->win_size[0];
            utarray_push_back(core->rend_points, &p);
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < 6; j++) {
            indices[i * 6 + j] = INDICES[j] + i * 4;
        }
    }
    buffer->mode = GL_TRIANGLES;
    buffer->offset = sizeof(**quads);
    buffer->offset_pos = offsetof(typeof(**quads), pos);
    buffer->offset_tex_pos = offsetof(typeof(**quads), tex_pos);
    buffer->offset_color = offsetof(typeof(**quads), color);
    buffer->offset_shift = offsetof(typeof(**quads), shift);

    GL(glGenBuffers(1, &buffer->array_buffer));
    GL(glGenBuffers(1, &buffer->index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, n * sizeof(*quads), quads,
                    GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_buffer));

    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, n * 6 * sizeof(*indices),
                    indices, GL_DYNAMIC_DRAW));
    free(quads);
    free(indices);

    render_buffer(rend, buffer, 6 * n, &rend->progs.points,
                  &(render_buffer_args_t) {
                      .color = painter->color,
                      .smooth = painter->points_smoothness,
                  });
    render_free_buffer(buffer);
}

static void compute_tangent_light_dir(
        const double pos[3], double view_mat[4][4],
        const projection_t *proj,
        const double light_dir[3], double out[3])
{
    double p1[3], p2[3];
    mat4_mul_vec3(view_mat, pos, p1);
    project(proj, 0, 2, p1, p1);
    vec3_add(pos, light_dir, p2);
    mat4_mul_vec3(view_mat, p2, p2);
    project(proj, 0, 2, p2, p2);

    vec2_sub(p2, p1, out);
    vec2_normalize(out, out);
    out[2] = -vec3_dot(pos, light_dir);
    vec2_mul(sqrt(1.0 - (out[2] * out[2])), out, out);
    vec3_normalize(out, out);
}

static void planet(renderer_t           *rend_,
                   const double         pos[3],
                   double               size,
                   const double         color[4],
                   const double         light_dir[3],
                   double               shadow_brightness,
                   double               view_mat[4][4],
                   const projection_t   *proj)
{
    renderer_gl_t *rend = (void*)rend_;
    int i;
    point_vertex_gl_t quad[4];
    uint16_t indices[6];
    double p[4], tang_light_dir[3];
    buffer_t *buffer = NULL;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };

    compute_tangent_light_dir(pos, view_mat, proj, light_dir, tang_light_dir);
    buffer = calloc(1, sizeof(*buffer));
    buffer->nb_vertices = 4;
    mat4_mul_vec3(view_mat, pos, p);
    project(proj, 0, 4, p, p);

    for (i = 0; i < 4; i++) {
        vec4_to_float(p, quad[i].pos);
        quad[i].shift[0] = (i % 2 - 0.5) * 2 * tan(size / 2) /
                           proj->scaling[0];
        quad[i].shift[1] = (i / 2 - 0.5) * 2 * tan(size / 2) /
                           proj->scaling[1];
        quad[i].tex_pos[0] = i % 2;
        quad[i].tex_pos[1] = i / 2;
        quad[i].color[0] = color[0] * 255;
        quad[i].color[1] = color[1] * 255;
        quad[i].color[2] = color[2] * 255;
        quad[i].color[3] = color[3] * 255;
    }
    for (i = 0; i < 6; i++) {
        indices[i] = INDICES[i];
    }
    buffer->mode = GL_TRIANGLES;
    buffer->offset = sizeof(*quad);
    buffer->offset_pos = offsetof(typeof(*quad), pos);
    buffer->offset_tex_pos = offsetof(typeof(*quad), tex_pos);
    buffer->offset_color = offsetof(typeof(*quad), color);
    buffer->offset_shift = offsetof(typeof(*quad), shift);

    GL(glGenBuffers(1, &buffer->array_buffer));
    GL(glGenBuffers(1, &buffer->index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad,
                    GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_buffer));

    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(*indices),
                    indices, GL_DYNAMIC_DRAW));

    render_buffer(rend, buffer, 6, &rend->progs.simple_planet,
                  &(render_buffer_args_t) {
                      .color = color,
                      .light_dir = tang_light_dir,
                      .shadow_brightness = shadow_brightness,
                  });
    render_free_buffer(buffer);
}

static void compute_tangent(const double uv[2], const projection_t *tex_proj,
                            double out[3])
{
    double uv1[2], uv2[2], p1[4], p2[4], tangent[3];
    const double delta = 0.1;
    vec2_copy(uv, uv1);
    vec2_copy(uv, uv2);
    uv2[0] += delta;
    project(tex_proj, PROJ_BACKWARD, 4, uv1, p1);
    project(tex_proj, PROJ_BACKWARD, 4, uv2, p2);
    vec3_sub(p2, p1, tangent);
    vec3_normalize(tangent, out);
}

static void quad(renderer_t          *rend_,
                 const painter_t     *painter,
                 int                 frame,
                 texture_t           *tex,
                 texture_t           *normalmap,
                 double              uv[4][2],
                 int                 grid_size,
                 const projection_t  *tex_proj)
{
    renderer_gl_t *rend = (void*)rend_;
    int n, i, j, k;
    double duvx[2], duvy[2], mv[4][4];
    buffer_t *buffer;
    vertex_gl_t *grid;
    uint16_t *indices;
    double p[4], normal[4], tangent[4];
    bool quad_cut_inv = painter->flags & PAINTER_QUAD_CUT_INV;

    // Positions of the triangles for both regular and inverted triangles.
    const int INDICES[2][6][2] = {
        { {0, 0}, {0, 1}, {1, 0},
          {1, 1}, {1, 0}, {0, 1} },
        { {1, 0}, {0, 0}, {1, 1},
          {0, 1}, {1, 1}, {0, 0} },
    };

    tex = tex ?: rend->white_tex;
    buffer = calloc(1, sizeof(*buffer));
    n = grid_size + 1;
    buffer->nb_vertices = n * n;
    grid = calloc(n * n, sizeof(*grid));
    indices = calloc(grid_size * grid_size * 6, sizeof(*indices));
    vec2_sub(uv[1], uv[0], duvx);
    vec2_sub(uv[2], uv[0], duvy);

    for (i = 0; i < n; i++) for (j = 0; j < n; j++) {
        vec4_set(p, uv[0][0], uv[0][1], 0, 1);
        vec2_addk(p, duvx, (double)j / grid_size, p);
        vec2_addk(p, duvy, (double)i / grid_size, p);
        grid[i * n + j].tex_pos[0] = p[0] * tex->w / tex->tex_w;
        grid[i * n + j].tex_pos[1] = p[1] * tex->h / tex->tex_h;

        if (normalmap) {
            compute_tangent(p, tex_proj, tangent);
            vec3_to_float(tangent, grid[i * n + j].tangent);
        }

        if (tex && (tex->flags & TF_FLIPPED))
            grid[i * n + j].tex_pos[1] = 1.0 - grid[i * n + j].tex_pos[1];
        project(tex_proj, PROJ_BACKWARD, 4, p, p);
        vec3_copy(p, normal);
        vec3_normalize(normal, normal);
        vec3_to_float(normal, grid[i * n + j].normal);
        mat4_mul_vec4(*painter->transform, p, p);
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0, p, p);

        vec4_to_float(p, grid[i * n + j].vpos);
        project(painter->proj, 0, 4, p, p);
        vec4_to_float(p, grid[i * n + j].pos);
        memcpy(grid[i * n + j].color, (uint8_t[]){255, 255, 255, 255}, 4);
    }

    for (i = 0; i < grid_size; i++) for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            indices[(i * grid_size + j) * 6 + k] =
                (INDICES[quad_cut_inv ? 1 : 0][k][1] + i) * n +
                (INDICES[quad_cut_inv ? 1 : 0][k][0] + j);
        }
    }

    buffer->mode = GL_TRIANGLES;
    buffer->offset = sizeof(vertex_gl_t);
    buffer->offset_pos = offsetof(vertex_gl_t, pos);
    buffer->offset_vpos = offsetof(vertex_gl_t, vpos);
    buffer->offset_tex_pos = offsetof(vertex_gl_t, tex_pos);
    buffer->offset_color = offsetof(vertex_gl_t, color);
    buffer->offset_normal = offsetof(vertex_gl_t, normal);
    buffer->offset_tangent = offsetof(vertex_gl_t, tangent);

    GL(glGenBuffers(1, &buffer->array_buffer));
    GL(glGenBuffers(1, &buffer->index_buffer));

    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, n * n * sizeof(*grid), grid,
                    GL_STATIC_DRAW));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    grid_size * grid_size * 6 * sizeof(*indices), indices,
                    GL_STATIC_DRAW));
    free(grid);
    free(indices);

    mat4_set_identity(mv);
    mat4_mul(mv, painter->obs->ro2v, mv);
    mat4_mul(mv, *painter->transform, mv);

    render_buffer(rend, buffer, grid_size * grid_size * 6,
                  (painter->flags & PAINTER_PLANET_SHADER) ?
                     &rend->progs.planet : &rend->progs.blit_proj,
                  &(render_buffer_args_t) {
                      .color = painter->color,
                      .tex = tex,
                      .normalmap = normalmap,
                      .mv = &mv,
                      .light_dir = *painter->light_dir,
                      .light_emit = *painter->light_emit,
                      .flags = painter->flags
                  });
    render_free_buffer(buffer);
}

static void texture2(renderer_gl_t *rend, const texture_t *tex,
                     double uv[4][2], double pos[4][2],
                     const double color[4])
{
    int i;
    vertex_gl_t quad[4];
    buffer_t *buffer;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };

    buffer = calloc(1, sizeof(*buffer));
    buffer->nb_vertices = 4;

    for (i = 0; i < 4; i++) {
        vec2_to_float(pos[i], quad[i].pos);
        quad[i].pos[2] = 0;
        quad[i].pos[3] = 1;
        vec2_to_float(uv[i], quad[i].tex_pos);
        quad[i].color[0] = 255;
        quad[i].color[1] = 255;
        quad[i].color[2] = 255;
        quad[i].color[3] = 255;
    }

    buffer->mode = GL_TRIANGLES;
    buffer->offset = sizeof(vertex_gl_t);
    buffer->offset_pos = offsetof(vertex_gl_t, pos);
    buffer->offset_tex_pos = offsetof(vertex_gl_t, tex_pos);
    buffer->offset_color = offsetof(vertex_gl_t, color);

    GL(glGenBuffers(1, &buffer->array_buffer));
    GL(glGenBuffers(1, &buffer->index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES), INDICES,
                    GL_DYNAMIC_DRAW));

    render_buffer(rend, buffer, 6,
                  &rend->progs.blit_tag,
                  &(render_buffer_args_t) {
                      .color = color,
                      .tex = tex,
                  });
    render_free_buffer(buffer);
}

static void texture(renderer_t *rend_,
                    const texture_t *tex,
                    double uv[4][2],
                    const double pos[2],
                    double size,
                    const double color[4],
                    double angle)
{
    renderer_gl_t *rend = (void*)rend_;
    int i;
    double verts[4][2], w, h;
    w = size;
    h = size * tex->h / tex->w;
    for (i = 0; i < 4; i++) {
        verts[i][0] = (i % 2 - 0.5);
        verts[i][1] = (i / 2 - 0.5);
        if (angle != 0.0) vec2_rotate(angle, verts[i], verts[i]);
        verts[i][0] = pos[0] + verts[i][0] * w / rend->fb_size[0] * 2;
        verts[i][1] = pos[1] + verts[i][1] * h / rend->fb_size[1] * 2;
    }
    texture2(rend, tex, uv, verts, color);
}

static void text(renderer_t *rend_, const char *text, const double pos[2],
                 double size, const double color[4], int out_size[2])
{
    renderer_gl_t *rend = (void*)rend_;
    double uv[4][2], verts[4][2];
    double p[2];
    const double oversample = 2;
    uint8_t *img;
    const int *fb_size = rend->fb_size;
    int i, w, h;
    tex_cache_t *ctex;
    texture_t *tex;

    DL_FOREACH(rend->tex_cache, ctex) {
        if (ctex->size == size && strcmp(ctex->text, text) == 0) break;
    }
    if (!ctex) {
        img = font_render(text, size * oversample, &w, &h);
        ctex = calloc(1, sizeof(*ctex));
        ctex->size = size;
        ctex->text = strdup(text);
        ctex->tex = texture_from_data(img, w, h, 1, 0, 0, w, h, 0);
        free(img);
        DL_APPEND(rend->tex_cache, ctex);
    }

    ctex->in_use = true;
    if (out_size) {
        out_size[0] = ctex->tex->w / oversample;
        out_size[1] = ctex->tex->h / oversample;
    }
    if (!pos) return;

    tex = ctex->tex;
    vec2_copy(pos, p);

    for (i = 0; i < 4; i++) {
        uv[i][0] = ((i % 2) * tex->w) / (double)tex->tex_w;
        uv[i][1] = ((i / 2) * tex->h) / (double)tex->tex_h;
        verts[i][0] = p[0] * fb_size[0] / 2.0 + (i % 2) * tex->w / oversample;
        verts[i][1] = p[1] * fb_size[1] / 2.0 + (i / 2) * tex->h / oversample;
        verts[i][0] = verts[i][0] / (fb_size[0] / 2);
        verts[i][1] = verts[i][1] / (fb_size[1] / 2);
    }

    texture2(rend, tex, uv, verts, color);
}

static void line(renderer_t           *rend_,
                 const painter_t      *painter,
                 int                  frame,
                 double               line[2][4],
                 int                  nb_segs,
                 const projection_t   *line_proj)
{
    renderer_gl_t *rend = (void*)rend_;
    int i, n;
    double k, pos[4], width;
    vertex_gl_t *verts;
    buffer_t *buffer;
    uint16_t *indices;
    n = nb_segs * 2;
    width = painter->lines_width ?: 1;

    buffer = calloc(1, sizeof(*buffer));
    verts = calloc(n, sizeof(*verts));
    indices = calloc(n, sizeof(*indices));
    buffer->nb_vertices = n;
    for (i = 0; i < n; i++) {
        k = ((i / 2) + (i % 2)) / (double)nb_segs;
        verts[i].tex_pos[0] = k;
        vec4_mix(line[0], line[1], k, pos);
        if (line_proj)
            project(line_proj, PROJ_BACKWARD, 4, pos, pos);
        mat4_mul_vec4(*painter->transform, pos, pos);
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0, pos, pos);
        pos[3] = 1.0;
        project(painter->proj, 0, 4, pos, pos);
        vec4_to_float(pos, verts[i].pos);
        memcpy(verts[i].color, (uint8_t[]){255, 255, 255, 255}, 4);
        indices[i] = i;
    }
    buffer->mode = GL_LINES;
    buffer->offset = sizeof(vertex_gl_t);
    buffer->offset_tex_pos = offsetof(vertex_gl_t, tex_pos);
    buffer->offset_pos = offsetof(vertex_gl_t, pos);
    buffer->offset_color = offsetof(vertex_gl_t, color);
    GL(glGenBuffers(1, &buffer->array_buffer));
    GL(glGenBuffers(1, &buffer->index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, n * sizeof(*verts), verts,
                    GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    n * sizeof(*indices), indices,
                    GL_STATIC_DRAW));
    free(verts);
    free(indices);
    GL(glLineWidth(width));
    render_buffer(rend, buffer, n,
                  &rend->progs.blit_proj,
                  &(render_buffer_args_t) {
                      .color = painter->color,
                      .tex = rend->white_tex,
                      .stripes = painter->lines_stripes,
                  });
    render_free_buffer(buffer);
}

static void render_buffer(renderer_gl_t *rend, const buffer_t *buff, int n,
                          const prog_t *prog,
                          const render_buffer_args_t *args)
{
    float mvf[16];
    const double white[4] = {1, 1, 1, 1};
    const double *color = args->color ?: white;
    const double *light_dir = args->light_dir;
    const double *light_emit = args->light_dir;

    if (args->tex) {
        GL(glActiveTexture(GL_TEXTURE0));
        GL(glBindTexture(GL_TEXTURE_2D, args->tex->id));
        switch (args->tex->format) {
        case GL_LUMINANCE:
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case GL_RGB:
            if (color[3] == 1.0) {
                GL(glDisable(GL_BLEND));
            } else {
                GL(glEnable(GL_BLEND));
                GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            }
            break;
        case GL_RGBA:
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case GL_LUMINANCE_ALPHA:
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case GL_ALPHA:
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        default:
            assert(false);
            break;
        }
    } else {
        GL(glEnable(GL_BLEND));
        GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    }

    if (args->flags & PAINTER_ADD) {
        GL(glEnable(GL_BLEND));
        if (color_is_white(color))
            GL(glBlendFunc(GL_ONE, GL_ONE));
        else {
            GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE));
            GL(glBlendColor(color[0] * color[3],
                            color[1] * color[3],
                            color[2] * color[3], color[3]));
        }
    }

    GL(glEnable(GL_CULL_FACE));
    GL(glUseProgram(prog->prog));

    GL(glActiveTexture(GL_TEXTURE1));
    if (args->normalmap) {
        GL(glBindTexture(GL_TEXTURE_2D, args->normalmap->id));
        GL(glUniform1i(prog->u_has_normal_tex_l, 1));
    } else {
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));
        GL(glUniform1i(prog->u_has_normal_tex_l, 0));
    }

    GL(glBindBuffer(GL_ARRAY_BUFFER, buff->array_buffer));
    GL(glVertexAttribPointer(prog->a_pos_l, 4, GL_FLOAT, false,
                    buff->offset, (void*)(long)buff->offset_pos));

    GL(glUniform1f(prog->u_smooth_l, args->smooth));
    GL(glUniform4f(prog->u_color_l, color[0], color[1], color[2], color[3]));
    if (light_dir) {
        GL(glUniform3f(prog->u_light_dir_l,
                       light_dir[0], light_dir[1], light_dir[2]));
    }
    if (light_emit) {
        GL(glUniform3f(prog->u_light_emit_l,
                       light_emit[0], light_emit[1], light_emit[2]));
    } else {
        GL(glUniform3f(prog->u_light_emit_l, 0, 0, 0));
    }
    GL(glUniform1f(prog->u_shadow_brightness_l, args->shadow_brightness));

    if (args->mv && prog->u_mv_l != -1) {
        mat4_to_float(*args->mv, mvf);
        GL(glUniformMatrix4fv(prog->u_mv_l, 1, 0, mvf));
    }
    if (prog->u_stripes_l != -1)
        GL(glUniform1f(prog->u_stripes_l, args->stripes));

    GL(glBindBuffer(GL_ARRAY_BUFFER, buff->array_buffer));

    GL(glEnableVertexAttribArray(prog->a_pos_l));
    if (prog->a_tex_pos_l != -1) {
        GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, false,
               buff->offset, (void*)(long)buff->offset_tex_pos));
        GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    }
    if (prog->a_color_l != -1) {
        GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                buff->offset, (void*)(long)buff->offset_color));
        GL(glEnableVertexAttribArray(prog->a_color_l));
    }
    if (prog->a_shift_l != -1) {
        GL(glVertexAttribPointer(prog->a_shift_l, 2, GL_FLOAT, false,
                buff->offset, (void*)(long)buff->offset_shift));
        GL(glEnableVertexAttribArray(prog->a_shift_l));
    }
    if (prog->a_normal_l != -1) {
        GL(glVertexAttribPointer(prog->a_normal_l, 3, GL_FLOAT, false,
               buff->offset, (void*)(long)buff->offset_normal));
        GL(glEnableVertexAttribArray(prog->a_normal_l));
    }
    if (prog->a_tangent_l != -1) {
        GL(glVertexAttribPointer(prog->a_tangent_l, 3, GL_FLOAT, false,
               buff->offset, (void*)(long)buff->offset_tangent));
        GL(glEnableVertexAttribArray(prog->a_tangent_l));
    }
    if (prog->a_vpos_l != -1) {
        GL(glVertexAttribPointer(prog->a_vpos_l, 4, GL_FLOAT, false,
               buff->offset, (void*)(long)buff->offset_vpos));
        GL(glEnableVertexAttribArray(prog->a_vpos_l));
    }
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff->index_buffer));
    GL(glDrawElements(buff->mode, n, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    if (prog->a_tex_pos_l != -1)
        GL(glDisableVertexAttribArray(prog->a_tex_pos_l));
    if (prog->a_color_l != -1)
        GL(glDisableVertexAttribArray(prog->a_color_l));
    if (prog->a_shift_l != -1)
        GL(glDisableVertexAttribArray(prog->a_shift_l));
    if (prog->a_normal_l != -1)
        GL(glDisableVertexAttribArray(prog->a_normal_l));
    if (prog->a_tangent_l != -1)
        GL(glDisableVertexAttribArray(prog->a_tangent_l));
    if (prog->a_vpos_l != -1)
        GL(glDisableVertexAttribArray(prog->a_vpos_l));
}

static void init_prog(prog_t *p, const char *vert, const char *frag,
                      const char *include)
{
    assert(vert);
    assert(frag);
    p->prog = gl_create_program(vert, frag, include);
    GL(glUseProgram(p->prog));
#define UNIFORM(x) p->x##_l = glGetUniformLocation(p->prog, #x);
#define ATTRIB(x) p->x##_l = glGetAttribLocation(p->prog, #x)
    UNIFORM(u_tex);
    UNIFORM(u_normal_tex);
    UNIFORM(u_has_normal_tex);
    UNIFORM(u_color);
    UNIFORM(u_smooth);
    UNIFORM(u_light_dir);
    UNIFORM(u_light_emit);
    UNIFORM(u_shadow_brightness);
    UNIFORM(u_mv);
    UNIFORM(u_stripes);
    ATTRIB(a_pos);
    ATTRIB(a_vpos);
    ATTRIB(a_tex_pos);
    ATTRIB(a_normal);
    ATTRIB(a_tangent);
    ATTRIB(a_color);
    ATTRIB(a_shift);
#undef ATTRIB
#undef UNIFORM
    // Default texture locations:
    GL(glUniform1i(p->u_tex_l, 0));
    GL(glUniform1i(p->u_normal_tex_l, 1));
}

static texture_t *create_white_texture(int w, int h)
{
    uint8_t *data;
    texture_t *tex;
    data = malloc(w * h * 3);
    memset(data, 255, w * h * 3);
    tex = texture_from_data(data, w, h, 3, 0, 0, w, h, 0);
    free(data);
    return tex;
}

renderer_t* render_gl_create(void)
{
    renderer_gl_t *rend;
    rend = calloc(1, sizeof(*rend));
    rend->white_tex = create_white_texture(16, 16);

    // Create all the shaders programs.
    init_prog(&rend->progs.points,
              asset_get_data("asset://shaders/points.vert", NULL, NULL),
              asset_get_data("asset://shaders/points.frag", NULL, NULL), NULL);
    init_prog(&rend->progs.simple_planet,
              asset_get_data("asset://shaders/simple_planet.vert", NULL, NULL),
              asset_get_data("asset://shaders/simple_planet.frag", NULL, NULL),
              NULL);
    init_prog(&rend->progs.blit_proj,
              asset_get_data("asset://shaders/blit.vert", NULL, NULL),
              asset_get_data("asset://shaders/blit.frag", NULL, NULL), NULL);
    init_prog(&rend->progs.blit_tag,
              asset_get_data("asset://shaders/blit_tag.vert", NULL, NULL),
              asset_get_data("asset://shaders/blit_tag.frag", NULL, NULL),
                             NULL);
    init_prog(&rend->progs.planet,
              asset_get_data("asset://shaders/planet.vert", NULL, NULL),
              asset_get_data("asset://shaders/planet.frag", NULL, NULL), NULL);

    rend->rend.prepare = prepare;
    rend->rend.points = points;
    rend->rend.planet = planet;
    rend->rend.quad = quad;
    rend->rend.texture = texture;
    rend->rend.text = text;
    rend->rend.line = line;

    return &rend->rend;
}
