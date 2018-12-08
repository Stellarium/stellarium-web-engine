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

// All the shader attribute locations.
enum {
    ATTR_POS,
    ATTR_MPOS,
    ATTR_TEX_POS,
    ATTR_NORMAL,
    ATTR_TANGENT,
    ATTR_COLOR,
    ATTR_SHIFT,
};

static const char *ATTR_NAMES[] = {
    [ATTR_POS]          = "a_pos",
    [ATTR_MPOS]         = "a_mpos",
    [ATTR_TEX_POS]      = "a_tex_pos",
    [ATTR_NORMAL]       = "a_normal",
    [ATTR_TANGENT]      = "a_tangent",
    [ATTR_COLOR]        = "a_color",
    [ATTR_SHIFT]        = "a_shift",
    NULL,
};

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

/*
 * Struct: prog_t
 * Contains an opengl shader and all it's uniform locations.
 *
 * All the location for all possible shaders are stored, so we can use this
 * struct for any of our shaders.
 */
typedef struct prog {
    GLuint prog;

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
    gl_buf_t    buf;
    gl_buf_t    indices;
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

static const gl_buf_info_t INDICES_BUF = {
    .size = 2,
    .attrs = {
        {GL_UNSIGNED_SHORT, 1, false, 0},
    },
};

static const gl_buf_info_t LINES_BUF = {
    .size = 28,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 4, false, 0},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 16},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 24},
    },
};

static const gl_buf_info_t POINTS_BUF = {
    .size = 36,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 4, false, 0},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 16},
        [ATTR_SHIFT]    = {GL_FLOAT, 2, false, 24},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 32},
    },
};

static const gl_buf_info_t TEXTURE_BUF = {
    .size = 20,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 2, false, 0},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 8},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 16},
    },
};

