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

#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_gl.h"

#include <float.h>

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
    GLuint a_mpos_l;
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
    GLuint u_depth_range_l;

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
    int     offset_mpos;
    int     offset_tex_pos;
    int     offset_normal;
    int     offset_tangent;
    int     offset_color;
    int     offset_shift;
} buffer_t;

enum {
    ITEM_LINES = 1,
    ITEM_POINTS,
    ITEM_ALPHA_TEXTURE,
    ITEM_TEXTURE,
    ITEM_PLANET,
    ITEM_VG_ELLIPSE,
    ITEM_VG_RECT,
    ITEM_VG_LINE,
};

typedef struct item item_t;
struct item
{
    int type;
    double      color[4];
    uint16_t    *indices;
    void        *buf;
    int         buf_size;
    int         nb;
    int         capacity;
    texture_t   *tex;
    int         flags;
    double      depth_range[2];

    union {
        struct {
            double width;
            double stripes;
        } lines;

        struct {
            double smooth;
        } points;

        struct {
            double contrast;
            const texture_t *normalmap;
            texture_t *shadow_color_tex;
            double mv[4][4];
            double sun[4]; // pos + radius.
            double light_emit[3];
            int    shadow_spheres_nb;
            double shadow_spheres[4][4]; // (pos + radius) * 4
            int    material;
        } planet;

        struct {
            double pos[2];
            double pos2[2];
            double size[2];
            double angle;
            double dashes;
        } vg;
    };

    item_t *next, *prev;
};

typedef struct {
    float pos[4]        __attribute__((aligned(4)));
    float tex_pos[2]    __attribute__((aligned(4)));
    uint8_t color[4]    __attribute__((aligned(4)));
} lines_buf_t;

typedef struct {
    float pos[4]        __attribute__((aligned(4)));
    float tex_pos[2]    __attribute__((aligned(4)));
    float shift[2]      __attribute__((aligned(4)));
    uint8_t color[4]    __attribute__((aligned(4)));
} points_buf_t;

typedef struct {
    float pos[2]        __attribute__((aligned(4)));
    float tex_pos[2]    __attribute__((aligned(4)));
    uint8_t color[4]    __attribute__((aligned(4)));
} texture_buf_t;

typedef struct {
    float   pos[4]      __attribute__((aligned(4)));
    float   mpos[4]     __attribute__((aligned(4)));
    float   tex_pos[2]  __attribute__((aligned(4)));
    uint8_t color[4]    __attribute__((aligned(4)));
    float   normal[3]   __attribute__((aligned(4)));
    float   tangent[3]  __attribute__((aligned(4)));
} planet_buf_t;

typedef struct renderer_gl {
    renderer_t  rend;

    int     fb_size[2];
    double  scale;

    struct {
        prog_t  points;
        prog_t  blit_proj;
        prog_t  blit_tag;
        prog_t  planet;
    } progs;

    int     projection_type;    // The current shader projection.
    double  depth_range[2];

    texture_t   *white_tex;
    tex_cache_t *tex_cache;
    NVGcontext *vg;

    item_t  *items;
} renderer_gl_t;


static void init_prog(prog_t *prog, const char *vert, const char *frag,
                      const char *include);

static bool color_is_white(const double c[4])
{
    return c[0] == 1.0 && c[1] == 1.0 && c[2] == 1.0 && c[3] == 1.0;
}

static void window_to_ndc(renderer_gl_t *rend,
                          const double win[2], double ndc[2])
{
    ndc[0] = (win[0] * rend->scale / rend->fb_size[0]) * 2 - 1;
    ndc[1] = 1 - (win[1] * rend->scale / rend->fb_size[1]) * 2;
}

