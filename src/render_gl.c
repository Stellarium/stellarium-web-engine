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
    GLuint u_shadow_color_tex_l;

    GLuint u_has_normal_tex_l;
    GLuint u_material_l;
    GLuint u_is_moon_l;
    GLuint u_color_l;
    GLuint u_contrast_l;
    GLuint u_smooth_l;
    GLuint u_mv_l;
    GLuint u_stripes_l;

    // For planets.
    GLuint u_sun_l;
    GLuint u_light_emit_l;
    GLuint u_shadow_brightness_l;
    GLuint u_shadow_spheres_nb_l;
    GLuint u_shadow_spheres_l;
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
        prog_t  blit_proj;
        prog_t  blit_tag;
        prog_t  planet;
    } progs;

    int     projection_type;    // The current shader projection.

    texture_t   *white_tex;
    tex_cache_t *tex_cache;
} renderer_gl_t;

// Hold all the possible attributes for the render_buffer function.
// XXX: we should probably just pass the painter!
typedef struct render_buffer_args {
    const double *color;
    const texture_t *tex;
    const texture_t *normalmap;
    const texture_t *shadow_color_tex;
    const double (*mv)[4][4];
    double smooth;
    double contrast;
    const double *sun;
    const double *light_emit;
    double shadow_brightness;
    int flags;
    double stripes; // Number of stripes for lines (0 for no stripes).
    const double (*depth_range)[2];
    int shadow_spheres_nb;
    double (*shadow_spheres)[4];
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
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
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
    double p[4], normal[4], tangent[4], z;
    bool quad_cut_inv = painter->flags & PAINTER_QUAD_CUT_INV;
    const double *depth_range = (const double*)painter->depth_range;

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
        z = p[2];
        vec4_to_float(p, grid[i * n + j].vpos);
        project(painter->proj, 0, 4, p, p);
        // For simplicity the projection doesn't take into account the depth
        // range, so we apply it manually after if needed.
        if (depth_range) {
            p[2] = (-z - depth_range[0]) / (depth_range[1] - depth_range[0]);
        }
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

    // Compute modelview matrix.
    mat4_set_identity(mv);
    if (frame == FRAME_OBSERVED) mat4_mul(mv, painter->obs->ro2v, mv);
    mat4_mul(mv, *painter->transform, mv);

    render_buffer(rend, buffer, grid_size * grid_size * 6,
                  (painter->flags & PAINTER_PLANET_SHADER) ?
                     &rend->progs.planet : &rend->progs.blit_proj,
                  &(render_buffer_args_t) {
                      .color = painter->color,
                      .contrast = painter->contrast,
                      .tex = tex,
                      .normalmap = normalmap,
                      .shadow_color_tex = painter->shadow_color_tex,
                      .mv = &mv,
                      .sun = (double*)painter->sun,
                      .light_emit = (double*)painter->light_emit,
                      .depth_range = painter->depth_range,
                      .shadow_spheres_nb = painter->shadow_spheres_nb,
                      .shadow_spheres = painter->shadow_spheres,
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
    const double *depth_range = (const double*)painter->depth_range;

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
        double z = pos[2];
        project(painter->proj, 0, 4, pos, pos);

        if (depth_range) {
            pos[2] = (-z - depth_range[0]) / (depth_range[1] - depth_range[0]);
        }

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
                      .depth_range = painter->depth_range,
                  });
    render_free_buffer(buffer);
}

static void render_buffer(renderer_gl_t *rend, const buffer_t *buff, int n,
                          const prog_t *prog,
                          const render_buffer_args_t *args)
{
    float mf[16];
    const double white[4] = {1, 1, 1, 1};
    const double *color = args->color ?: white;
    const double *sun = args->sun;
    const double *light_emit = args->light_emit;

    if (args->tex) {
        GL(glActiveTexture(GL_TEXTURE0));
        GL(glBindTexture(GL_TEXTURE_2D, args->tex->id));
        switch (args->tex->format) {
        case GL_LUMINANCE:
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case GL_RGB:
            if (color[3] == 1.0 && !args->stripes) {
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
    if (args->depth_range) {
        GL(glEnable(GL_DEPTH_TEST));
    } else {
        GL(glDisable(GL_DEPTH_TEST));
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

    if (args->flags & PAINTER_NO_CULL_FACE)
        GL(glDisable(GL_CULL_FACE));
    else
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

    GL(glActiveTexture(GL_TEXTURE2));
    if (args->shadow_color_tex && texture_load(args->shadow_color_tex, NULL))
        GL(glBindTexture(GL_TEXTURE_2D, args->shadow_color_tex->id));
    else
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));

    GL(glBindBuffer(GL_ARRAY_BUFFER, buff->array_buffer));
    GL(glVertexAttribPointer(prog->a_pos_l, 4, GL_FLOAT, false,
                    buff->offset, (void*)(long)buff->offset_pos));

    GL(glUniform1f(prog->u_smooth_l, args->smooth));
    GL(glUniform1f(prog->u_contrast_l, args->contrast));
    GL(glUniform4f(prog->u_color_l, color[0], color[1], color[2], color[3]));
    if (sun) {
        GL(glUniform4f(prog->u_sun_l, sun[0], sun[1], sun[2], sun[3]));
    }
    // For the moment we automatically assign generic diffuse material (1) to
    // light emiting surfaces, and Oren Nayar (0) to the other.
    // This only affect planets shader.
    if (light_emit) {
        GL(glUniform3f(prog->u_light_emit_l,
                       light_emit[0], light_emit[1], light_emit[2]));
        GL(glUniform1i(prog->u_material_l, 1));
    } else {
        GL(glUniform3f(prog->u_light_emit_l, 0, 0, 0));
        GL(glUniform1i(prog->u_material_l, 0));
    }
    GL(glUniform1f(prog->u_shadow_brightness_l, args->shadow_brightness));
    GL(glUniform1i(prog->u_is_moon_l, args->flags & PAINTER_IS_MOON ? 1 : 0));

    if (args->mv && prog->u_mv_l != -1) {
        mat4_to_float(*args->mv, mf);
        GL(glUniformMatrix4fv(prog->u_mv_l, 1, 0, mf));
    }
    if (prog->u_stripes_l != -1)
        GL(glUniform1f(prog->u_stripes_l, args->stripes));

    if (prog->u_shadow_spheres_nb_l != -1)
        GL(glUniform1i(prog->u_shadow_spheres_nb_l, args->shadow_spheres_nb));
    if (args->shadow_spheres && prog->u_shadow_spheres_l != -1) {
        mat4_to_float(args->shadow_spheres, mf);
        GL(glUniformMatrix4fv(prog->u_shadow_spheres_l, 1, 0, mf));
    }

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
    UNIFORM(u_material);
    UNIFORM(u_is_moon);
    UNIFORM(u_shadow_color_tex);
    UNIFORM(u_color);
    UNIFORM(u_contrast);
    UNIFORM(u_smooth);
    UNIFORM(u_sun);
    UNIFORM(u_light_emit);
    UNIFORM(u_shadow_brightness);
    UNIFORM(u_shadow_spheres_nb);
    UNIFORM(u_shadow_spheres);
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
    GL(glUniform1i(p->u_shadow_color_tex_l, 2));
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
    rend->rend.quad = quad;
    rend->rend.texture = texture;
    rend->rend.text = text;
    rend->rend.line = line;

    return &rend->rend;
}