static const gl_buf_info_t PLANET_BUF = {
    .size = 68,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 4, false, 0},
        [ATTR_MPOS]     = {GL_FLOAT, 4, false, 16},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 32},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 40},
        [ATTR_NORMAL]   = {GL_FLOAT, 3, false, 44},
        [ATTR_TANGENT]  = {GL_FLOAT, 3, false, 56},
    },
};

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

    double  depth_range[2];

    texture_t   *white_tex;
    tex_cache_t *tex_cache;
    NVGcontext *vg;

    item_t  *items;
} renderer_gl_t;


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
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };
    // Scale size from window to NDC.
    double s[2] = {2 * rend->scale / rend->fb_size[0],
                   2 * rend->scale / rend->fb_size[1]};
    int i, j, ofs;
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
        gl_buf_alloc(&item->buf, &POINTS_BUF, MAX_POINTS * 4);
        gl_buf_alloc(&item->indices, &INDICES_BUF, MAX_POINTS * 6);
        vec4_copy(painter->color, item->color);
        item->points.smooth = painter->points_smoothness;
        DL_APPEND(rend->items, item);
    }

    ofs = item->buf.nb;

    for (i = 0; i < n; i++) {
        p = points[i];
        if (frame == FRAME_WINDOW) {
            window_to_ndc(rend, p.pos, p.pos);
        } else if (frame != FRAME_NDC) {
            convert_framev4(painter->obs, frame, FRAME_VIEW, p.pos, p.pos);
            project(painter->proj, PROJ_TO_NDC_SPACE, 3, p.pos, p.pos);
        }
        for (j = 0; j < 4; j++) {
            gl_buf_4f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(p.pos), 1);
            gl_buf_2f(&item->buf, -1, ATTR_SHIFT,
                      (j % 2 - 0.5) * p.size * s[0] * sm,
                      (j / 2 - 0.5) * p.size * s[1] * sm);
            gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, j % 2, j / 2);
            gl_buf_4i(&item->buf, -1, ATTR_COLOR,
                      p.color[0] * 255,
                      p.color[1] * 255,
                      p.color[2] * 255,
                      p.color[3] * 255);
            gl_buf_next(&item->buf);
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
            gl_buf_1i(&item->indices, -1, 0, ofs + INDICES[j] + i * 4);
            gl_buf_next(&item->indices);
        }
    }
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
    int n, i, j, k;
    double duvx[2], duvy[2];
    double p[4], normal[4] = {0}, tangent[4] = {0}, z;

    // Positions of the triangles in the quads.
    const int INDICES[6][2] = { {0, 0}, {0, 1}, {1, 0},
                                {1, 1}, {1, 0}, {0, 1} };

    tex = tex ?: rend->white_tex;
    n = grid_size + 1;

    item = calloc(1, sizeof(*item));
    item->type = ITEM_PLANET;
    gl_buf_alloc(&item->buf, &PLANET_BUF, n * n * 4);
    gl_buf_alloc(&item->indices, &INDICES_BUF, n * n * 6);
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

    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec4_set(p, uv[0][0], uv[0][1], 0, 1);
        vec2_addk(p, duvx, (double)j / grid_size, p);
        vec2_addk(p, duvy, (double)i / grid_size, p);
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, p[0] * tex->w / tex->tex_w,
                                                 p[1] * tex->h / tex->tex_h);

        if (normalmap) {
            compute_tangent(p, tex_proj, tangent);
            mat4_mul_vec4(*painter->transform, tangent, tangent);
            gl_buf_3f(&item->buf, -1, ATTR_TANGENT, VEC3_SPLIT(tangent));
        }

        // XXX: move up.
        if (tex->flags & TF_FLIPPED) {
            gl_buf_2f(&item->buf, -1, ATTR_TEX_POS,
                      p[0] * tex->w / tex->tex_w,
                      1.0 - p[1] * tex->h / tex->tex_h);
        }
        project(tex_proj, PROJ_BACKWARD, 4, p, p);

        vec3_copy(p, normal);
        mat4_mul_vec4(*painter->transform, normal, normal);
        gl_buf_3f(&item->buf, -1, ATTR_NORMAL, VEC3_SPLIT(normal));

        mat4_mul_vec4(*painter->transform, p, p);
        gl_buf_4f(&item->buf, -1, ATTR_MPOS, VEC4_SPLIT(p));
        convert_framev4(painter->obs, frame, FRAME_VIEW, p, p);
        z = p[2];
        project(painter->proj, 0, 4, p, p);
        if (painter->depth_range) {
            vec2_copy(*painter->depth_range, item->depth_range);
            p[2] = -z;
        }
        gl_buf_4f(&item->buf, -1, ATTR_POS, VEC4_SPLIT(p));
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, 255, 255, 255, 255);
        gl_buf_next(&item->buf);
    }

    for (i = 0; i < grid_size; i++)
    for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            gl_buf_1i(&item->indices, -1, 0,
                      (INDICES[k][1] + i) * n + (INDICES[k][0] + j));
            gl_buf_next(&item->indices);
        }
    }
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
    gl_buf_alloc(&item->buf, &TEXTURE_BUF, n * n * 4);
    gl_buf_alloc(&item->indices, &INDICES_BUF, n * n * 6);
    item->tex = tex;
    item->tex->ref++;
    vec4_copy(painter->color, item->color);
    item->flags = painter->flags;

    vec2_sub(uv[1], uv[0], duvx);
    vec2_sub(uv[2], uv[0], duvy);

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
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, tex_pos[0], tex_pos[1]);

        project(tex_proj, PROJ_BACKWARD, 4, p, p);
        mat4_mul_vec4(*painter->transform, p, p);
        convert_framev4(painter->obs, frame, FRAME_VIEW, p, p);
        project(painter->proj, 0, 4, p, p);
        gl_buf_2f(&item->buf, -1, ATTR_POS, p[0], p[1]);
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, 255, 255, 255, 255);
        gl_buf_next(&item->buf);
    }

    for (i = 0; i < grid_size; i++)
    for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            gl_buf_1i(&item->indices, -1, 0,
                      (INDICES[k][1] + i) * n + (INDICES[k][0] + j));
            gl_buf_next(&item->indices);
        }
    }
    DL_APPEND(rend->items, item);
}