static void prepare(renderer_t *rend_, double win_w, double win_h,
                    double scale)
{
    renderer_gl_t *rend = (void*)rend_;
    tex_cache_t *ctex, *tmp;

    assert(scale);
    DL_FOREACH_SAFE(rend->tex_cache, ctex, tmp) {
        if (!ctex->in_use) {
            DL_DELETE(rend->tex_cache, ctex);
            texture_release(ctex->tex);
            free(ctex->text);
            free(ctex);
        }
    }

    GL(glClearColor(0.0, 0.0, 0.0, 1.0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GL(glViewport(0, 0, win_w * scale, win_h * scale));
    rend->fb_size[0] = win_w * scale;
    rend->fb_size[1] = win_h * scale;
    rend->scale = scale;

    DL_FOREACH(rend->tex_cache, ctex)
        ctex->in_use = false;

}

static void rend_flush(renderer_gl_t *rend);
static void finish(renderer_t *rend_)
{
    renderer_gl_t *rend = (void*)rend_;
    rend_flush(rend);
}

static void flush(renderer_t *rend_)
{
    renderer_gl_t *rend = (void*)rend_;
    rend_flush(rend);
}

static item_t *get_item(renderer_gl_t *rend, int type, int nb, texture_t *tex);

static void points(renderer_t *rend_,
                   const painter_t *painter,
                   int frame,
                   int n,
                   const point_t *points)
{
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    points_buf_t *buf;
    uint16_t *indices;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };
    // Scale size from window to NDC.
    double s[2] = {2 * rend->scale / rend->fb_size[0],
                   2 * rend->scale / rend->fb_size[1]};
    int i, j, idx, ofs;
    const int MAX_POINTS = 4096;
    point_t p;
    // Adjust size so that at any smoothness value the points look more or
    // less at the same intensity.
    double sm = 1.0 / (1.0 - 0.7 * painter->points_smoothness);

    if (n > MAX_POINTS) {
        LOG_E("Try to render more than %d points: %d", MAX_POINTS, n);
        n = MAX_POINTS;
    }

    item = get_item(rend, ITEM_POINTS, n * 6, NULL);
    if (item && item->points.smooth != painter->points_smoothness)
        item = NULL;
    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_POINTS;
        item->capacity = MAX_POINTS * 6;
        item->buf = malloc(item->capacity / 6 * 4 * sizeof(*buf));
        item->indices = malloc(item->capacity * sizeof(*indices));
        vec4_copy(painter->color, item->color);
        item->points.smooth = painter->points_smoothness;
        DL_APPEND(rend->items, item);
    }

    buf = item->buf + item->buf_size;
    indices = item->indices + item->nb;
    ofs = item->buf_size / sizeof(*buf);

    for (i = 0; i < n; i++) {
        p = points[i];
        if (frame == FRAME_WINDOW) {
            window_to_ndc(rend, p.pos, p.pos);
        } else if (frame != FRAME_NDC) {
            convert_directionv4(painter->obs, frame, FRAME_VIEW, p.pos, p.pos);
            project(painter->proj, PROJ_TO_NDC_SPACE, 3, p.pos, p.pos);
        }
        for (j = 0; j < 4; j++) {
            idx = i * 4 + j;
            buf[idx].pos[3] = 1;
            vec3_to_float(p.pos, buf[i * 4 + j].pos);
            buf[idx].shift[0] = (j % 2 - 0.5) * p.size * s[0] * sm;
            buf[idx].shift[1] = (j / 2 - 0.5) * p.size * s[1] * sm;
            buf[idx].tex_pos[0] = j % 2;
            buf[idx].tex_pos[1] = j / 2;
            buf[idx].color[0] = p.color[0] * 255;
            buf[idx].color[1] = p.color[1] * 255;
            buf[idx].color[2] = p.color[2] * 255;
            buf[idx].color[3] = p.color[3] * 255;
        }
        // Add the point int the global list of rendered points.
        // XXX: could be done in the painter.
        if (p.oid) {
            p.pos[0] = (+p.pos[0] + 1) / 2 * core->win_size[0];
            p.pos[1] = (-p.pos[1] + 1) / 2 * core->win_size[1];
            areas_add_circle(core->areas, p.pos, p.size, p.oid, p.hint);
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < 6; j++) {
            indices[i * 6 + j] = ofs + INDICES[j] + i * 4;
        }
    }

    item->buf_size += n * 4 * sizeof(points_buf_t);
    item->nb += n * 6;
}

static void compute_tangent(const double uv[2], const projection_t *tex_proj,
                            double out[3])
{
    // XXX: this is what the algo should look like, except the normal map
    // texture we use (for the Moon) doesn't follow the healpix projection.
    /*
    double uv1[2], uv2[2], p1[4], p2[4], tangent[3];
    const double delta = 0.1;
    vec2_copy(uv, uv1);
    vec2_copy(uv, uv2);
    uv2[0] += delta;
    project(tex_proj, PROJ_BACKWARD, 4, uv1, p1);
    project(tex_proj, PROJ_BACKWARD, 4, uv2, p2);
    vec3_sub(p2, p1, tangent);
    vec3_normalize(tangent, out);
    */

    double p[4] = {0};
    project(tex_proj, PROJ_BACKWARD, 4, uv, p);
    vec3_cross(VEC(0, 0, 1), p, out);
}

static item_t *get_item(renderer_gl_t *rend, int type, int nb, texture_t *tex);


static void quad_planet(
                 renderer_t          *rend_,
                 const painter_t     *painter,
                 int                 frame,
                 texture_t           *tex,
                 texture_t           *normalmap,
                 double              uv[4][2],
                 int                 grid_size,
                 const projection_t  *tex_proj)
{
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    planet_buf_t *buf;
    int n, i, j, k;
    double duvx[2], duvy[2];
    uint16_t *indices;
    double p[4], normal[4] = {0}, tangent[4] = {0}, z;

    // Positions of the triangles in the quads.
    const int INDICES[6][2] = { {0, 0}, {0, 1}, {1, 0},
                                {1, 1}, {1, 0}, {0, 1} };

    tex = tex ?: rend->white_tex;
    n = grid_size + 1;

    item = calloc(1, sizeof(*item));
    item->type = ITEM_PLANET;
    item->capacity = n * n * 6;
    item->buf = calloc(n * n, sizeof(*buf));
    item->indices = calloc(item->capacity, sizeof(*indices));
    item->tex = tex;
    item->tex->ref++;
    vec4_copy(painter->color, item->color);
    item->flags = painter->flags;
    item->planet.normalmap = normalmap;
    item->planet.shadow_color_tex = painter->shadow_color_tex;
    item->planet.contrast = painter->contrast;
    item->planet.shadow_spheres_nb = painter->shadow_spheres_nb;
    if (painter->shadow_spheres_nb)
        memcpy(item->planet.shadow_spheres, painter->shadow_spheres,
               painter->shadow_spheres_nb * sizeof(*painter->shadow_spheres));
    vec4_copy(*painter->sun, item->planet.sun);
    if (painter->light_emit)
        vec3_copy(*painter->light_emit, item->planet.light_emit);

    // Compute modelview matrix.
    mat4_set_identity(item->planet.mv);
    if (frame == FRAME_OBSERVED)
        mat4_mul(item->planet.mv, painter->obs->ro2v, item->planet.mv);
    if (frame == FRAME_ICRF)
        mat4_mul(item->planet.mv, painter->obs->ri2v, item->planet.mv);
    mat4_mul(item->planet.mv, *painter->transform, item->planet.mv);

    // Set material
    if (painter->light_emit)
        item->planet.material = 1; // Generic
    else
        item->planet.material = 0; // Oren Nayar
    if (painter->flags & PAINTER_RING_SHADER)
        item->planet.material = 2; // Ring

    vec2_sub(uv[1], uv[0], duvx);
    vec2_sub(uv[2], uv[0], duvy);
    buf = item->buf;
    indices = item->indices;

    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec4_set(p, uv[0][0], uv[0][1], 0, 1);
        vec2_addk(p, duvx, (double)j / grid_size, p);
        vec2_addk(p, duvy, (double)i / grid_size, p);
        buf[i * n + j].tex_pos[0] = p[0] * tex->w / tex->tex_w;
        buf[i * n + j].tex_pos[1] = p[1] * tex->h / tex->tex_h;

        if (normalmap) {
            compute_tangent(p, tex_proj, tangent);
            mat4_mul_vec4(*painter->transform, tangent, tangent);
            vec3_to_float(tangent, buf[i * n + j].tangent);
        }

        if (tex->flags & TF_FLIPPED)
            buf[i * n + j].tex_pos[1] = 1.0 - buf[i * n + j].tex_pos[1];
        project(tex_proj, PROJ_BACKWARD, 4, p, p);

        vec3_copy(p, normal);
        mat4_mul_vec4(*painter->transform, normal, normal);
        vec3_to_float(normal, buf[i * n + j].normal);

        mat4_mul_vec4(*painter->transform, p, p);
        vec4_to_float(p, buf[i * n + j].mpos);
        convert_directionv4(painter->obs, frame, FRAME_VIEW, p, p);
        z = p[2];
        project(painter->proj, 0, 4, p, p);
        if (painter->depth_range) {
            vec2_copy(*painter->depth_range, item->depth_range);
            p[2] = -z;
        }
        vec4_to_float(p, buf[i * n + j].pos);
        memcpy(buf[i * n + j].color, (uint8_t[]){255, 255, 255, 255}, 4);
    }

    for (i = 0; i < grid_size; i++)
    for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            indices[(i * grid_size + j) * 6 + k] =
                (INDICES[k][1] + i) * n + (INDICES[k][0] + j);
        }
    }

    item->buf_size += n * n * sizeof(*buf);
    item->nb += grid_size * grid_size * 6;

    DL_APPEND(rend->items, item);
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
    item_t *item;
    texture_buf_t *buf;
    uint16_t *indices;
    int n, i, j, k;
    const int INDICES[6][2] = {
        {0, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 0}, {0, 1} };
    double p[4], tex_pos[2], duvx[2], duvy[2];

    // Special case for planet shader.
    if (painter->flags & (PAINTER_PLANET_SHADER | PAINTER_RING_SHADER))
        return quad_planet(rend_, painter, frame, tex, normalmap,
                           uv, grid_size, tex_proj);

    if (!tex) tex = rend->white_tex;
    n = grid_size + 1;

    item = calloc(1, sizeof(*item));
    item->type = ITEM_TEXTURE;
    item->capacity = n * n * 6;
    item->buf = calloc(n * n, sizeof(*buf));
    item->indices = calloc(item->capacity, sizeof(*indices));
    item->tex = tex;
    item->tex->ref++;
    vec4_copy(painter->color, item->color);
    item->flags = painter->flags;

    vec2_sub(uv[1], uv[0], duvx);
    vec2_sub(uv[2], uv[0], duvy);

    buf = item->buf;
    indices = item->indices;

    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec4_set(p, uv[0][0], uv[0][1], 0, 1);
        vec2_addk(p, duvx, (double)j / grid_size, p);
        vec2_addk(p, duvy, (double)i / grid_size, p);
        tex_pos[0] = p[0] * tex->w / tex->tex_w;
        tex_pos[1] = p[1] * tex->h / tex->tex_h;
        if (tex->border) {
            tex_pos[0] = mix((tex->border - 0.5) / tex->w,
                             1.0 - (tex->border - 0.5) / tex->w, tex_pos[0]);
            tex_pos[1] = mix((tex->border - 0.5) / tex->w,
                             1.0 - (tex->border - 0.5) / tex->h, tex_pos[1]);
        }
        if (tex->flags & TF_FLIPPED) tex_pos[1] = 1.0 - tex_pos[1];
        vec2_to_float(tex_pos, buf[i * n + j].tex_pos);

        project(tex_proj, PROJ_BACKWARD, 4, p, p);
        mat4_mul_vec4(*painter->transform, p, p);
        convert_directionv4(painter->obs, frame, FRAME_VIEW, p, p);
        project(painter->proj, 0, 4, p, p);
        vec2_to_float(p, buf[i * n + j].pos);
        memcpy(buf[i * n + j].color, (uint8_t[]){255, 255, 255, 255}, 4);
    }

    for (i = 0; i < grid_size; i++)
    for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            indices[(i * grid_size + j) * 6 + k] =
                (INDICES[k][1] + i) * n + (INDICES[k][0] + j);
        }
    }

    item->buf_size += n * n * sizeof(texture_buf_t);
    item->nb += grid_size * grid_size * 6;

    DL_APPEND(rend->items, item);
}