static void texture2(renderer_gl_t *rend, texture_t *tex,
                     double uv[4][2], double pos[4][2],
                     const double color[4])
{
    int i, ofs;
    item_t *item;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };

    item = get_item(rend, ITEM_ALPHA_TEXTURE, 6, tex);
    if (item && !vec4_equal(item->color, color)) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_ALPHA_TEXTURE;
        gl_buf_alloc(&item->buf, &TEXTURE_BUF, 64 * 4);
        gl_buf_alloc(&item->indices, &INDICES_BUF, 64 * 6);
        item->tex = tex;
        item->tex->ref++;
        vec4_copy(color, item->color);
        DL_APPEND(rend->items, item);
    }

    ofs = item->buf.nb;

    for (i = 0; i < 4; i++) {
        gl_buf_2f(&item->buf, -1, ATTR_POS, pos[i][0], pos[i][1]);
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, uv[i][0], uv[i][1]);
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, 255, 255, 255, 255);
        gl_buf_next(&item->buf);
    }
    for (i = 0; i < 6; i++) {
        gl_buf_1i(&item->indices, -1, 0, ofs + INDICES[i]);
        gl_buf_next(&item->indices);
    }
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
                    item->indices.nb * item->indices.info->size,
                    item->indices.data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, item->color[0],
                                    item->color[1],
                                    item->color[2],
                                    item->color[3]));
    GL(glUniform1f(prog->u_smooth_l, item->points.smooth));

    gl_buf_enable(&item->buf);
    GL(glDrawElements(GL_TRIANGLES, item->indices.nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(&item->buf);

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
                    item->indices.nb * item->indices.info->size,
                    item->indices.data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));

    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, VEC4_SPLIT(item->color)));

    gl_buf_enable(&item->buf);
    GL(glDrawElements(GL_LINES, item->indices.nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(&item->buf);

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
                    item->indices.nb * item->indices.info->size,
                    item->indices.data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, VEC4_SPLIT(item->color)));

    gl_buf_enable(&item->buf);
    GL(glDrawElements(GL_TRIANGLES, item->indices.nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(&item->buf);

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
                    item->indices.nb * item->indices.info->size,
                    item->indices.data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    GL(glUniform4f(prog->u_color_l, VEC4_SPLIT(item->color)));

    gl_buf_enable(&item->buf);
    GL(glDrawElements(GL_TRIANGLES, item->indices.nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(&item->buf);

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
                    item->indices.nb * item->indices.info->size,
                    item->indices.data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    // Set all uniforms.
    GL(glUniform4f(prog->u_color_l, VEC4_SPLIT(item->color)));
    GL(glUniform1f(prog->u_contrast_l, item->planet.contrast));
    GL(glUniform4f(prog->u_sun_l, VEC4_SPLIT(item->planet.sun)));
    GL(glUniform3f(prog->u_light_emit_l, VEC3_SPLIT(item->planet.light_emit)));
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

    gl_buf_enable(&item->buf);
    GL(glDrawElements(GL_TRIANGLES, item->indices.nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(&item->buf);

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
        gl_buf_release(&item->buf);
        gl_buf_release(&item->indices);
        free(item);
    }
}

static item_t *get_item(renderer_gl_t *rend, int type, int nb, texture_t *tex)
{
    item_t *item;
    item = rend->items ? rend->items->prev : NULL;
    if (    item && item->type == type &&
            item->indices.capacity > item->indices.nb + nb &&
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

    item = get_item(rend, ITEM_LINES, nb_segs * 2, NULL);
    if (item && !vec4_equal(item->color, painter->color)) item = NULL;
    if (item && item->lines.width != painter->lines_width) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_LINES;
        gl_buf_alloc(&item->buf, &LINES_BUF, 1024);
        gl_buf_alloc(&item->indices, &INDICES_BUF, 1024);
        item->lines.width = painter->lines_width;
        vec4_copy(painter->color, item->color);
        DL_APPEND(rend->items, item);
    }

    ofs = item->buf.nb;

    for (i = 0; i < nb_segs + 1; i++) {
        k = i / (double)nb_segs;
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, k, 0);
        vec4_mix(line[0], line[1], k, pos);
        if (line_proj)
            project(line_proj, PROJ_BACKWARD, 4, pos, pos);
        mat4_mul_vec4(*painter->transform, pos, pos);
        vec3_normalize(pos, pos);
        convert_frame(painter->obs, frame, FRAME_VIEW, true, pos, pos);
        pos[3] = 0.0;
        project(painter->proj, PROJ_ALREADY_NORMALIZED, 4, pos, pos);
        gl_buf_4f(&item->buf, -1, ATTR_POS, VEC4_SPLIT(pos));
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, 255, 255, 255, 255);
        if (i < nb_segs) {
            gl_buf_1i(&item->indices, -1, 0, ofs + i);
            gl_buf_next(&item->indices);
            gl_buf_1i(&item->indices, -1, 0, ofs + i + 1);
            gl_buf_next(&item->indices);
        }
        gl_buf_next(&item->buf);
    }
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

static void init_prog(prog_t *p, const char *code, const char *include)
{
    assert(code);
    p->prog = gl_create_program(code, code, include, ATTR_NAMES);
    GL(glUseProgram(p->prog));
#define UNIFORM(x) p->x##_l = glGetUniformLocation(p->prog, #x);
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
              asset_get_data("asset://shaders/points.glsl", NULL, NULL), NULL);
    init_prog(&rend->progs.blit_proj,
              asset_get_data("asset://shaders/blit.glsl", NULL, NULL), NULL);
    init_prog(&rend->progs.blit_tag,
              asset_get_data("asset://shaders/blit_tag.glsl", NULL, NULL),
                             NULL);
    init_prog(&rend->progs.planet,
              asset_get_data("asset://shaders/planet.glsl", NULL, NULL), NULL);

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