static void texture2(renderer_gl_t *rend, texture_t *tex,
                     double uv[4][2], double pos[4][2],
                     const double color[4])
{
    int i, ofs;
    item_t *item;
    texture_buf_t *buf;
    uint16_t *indices;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };

    item = get_item(rend, ITEM_ALPHA_TEXTURE, 6, tex);
    if (item && !vec4_equal(item->color, color)) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_ALPHA_TEXTURE;
        item->capacity = 6 * 64;
        item->buf = calloc(item->capacity / 6 * 4, sizeof(*buf));
        item->indices = calloc(item->capacity, sizeof(*indices));
        item->tex = tex;
        item->tex->ref++;
        vec4_copy(color, item->color);
        DL_APPEND(rend->items, item);
    }

    buf = item->buf + item->buf_size;
    indices = item->indices + item->nb;
    ofs = item->buf_size / sizeof(*buf);

    for (i = 0; i < 4; i++) {
        vec2_to_float(pos[i], buf[i].pos);
        vec2_to_float(uv[i], buf[i].tex_pos);
        buf[i].color[0] = 255;
        buf[i].color[1] = 255;
        buf[i].color[2] = 255;
        buf[i].color[3] = 255;
    }
    for (i = 0; i < 6; i++) {
        indices[i] = ofs + INDICES[i];
    }

    item->buf_size += 4 * sizeof(texture_buf_t);
    item->nb += 6;
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
        verts[i][0] = (i % 2 - 0.5) * w;
        verts[i][1] = (0.5 - i / 2) * h;
        if (angle != 0.0) vec2_rotate(-angle, verts[i], verts[i]);
        verts[i][0] = pos[0] + verts[i][0];
        verts[i][1] = pos[1] + verts[i][1];
        window_to_ndc(rend, verts[i], verts[i]);
    }
    texture2(rend, tex, uv, verts, color);
}

static void text(renderer_t *rend_, const char *text, const double pos[2],
                 double size, const double color[4], double angle,
                 int out_size[2])
{
    renderer_gl_t *rend = (void*)rend_;
    double uv[4][2], verts[4][2];
    double p[2];
    const double oversample = 2;
    uint8_t *img;
    int i, w, h;
    tex_cache_t *ctex;
    texture_t *tex;

    DL_FOREACH(rend->tex_cache, ctex) {
        if (ctex->size == size && strcmp(ctex->text, text) == 0) break;
    }
    if (!ctex) {
        img = (void*)sys_render_text(text, size * oversample, &w, &h);
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
        verts[i][0] = (i % 2 - 0.5) * tex->w / oversample;
        verts[i][1] = (0.5 - i / 2) * tex->h / oversample;
        vec2_rotate(angle, verts[i], verts[i]);
        verts[i][0] += p[0];
        verts[i][1] += p[1];
        window_to_ndc(rend, verts[i], verts[i]);
    }

    texture2(rend, tex, uv, verts, color);
}

static void item_points_render(renderer_gl_t *rend, const item_t *item)
{
    prog_t *prog;
    GLuint  array_buffer;
    GLuint  index_buffer;

    prog = &rend->progs.points;
    GL(glUseProgram(prog->prog));

    GL(glEnable(GL_CULL_FACE));
    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    item->nb * sizeof(*item->indices),
                    item->indices, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf_size, item->buf,
                    GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));
    GL(glUniform1f(prog->u_smooth_l, item->points.smooth));

    GL(glEnableVertexAttribArray(prog->a_pos_l));
    GL(glVertexAttribPointer(prog->a_pos_l, 4, GL_FLOAT, false,
                    sizeof(points_buf_t),
                    (void*)(long)offsetof(points_buf_t, pos)));

    GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, false,
                    sizeof(points_buf_t),
                    (void*)(long)offsetof(points_buf_t, tex_pos)));

    GL(glEnableVertexAttribArray(prog->a_color_l));
    GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                    sizeof(points_buf_t),
                    (void*)(long)offsetof(points_buf_t, color)));

    GL(glEnableVertexAttribArray(prog->a_shift_l));
    GL(glVertexAttribPointer(prog->a_shift_l, 2, GL_FLOAT, false,
                    sizeof(points_buf_t),
                    (void*)(long)offsetof(points_buf_t, shift)));

    GL(glDrawElements(GL_TRIANGLES, item->nb, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    GL(glDisableVertexAttribArray(prog->a_color_l));
    GL(glDisableVertexAttribArray(prog->a_tex_pos_l));
    GL(glDisableVertexAttribArray(prog->a_shift_l));

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void item_lines_render(renderer_gl_t *rend, const item_t *item)
{
    prog_t *prog;
    GLuint  array_buffer;
    GLuint  index_buffer;

    prog = &rend->progs.blit_proj;
    GL(glUseProgram(prog->prog));

    GL(glEnable(GL_CULL_FACE));
    GL(glLineWidth(item->lines.width));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ZERO, GL_ONE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    item->nb * sizeof(*item->indices),
                    item->indices, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf_size, item->buf,
                    GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));

    GL(glEnableVertexAttribArray(prog->a_pos_l));
    GL(glVertexAttribPointer(prog->a_pos_l, 4, GL_FLOAT, false,
                    sizeof(lines_buf_t),
                    (void*)(long)offsetof(lines_buf_t, pos)));

    GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, true,
                    sizeof(lines_buf_t),
                    (void*)(long)offsetof(lines_buf_t, tex_pos)));

    GL(glEnableVertexAttribArray(prog->a_color_l));
    GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                    sizeof(lines_buf_t),
                    (void*)(long)offsetof(lines_buf_t, color)));

    GL(glDrawElements(GL_LINES, item->nb, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    GL(glDisableVertexAttribArray(prog->a_color_l));
    GL(glDisableVertexAttribArray(prog->a_tex_pos_l));

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void item_vg_render(renderer_gl_t *rend, const item_t *item)
{
    double a, da;
    nvgBeginFrame(rend->vg, rend->fb_size[0] / rend->scale,
                            rend->fb_size[1] / rend->scale, rend->scale);
    nvgSave(rend->vg);
    nvgTranslate(rend->vg, item->vg.pos[0], item->vg.pos[1]);
    nvgRotate(rend->vg, item->vg.angle);
    nvgBeginPath(rend->vg);

    if (item->type == ITEM_VG_ELLIPSE && !item->vg.dashes)
        nvgEllipse(rend->vg, 0, 0, item->vg.size[0], item->vg.size[1]);

    if (item->type == ITEM_VG_ELLIPSE && item->vg.dashes) {
        da = 2 * M_PI / item->vg.dashes;
        for (a = 0; a < 2 * M_PI; a += da) {
            nvgMoveTo(rend->vg, item->vg.size[0] * cos(a),
                                item->vg.size[1] * sin(a));
            nvgLineTo(rend->vg, item->vg.size[0] * cos(a + da / 2),
                                item->vg.size[1] * sin(a + da / 2));
        }
    }

    if (item->type == ITEM_VG_RECT)
        nvgRect(rend->vg, -item->vg.size[0], -item->vg.size[1],
                2 * item->vg.size[0], 2 * item->vg.size[1]);

    if (item->type == ITEM_VG_LINE) {
        nvgMoveTo(rend->vg, 0, 0);
        nvgLineTo(rend->vg, item->vg.pos2[0] - item->vg.pos[0],
                            item->vg.pos2[1] - item->vg.pos[1]);
    }

    nvgStrokeColor(rend->vg, nvgRGBA(item->color[0] * 255,
                                     item->color[1] * 255,
                                     item->color[2] * 255,
                                     item->color[3] * 255));
    nvgStrokeWidth(rend->vg, 1);
    nvgStroke(rend->vg);
    nvgRestore(rend->vg);
    nvgEndFrame(rend->vg);
}

static void item_alpha_texture_render(renderer_gl_t *rend, const item_t *item)
{
    prog_t *prog;
    GLuint  array_buffer;
    GLuint  index_buffer;

    prog = &rend->progs.blit_tag;
    GL(glUseProgram(prog->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));
    GL(glEnable(GL_CULL_FACE));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ZERO, GL_ONE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    item->nb * sizeof(*item->indices),
                    item->indices, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf_size, item->buf,
                    GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));

    GL(glEnableVertexAttribArray(prog->a_pos_l));
    GL(glVertexAttribPointer(prog->a_pos_l, 2, GL_FLOAT, false,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, pos)));

    GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, false,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, tex_pos)));

    GL(glEnableVertexAttribArray(prog->a_color_l));
    GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, color)));

    GL(glDrawElements(GL_TRIANGLES, item->nb, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    GL(glDisableVertexAttribArray(prog->a_color_l));
    GL(glDisableVertexAttribArray(prog->a_tex_pos_l));

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void item_texture_render(renderer_gl_t *rend, const item_t *item)
{
    prog_t *prog;
    GLuint  array_buffer;
    GLuint  index_buffer;

    prog = &rend->progs.blit_proj;
    GL(glUseProgram(prog->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));
    GL(glEnable(GL_CULL_FACE));

    if (item->tex->format == GL_RGB && item->color[3] == 1.0) {
        GL(glDisable(GL_BLEND));
    } else {
        GL(glEnable(GL_BLEND));
        GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                               GL_ZERO, GL_ONE));
    }
    GL(glDisable(GL_DEPTH_TEST));

    if (item->flags & PAINTER_ADD) {
        GL(glEnable(GL_BLEND));
        if (color_is_white(item->color))
            GL(glBlendFunc(GL_ONE, GL_ONE));
        else {
            GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE));
            GL(glBlendColor(item->color[0] * item->color[3],
                            item->color[1] * item->color[3],
                            item->color[2] * item->color[3],
                            item->color[3]));
        }
    }

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    item->nb * sizeof(*item->indices),
                    item->indices, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf_size, item->buf,
                    GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));

    GL(glEnableVertexAttribArray(prog->a_pos_l));
    GL(glVertexAttribPointer(prog->a_pos_l, 2, GL_FLOAT, false,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, pos)));

    GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, false,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, tex_pos)));

    GL(glEnableVertexAttribArray(prog->a_color_l));
    GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                    sizeof(texture_buf_t),
                    (void*)(long)offsetof(texture_buf_t, color)));

    GL(glDrawElements(GL_TRIANGLES, item->nb, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    GL(glDisableVertexAttribArray(prog->a_color_l));
    GL(glDisableVertexAttribArray(prog->a_tex_pos_l));

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void item_planet_render(renderer_gl_t *rend, const item_t *item)
{
    prog_t *prog;
    GLuint  array_buffer;
    GLuint  index_buffer;
    float mf[16];

    prog = &rend->progs.planet;
    GL(glUseProgram(prog->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));

    GL(glActiveTexture(GL_TEXTURE1));
    if (item->planet.normalmap) {
        GL(glBindTexture(GL_TEXTURE_2D, item->planet.normalmap->id));
        GL(glUniform1i(prog->u_has_normal_tex_l, 1));
    } else {
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));
        GL(glUniform1i(prog->u_has_normal_tex_l, 0));
    }

    GL(glActiveTexture(GL_TEXTURE2));
    if (item->planet.shadow_color_tex &&
            texture_load(item->planet.shadow_color_tex, NULL))
        GL(glBindTexture(GL_TEXTURE_2D, item->planet.shadow_color_tex->id));
    else
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));

    if (item->flags & PAINTER_RING_SHADER)
        GL(glDisable(GL_CULL_FACE));
    else
        GL(glEnable(GL_CULL_FACE));

    if (item->tex->format == GL_RGB && item->color[3] == 1.0) {
        GL(glDisable(GL_BLEND));
    } else {
        GL(glEnable(GL_BLEND));
        GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                               GL_ZERO, GL_ONE));
    }
    if (item->depth_range[0] || item->depth_range[1])
        GL(glEnable(GL_DEPTH_TEST));
    else
        GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    item->nb * sizeof(*item->indices),
                    item->indices, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf_size, item->buf,
                    GL_DYNAMIC_DRAW));

    // Set all uniforms.
    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));
    GL(glUniform1f(prog->u_contrast_l, item->planet.contrast));
    GL(glUniform4f(prog->u_sun_l, item->planet.sun[0],
                                  item->planet.sun[1],
                                  item->planet.sun[2],
                                  item->planet.sun[3]));
    GL(glUniform3f(prog->u_light_emit_l,
                   item->planet.light_emit[0],
                   item->planet.light_emit[1],
                   item->planet.light_emit[2]));
    GL(glUniform1i(prog->u_material_l, item->planet.material));
    GL(glUniform1i(prog->u_is_moon_l, item->flags & PAINTER_IS_MOON ? 1 : 0));
    mat4_to_float(item->planet.mv, mf);
    GL(glUniformMatrix4fv(prog->u_mv_l, 1, 0, mf));
    GL(glUniform1i(prog->u_shadow_spheres_nb_l,
                   item->planet.shadow_spheres_nb));
    mat4_to_float(item->planet.shadow_spheres, mf);
    GL(glUniformMatrix4fv(prog->u_shadow_spheres_l, 1, 0, mf));

    GL(glUniform2f(prog->u_depth_range_l,
                   rend->depth_range[0], rend->depth_range[1]));

    // Set array positions.
    GL(glEnableVertexAttribArray(prog->a_pos_l));
    GL(glVertexAttribPointer(prog->a_pos_l, 4, GL_FLOAT, false,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, pos)));

    GL(glEnableVertexAttribArray(prog->a_mpos_l));
    GL(glVertexAttribPointer(prog->a_mpos_l, 4, GL_FLOAT, false,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, mpos)));

    GL(glEnableVertexAttribArray(prog->a_tex_pos_l));
    GL(glVertexAttribPointer(prog->a_tex_pos_l, 2, GL_FLOAT, false,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, tex_pos)));

    GL(glEnableVertexAttribArray(prog->a_color_l));
    GL(glVertexAttribPointer(prog->a_color_l, 4, GL_UNSIGNED_BYTE, true,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, color)));

    GL(glEnableVertexAttribArray(prog->a_normal_l));
    GL(glVertexAttribPointer(prog->a_normal_l, 3, GL_FLOAT, false,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, normal)));

    GL(glEnableVertexAttribArray(prog->a_tangent_l));
    GL(glVertexAttribPointer(prog->a_tangent_l, 3, GL_FLOAT, false,
                    sizeof(planet_buf_t),
                    (void*)(long)offsetof(planet_buf_t, tangent)));

    GL(glDrawElements(GL_TRIANGLES, item->nb, GL_UNSIGNED_SHORT, 0));

    GL(glDisableVertexAttribArray(prog->a_pos_l));
    GL(glDisableVertexAttribArray(prog->a_mpos_l));
    GL(glDisableVertexAttribArray(prog->a_color_l));
    GL(glDisableVertexAttribArray(prog->a_tex_pos_l));
    GL(glDisableVertexAttribArray(prog->a_normal_l));
    GL(glDisableVertexAttribArray(prog->a_tangent_l));

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void rend_flush(renderer_gl_t *rend)
{
    item_t *item, *tmp;

    // Compute depth range.
    rend->depth_range[0] = DBL_MAX;
    rend->depth_range[1] = DBL_MIN;
    DL_FOREACH(rend->items, item) {
        if (item->depth_range[0] || item->depth_range[1]) {
            rend->depth_range[0] = min(rend->depth_range[0],
                                       item->depth_range[0]);
            rend->depth_range[1] = max(rend->depth_range[1],
                                       item->depth_range[1]);
        }
    }
    if (rend->depth_range[0] == DBL_MAX) {
        rend->depth_range[0] = 0;
        rend->depth_range[1] = 1;
    }

    DL_FOREACH_SAFE(rend->items, item, tmp) {
        if (item->type == ITEM_LINES) item_lines_render(rend, item);
        if (item->type == ITEM_POINTS) item_points_render(rend, item);
        if (item->type == ITEM_ALPHA_TEXTURE)
            item_alpha_texture_render(rend, item);
        if (item->type == ITEM_TEXTURE)
            item_texture_render(rend, item);
        if (item->type == ITEM_PLANET) item_planet_render(rend, item);
        if (item->type == ITEM_VG_ELLIPSE) item_vg_render(rend, item);
        if (item->type == ITEM_VG_RECT) item_vg_render(rend, item);
        if (item->type == ITEM_VG_LINE) item_vg_render(rend, item);
        DL_DELETE(rend->items, item);
        texture_release(item->tex);
        free(item->indices);
        free(item->buf);
        free(item);
    }
}

static item_t *get_item(renderer_gl_t *rend, int type, int nb, texture_t *tex)
{
    item_t *item;
    item = rend->items ? rend->items->prev : NULL;
    if (    item && item->type == type && item->capacity > item->nb + nb &&
            item->tex == tex)
        return item;
    return NULL;
}

static void line(renderer_t           *rend_,
                 const painter_t      *painter,
                 int                  frame,
                 double               line[2][4],
                 int                  nb_segs,
                 const projection_t   *line_proj)
{
    int i, ofs;
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    double k, pos[4];
    lines_buf_t *buf;
    uint16_t *indices;

    item = get_item(rend, ITEM_LINES, nb_segs * 2, NULL);
    if (item && !vec4_equal(item->color, painter->color)) item = NULL;
    if (item && item->lines.width != painter->lines_width) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_LINES;
        item->capacity = 1024;
        item->buf = calloc(item->capacity, sizeof(*buf));
        item->indices = calloc(item->capacity, sizeof(*indices));
        item->lines.width = painter->lines_width;
        vec4_copy(painter->color, item->color);
        DL_APPEND(rend->items, item);
    }

    buf = item->buf + item->buf_size;
    indices = item->indices + item->nb;
    ofs = item->buf_size / sizeof(*buf);

    for (i = 0; i < nb_segs + 1; i++) {
        k = i / (double)nb_segs;
        buf[i].tex_pos[0] = k;
        vec4_mix(line[0], line[1], k, pos);
        if (line_proj)
            project(line_proj, PROJ_BACKWARD, 4, pos, pos);
        mat4_mul_vec4(*painter->transform, pos, pos);
        convert_directionv4(painter->obs, frame, FRAME_VIEW, pos, pos);
        pos[3] = 1.0;
        project(painter->proj, 0, 4, pos, pos);
        vec4_to_float(pos, buf[i].pos);
        memcpy(buf[i].color, (uint8_t[]){255, 255, 255, 255}, 4);
        if (i < nb_segs) {
            indices[i * 2 + 0] = ofs + i;
            indices[i * 2 + 1] = ofs + i + 1;
        }
    }
    item->buf_size += (nb_segs + 1) * sizeof(lines_buf_t);
    item->nb += nb_segs * 2;
}

void ellipse_2d(renderer_t        *rend_,
                const painter_t   *painter,
                const double pos[2], const double size[2], double angle)
{
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_ELLIPSE;
    vec2_copy(pos, item->vg.pos);
    vec2_copy(size, item->vg.size);
    vec4_copy(painter->color, item->color);
    item->vg.angle = angle;
    item->vg.dashes = painter->lines_stripes;
    DL_APPEND(rend->items, item);
}

void rect_2d(renderer_t        *rend_,
             const painter_t   *painter,
             const double pos[2], const double size[2], double angle)
{
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_RECT;
    vec2_copy(pos, item->vg.pos);
    vec2_copy(size, item->vg.size);
    vec4_copy(painter->color, item->color);
    item->vg.angle = angle;
    DL_APPEND(rend->items, item);
}

void line_2d(renderer_t          *rend_,
             const painter_t     *painter,
             const double        p1[2],
             const double        p2[2])
{
    renderer_gl_t *rend = (void*)rend_;
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_LINE;
    vec2_copy(p1, item->vg.pos);
    vec2_copy(p2, item->vg.pos2);
    vec4_copy(painter->color, item->color);
    DL_APPEND(rend->items, item);
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
    UNIFORM(u_depth_range);
    ATTRIB(a_pos);
    ATTRIB(a_mpos);
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
    rend->vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

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
    rend->rend.finish = finish;
    rend->rend.flush = flush;
    rend->rend.points = points;
    rend->rend.quad = quad;
    rend->rend.texture = texture;
    rend->rend.text = text;
    rend->rend.line = line;
    rend->rend.ellipse_2d = ellipse_2d;
    rend->rend.rect_2d = rect_2d;
    rend->rend.line_2d = line_2d;

    return &rend->rend;
}
