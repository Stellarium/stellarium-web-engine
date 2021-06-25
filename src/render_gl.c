/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "render.h"
#include "swe.h"

#include "line_mesh.h"
#include "shader_cache.h"
#include "utils/gl.h"

#ifdef GLES2
#   define NANOVG_GLES2_IMPLEMENTATION
#else
#   define NANOVG_GL2_IMPLEMENTATION
#endif
#include "nanovg.h"
#include "nanovg_gl.h"

#include <float.h>

#define GRID_CACHE_SIZE (2 * (1 << 20))

// Fix GL_PROGRAM_POINT_SIZE support on Mac.
#ifdef __APPLE__
#   define GL_PROGRAM_POINT_SIZE GL_PROGRAM_POINT_SIZE_EXT
#endif

enum {
    FONT_REGULAR = 0,
    FONT_BOLD    = 1,
};

// All the shader attribute locations.
enum {
    ATTR_POS,
    ATTR_MPOS,
    ATTR_TEX_POS,
    ATTR_NORMAL,
    ATTR_TANGENT,
    ATTR_COLOR,
    ATTR_SKY_POS,
    ATTR_LUMINANCE,
    ATTR_SIZE,
    ATTR_WPOS,
};

static const char *ATTR_NAMES[] = {
    [ATTR_POS]          = "a_pos",
    [ATTR_MPOS]         = "a_mpos",
    [ATTR_TEX_POS]      = "a_tex_pos",
    [ATTR_NORMAL]       = "a_normal",
    [ATTR_TANGENT]      = "a_tangent",
    [ATTR_COLOR]        = "a_color",
    [ATTR_SKY_POS]      = "a_sky_pos",
    [ATTR_LUMINANCE]    = "a_luminance",
    [ATTR_SIZE]         = "a_size",
    [ATTR_WPOS]         = "a_wpos",
    NULL,
};

// We keep all the text textures in a cache so that we don't have to recreate
// them each time.
typedef struct tex_cache tex_cache_t;
struct tex_cache {
    tex_cache_t *next, *prev;
    double      size;
    char        *text;
    int         effects;
    bool        in_use;
    int         xoff;
    int         yoff;
    double      color[3];
    texture_t   *tex;
};

enum {
    ITEM_LINES = 1,
    ITEM_MESH,
    ITEM_POINTS,
    ITEM_POINTS_3D,
    ITEM_TEXTURE,
    ITEM_TEXTURE_2D,
    ITEM_ATMOSPHERE,
    ITEM_FOG,
    ITEM_PLANET,
    ITEM_VG_ELLIPSE,
    ITEM_VG_RECT,
    ITEM_VG_LINE,
    ITEM_TEXT,
    ITEM_GLTF,
};

typedef struct item item_t;
struct item
{
    int type;

    float       color[4];
    gl_buf_t    buf;
    gl_buf_t    indices;
    texture_t   *tex;
    int         flags;

    union {
        struct {
            float width;
            float glow;
            float dash_length;
            float dash_ratio;
            float fade_dist_min;
            float fade_dist_max;
        } lines;

        struct {
            float halo;
        } points;

        struct {
            float contrast;
            texture_t *normalmap;
            texture_t *shadow_color_tex;
            float mv[16];
            float sun[4]; // pos + radius.
            float light_emit[3];
            int   shadow_spheres_nb;
            float shadow_spheres[4][4]; // (pos + radius) * 4
            int   material;
            float tex_transf[9];
            float normal_tex_transf[9];
            float min_brightness;
        } planet;

        struct {
            float pos[2];
            float pos2[2];
            float size[2];
            float angle;
            float dashes;
            float stroke_width;
        } vg;

        struct {
            float p[12];    // Color computation coefs.
            float sun[3];   // Sun position.
        } atm;

        struct {
            char text[128];
            float pos[2];
            float size;
            float angle;
            int   align;
            int   effects;
        } text;

        struct {
            int mode;
            float stroke_width;
            // Projection setttings.  Should be set globally probably.
            int proj;
            float proj_scaling[2];
            bool use_stencil;
        } mesh;

        struct {
            const char *model;
            double model_mat[4][4];
            double view_mat[4][4];
            double proj_mat[4][4];
            double light_dir[3];
            json_value *args;
        } gltf;
    };

    item_t *next, *prev;
};

static const gl_buf_info_t INDICES_BUF = {
    .size = 2,
    .attrs = {
        {GL_UNSIGNED_SHORT, 1, false, 0},
    },
};

static const gl_buf_info_t MESH_BUF = {
    .size = 16,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 12},
    },
};

static const gl_buf_info_t LINES_BUF = {
    .size = 28,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_WPOS]     = {GL_FLOAT, 2, false, 12},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 20},
    },
};

static const gl_buf_info_t POINTS_BUF = {
    .size = 16,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 2, false, 0},
        [ATTR_SIZE]     = {GL_FLOAT, 1, false, 8},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 12},
    },
};

static const gl_buf_info_t POINTS_3D_BUF = {
    .size = 20,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_SIZE]     = {GL_FLOAT, 1, false, 12},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 16},
    },
};

static const gl_buf_info_t TEXTURE_BUF = {
    .size = 20,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 12},
    },
};

static const gl_buf_info_t TEXTURE_2D_BUF = {
    .size = 28,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_WPOS]     = {GL_FLOAT, 2, false, 12},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 20},
    },
};

static const gl_buf_info_t PLANET_BUF = {
    .size = 60,
    .attrs = {
        [ATTR_POS]      = {GL_FLOAT, 3, false, 0},
        [ATTR_MPOS]     = {GL_FLOAT, 3, false, 12},
        [ATTR_TEX_POS]  = {GL_FLOAT, 2, false, 24},
        [ATTR_COLOR]    = {GL_UNSIGNED_BYTE, 4, true, 32},
        [ATTR_NORMAL]   = {GL_FLOAT, 3, false, 36},
        [ATTR_TANGENT]  = {GL_FLOAT, 3, false, 48},
    },
};

static const gl_buf_info_t ATMOSPHERE_BUF = {
    .size = 28,
    .attrs = {
        [ATTR_POS]       = {GL_FLOAT, 3, false, 0},
        [ATTR_SKY_POS]   = {GL_FLOAT, 3, false, 12},
        [ATTR_LUMINANCE] = {GL_FLOAT, 1, false, 24},
    },
};

static const gl_buf_info_t FOG_BUF = {
    .size = 24,
    .attrs = {
        [ATTR_POS]       = {GL_FLOAT, 3, false, 0},
        [ATTR_SKY_POS]   = {GL_FLOAT, 3, false, 12},
    },
};

struct renderer {

    projection_t proj;
    int     fb_size[2];
    double  scale;
    bool    cull_flipped;

    double  depth_min;
    double  depth_max;

    texture_t   *white_tex;
    tex_cache_t *tex_cache;
    NVGcontext *vg;

    // Nanovg fonts references for regular and bold.
    struct {
        int   id;
        float scale;
        bool  is_default_font; // Set only for the original default fonts.
    } fonts[2];

    item_t  *items;
    cache_t *grid_cache;

};

// Weak linking, so that we can put the implementation in a module.
__attribute__((weak))
int gltf_render(const char *url,
                const double model_mat[4][4],
                const double view_mat[4][4],
                const double proj_mat[4][4],
                const double light_dir[3],
                json_value *args)
{
    return 0;
}

static void init_shader(gl_shader_t *shader)
{
    // Set some common uniforms.
    GL(glUseProgram(shader->prog));
    gl_update_uniform(shader, "u_tex", 0);
    gl_update_uniform(shader, "u_normal_tex", 1);
    gl_update_uniform(shader, "u_shadow_color_tex", 2);
}

static bool color_is_white(const float c[4])
{
    return c[0] == 1.0f && c[1] == 1.0f && c[2] == 1.0f && c[3] == 1.0f;
}

static void proj_set_depth_range(projection_t *proj,
                                 double nearval, double farval)
{
    proj->mat[2][2] = (farval + nearval) / (nearval - farval);
    proj->mat[3][2] = 2. * farval * nearval / (nearval - farval);
}

static double proj_get_depth(const projection_t *proj,
                             const double p[3])
{
    if (proj->klass->id == PROJ_PERSPECTIVE) {
        return -p[2];
    } else {
        return vec3_norm(p);
    }
}

/*
 * Return the current flush projection, with depth range sets to infinity
 * if we did not enable the depth
 */
static projection_t rend_get_proj(const renderer_t *rend, int flags)
{
    const double eps = 0.000001;
    const double nearval = 5 * DM2AU;
    projection_t proj = rend->proj;
    if (!(flags & PAINTER_ENABLE_DEPTH)) {
        // Infinite zfar projection matrix.
        // from 'Projection Matrix Tricks', by Eric Lengyel.
        proj.mat[2][2] = eps - 1;
        proj.mat[3][2] = (eps - 2) * nearval;
    }
    return proj;
}

static void window_to_ndc(renderer_t *rend,
                          const double win[2], double ndc[2])
{
    ndc[0] = (win[0] * rend->scale / rend->fb_size[0]) * 2 - 1;
    ndc[1] = 1 - (win[1] * rend->scale / rend->fb_size[1]) * 2;
}

void render_prepare(renderer_t *rend, const projection_t *proj,
                    double win_w, double win_h,
                    double scale, bool cull_flipped)
{
    tex_cache_t *ctex;

    rend->fb_size[0] = win_w * scale;
    rend->fb_size[1] = win_h * scale;
    rend->scale = scale;
    rend->cull_flipped = cull_flipped;
    rend->proj = *proj;

    DL_FOREACH(rend->tex_cache, ctex)
        ctex->in_use = false;

    rend->depth_min = DBL_MAX;
    rend->depth_max = DBL_MIN;
}

/*
 * Function: get_item
 * Try to get a render item we can batch with.
 *
 * Parameters:
 *   type           - The type of item.
 *   buf_size       - The free vertex buffer size requiered.
 *   indices_size   - The free indice size required.
 */
static item_t *get_item(renderer_t *rend, int type,
                        int buf_size,
                        int indices_size,
                        texture_t *tex)
{
    item_t *item;
    item = rend->items ? rend->items->prev : NULL;

    while (item) {
        if (item->type == type &&
            item->buf.capacity > item->buf.nb + buf_size &&
            (indices_size == 0 ||
                item->indices.capacity > item->indices.nb + indices_size) &&
            item->tex == tex)
        {
            return item;
        }
        // Keep searching only if we allow reordering.
        if (!(item->flags & PAINTER_ALLOW_REORDER)) break;
        item = item->prev;
    }
    return NULL;
}

void render_points_2d(renderer_t *rend, const painter_t *painter,
                      int n, const point_t *points)
{
    item_t *item;
    int i;
    const int MAX_POINTS = 4096;
    point_t p;

    if (n > MAX_POINTS) {
        LOG_E("Try to render more than %d points: %d", MAX_POINTS, n);
        n = MAX_POINTS;
    }

    item = get_item(rend, ITEM_POINTS, n, 0, NULL);
    if (item && item->points.halo != painter->points_halo)
        item = NULL;
    if (item && item->flags != painter->flags)
        item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_POINTS;
        item->flags = painter->flags;
        gl_buf_alloc(&item->buf, &POINTS_BUF, MAX_POINTS);
        vec4_to_float(painter->color, item->color);
        item->points.halo = painter->points_halo;
        DL_APPEND(rend->items, item);
    }

    for (i = 0; i < n; i++) {
        p = points[i];
        window_to_ndc(rend, p.pos, p.pos);

        gl_buf_2f(&item->buf, -1, ATTR_POS, VEC2_SPLIT(p.pos));
        gl_buf_1f(&item->buf, -1, ATTR_SIZE, p.size * rend->scale);
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, VEC4_SPLIT(p.color));
        gl_buf_next(&item->buf);

        // Add the point int the global list of rendered points.
        // XXX: could be done in the painter.
        if (p.obj) {
            p.pos[0] = (+p.pos[0] + 1) / 2 * core->win_size[0];
            p.pos[1] = (-p.pos[1] + 1) / 2 * core->win_size[1];
            areas_add_circle(core->areas, p.pos, p.size, p.obj);
        }
    }
}

void render_points_3d(renderer_t *rend, const painter_t *painter,
                      int n, const point_3d_t *points)
{
    item_t *item;
    int i;
    const int MAX_POINTS = 4096;
    double win_xy[2], depth;
    point_3d_t p;

    if (n > MAX_POINTS) {
        LOG_E("Try to render more than %d points: %d", MAX_POINTS, n);
        n = MAX_POINTS;
    }

    item = get_item(rend, ITEM_POINTS_3D, n, 0, NULL);
    if (item && item->points.halo != painter->points_halo)
        item = NULL;
    if (item && item->flags != painter->flags)
        item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_POINTS_3D;
        item->flags = painter->flags;
        gl_buf_alloc(&item->buf, &POINTS_3D_BUF, MAX_POINTS);
        vec4_to_float(painter->color, item->color);
        item->points.halo = painter->points_halo;
        DL_APPEND(rend->items, item);
    }

    for (i = 0; i < n; i++) {
        p = points[i];
        gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(p.pos));
        gl_buf_1f(&item->buf, -1, ATTR_SIZE, p.size * rend->scale);
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, VEC4_SPLIT(p.color));
        gl_buf_next(&item->buf);

        depth = proj_get_depth(painter->proj, p.pos);
        rend->depth_min = min(rend->depth_min, depth);
        rend->depth_max = max(rend->depth_max, depth);

        // Add the point int the global list of rendered points.
        // XXX: could be done in the painter.
        if (p.obj) {
            project_to_win_xy(painter->proj, p.pos, win_xy);
            areas_add_circle(core->areas, win_xy, p.size, p.obj);
        }
    }
}

/*
 * Function: get_grid
 * Compute an uv_map grid, and cache it if possible.
 */
static const double (*get_grid(renderer_t *rend,
                               const uv_map_t *map, int split,
                               bool *should_delete))[4]
{
    int n = split + 1;
    double (*grid)[4];
    struct {
        int order;
        int pix;
        int split;
        int swapped;
    } key = { map->order, map->pix, split, map->swapped };
    _Static_assert(sizeof(key) == 16, "");
    bool can_cache = map->type == UV_MAP_HEALPIX && map->at_infinity;

    *should_delete = !can_cache;
    if (can_cache) {
        if (!rend->grid_cache)
            rend->grid_cache = cache_create(GRID_CACHE_SIZE);
        grid = cache_get(rend->grid_cache, &key, sizeof(key));
        if (grid)
            return grid;
    }

    grid = malloc(n * n * sizeof(*grid));
    uv_map_grid(map, split, grid, NULL);

    if (can_cache) {
        cache_add(rend->grid_cache, &key, sizeof(key),
                  grid, sizeof(*grid) * n * n, NULL);
    }

    return grid;
}

static void compute_tangent(const double uv[2], const uv_map_t *map,
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
    unproject(tex_proj, uv1, p1);
    unproject(tex_proj, uv2, p2);
    vec3_sub(p2, p1, tangent);
    vec3_normalize(tangent, out);
    */

    double p[4] = {0}, n[3];
    uv_map(map, uv, p, n);
    vec3_cross(VEC(0, 0, 1), n, out);
}

static void quad_planet(
                 renderer_t          *rend,
                 const painter_t     *painter,
                 int                 frame,
                 int                 grid_size,
                 const uv_map_t      *map)
{
    item_t *item;
    int n, i, j, k;
    double p[4], mpos[4], normal[4] = {0}, tangent[4] = {0}, mv[4][4], depth;

    // Positions of the triangles in the quads.
    const int INDICES[6][2] = { {0, 0}, {0, 1}, {1, 0},
                                {1, 1}, {1, 0}, {0, 1} };
    n = grid_size + 1;

    assert(painter->flags & PAINTER_ENABLE_DEPTH);
    item = calloc(1, sizeof(*item));
    item->type = ITEM_PLANET;
    gl_buf_alloc(&item->buf, &PLANET_BUF, n * n * 4);
    gl_buf_alloc(&item->indices, &INDICES_BUF, n * n * 6);
    vec4_to_float(painter->color, item->color);
    item->flags = painter->flags;
    item->planet.shadow_color_tex = painter->planet.shadow_color_tex;
    item->planet.contrast = painter->contrast;
    item->planet.min_brightness = painter->planet.min_brightness;
    item->planet.shadow_spheres_nb = painter->planet.shadow_spheres_nb;
    for (i = 0; i < painter->planet.shadow_spheres_nb; i++) {
        vec4_to_float(painter->planet.shadow_spheres[i],
                      item->planet.shadow_spheres[i]);
    }
    vec4_to_float(*painter->planet.sun, item->planet.sun);
    if (painter->planet.light_emit)
        vec3_to_float(*painter->planet.light_emit, item->planet.light_emit);

    // Compute modelview matrix.
    mat4_set_identity(mv);
    if (frame == FRAME_OBSERVED)
        mat3_to_mat4(painter->obs->ro2v, mv);
    if (frame == FRAME_ICRF)
        mat3_to_mat4(painter->obs->ri2v, mv);
    mat4_to_float(mv, item->planet.mv);

    // Set material
    if (painter->planet.light_emit)
        item->planet.material = 1; // Generic
    else
        item->planet.material = 0; // Oren Nayar
    if (painter->flags & PAINTER_RING_SHADER)
        item->planet.material = 2; // Ring

    // Set textures.
    item->tex = painter->textures[PAINTER_TEX_COLOR].tex ?: rend->white_tex;
    mat3_to_float(painter->textures[PAINTER_TEX_COLOR].mat,
                  item->planet.tex_transf);
    item->tex->ref++;
    item->planet.normalmap = painter->textures[PAINTER_TEX_NORMAL].tex;
    mat3_to_float(painter->textures[PAINTER_TEX_NORMAL].mat,
                  item->planet.normal_tex_transf);
    if (item->planet.normalmap) item->planet.normalmap->ref++;

    // Only support POT textures for planets.
    assert(item->tex->w == item->tex->tex_w &&
           item->tex->h == item->tex->tex_h);

    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec3_set(p, (double)j / grid_size, (double)i / grid_size, 1.0);
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, p[0], p[1]);
        if (item->planet.normalmap) {
            compute_tangent(p, map, tangent);
            gl_buf_3f(&item->buf, -1, ATTR_TANGENT, VEC3_SPLIT(tangent));
        }

        vec3_set(p, (double)j / grid_size, (double)i / grid_size, 1.0);
        uv_map(map, p, p, normal);
        assert(p[3] == 1.0); // Planet can never be at infinity.

        gl_buf_3f(&item->buf, -1, ATTR_NORMAL, VEC3_SPLIT(normal));

        // Model position (without scaling applied).
        vec4_copy(p, mpos);
        assert(map->transf);
        vec3_sub(mpos, (*map->transf)[3], mpos);
        vec3_mul(1.0 / painter->planet.scale, mpos, mpos);
        vec3_add(mpos, (*map->transf)[3], mpos);
        gl_buf_3f(&item->buf, -1, ATTR_MPOS, VEC3_SPLIT(mpos));

        // Rendering position (with scaling applied).
        convert_framev4(painter->obs, frame, FRAME_VIEW, p, p);

        depth = proj_get_depth(painter->proj, p);
        rend->depth_min = min(rend->depth_min, depth);
        rend->depth_max = max(rend->depth_max, depth);

        gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(p));
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

void render_quad(renderer_t *rend, const painter_t *painter,
                 int frame, int grid_size, const uv_map_t *map)
{
    item_t *item;
    int n, i, j, k, ofs;
    const int INDICES[6][2] = {
        {0, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 0}, {0, 1} };
    double p[4], tex_pos[2], ndc_p[4];
    float lum;
    const double (*grid)[4] = NULL;
    bool should_delete_grid;
    texture_t *tex = painter->textures[PAINTER_TEX_COLOR].tex;

    // Special case for planet shader.
    if (painter->flags & (PAINTER_PLANET_SHADER | PAINTER_RING_SHADER))
        return quad_planet(rend, painter, frame, grid_size, map);

    if (!tex) tex = rend->white_tex;
    n = grid_size + 1;

    if (painter->flags & PAINTER_ATMOSPHERE_SHADER) {
        item = get_item(rend, ITEM_ATMOSPHERE,
                        n * n, grid_size * grid_size * 6, tex);
        if (item && (
                memcmp(item->atm.p, painter->atm.p, sizeof(item->atm.p)) ||
                memcmp(item->atm.sun, painter->atm.sun, sizeof(item->atm.sun))))
            item = NULL;
        if (!item) {
            item = calloc(1, sizeof(*item));
            item->type = ITEM_ATMOSPHERE;
            gl_buf_alloc(&item->buf, &ATMOSPHERE_BUF, 256);
            gl_buf_alloc(&item->indices, &INDICES_BUF, 256 * 6);
            memcpy(item->atm.p, painter->atm.p, sizeof(item->atm.p));
            memcpy(item->atm.sun, painter->atm.sun, sizeof(item->atm.sun));
        }
    } else if (painter->flags & PAINTER_FOG_SHADER) {
        item = get_item(rend, ITEM_FOG, n * n, grid_size * grid_size * 6, tex);
        if (!item) {
            item = calloc(1, sizeof(*item));
            item->type = ITEM_FOG;
            vec4_copy(painter->color, item->color);
            gl_buf_alloc(&item->buf, &FOG_BUF, 256);
            gl_buf_alloc(&item->indices, &INDICES_BUF, 256 * 6);
        }
    } else {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_TEXTURE;
        gl_buf_alloc(&item->buf, &TEXTURE_BUF, n * n);
        gl_buf_alloc(&item->indices, &INDICES_BUF, n * n * 6);
    }

    ofs = item->buf.nb;
    item->tex = tex;
    item->tex->ref++;
    vec4_to_float(painter->color, item->color);
    item->flags = painter->flags;

    grid = get_grid(rend, map, grid_size, &should_delete_grid);
    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
        vec3_set(p, (double)j / grid_size, (double)i / grid_size, 1.0);
        mat3_mul_vec3(painter->textures[PAINTER_TEX_COLOR].mat, p, p);
        tex_pos[0] = p[0] * tex->w / tex->tex_w;
        tex_pos[1] = p[1] * tex->h / tex->tex_h;
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, tex_pos[0], tex_pos[1]);

        vec4_set(p, VEC4_SPLIT(grid[i * n + j]));
        convert_framev4(painter->obs, frame, FRAME_VIEW, p, ndc_p);
        gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(ndc_p));
        // For atmosphere shader, in the first pass we do not compute the
        // luminance yet, only if the point is visible.
        if (painter->flags & PAINTER_ATMOSPHERE_SHADER) {
            gl_buf_3f(&item->buf, -1, ATTR_SKY_POS, VEC3_SPLIT(p));
            lum = painter->atm.compute_lum(painter->atm.user,
                    (float[3]){p[0], p[1], p[2]});
            gl_buf_1f(&item->buf, -1, ATTR_LUMINANCE, lum);
        }
        if (painter->flags & PAINTER_FOG_SHADER) {
            gl_buf_3f(&item->buf, -1, ATTR_SKY_POS, VEC3_SPLIT(p));
        }
        gl_buf_next(&item->buf);
    }
    if (should_delete_grid) free(grid);

    // Set the index buffer.
    for (i = 0; i < grid_size; i++)
    for (j = 0; j < grid_size; j++) {
        for (k = 0; k < 6; k++) {
            gl_buf_1i(&item->indices, -1, 0,
                      ofs + (INDICES[k][1] + i) * n + (INDICES[k][0] + j));
            gl_buf_next(&item->indices);
        }
    }
    DL_APPEND(rend->items, item);
}

static void texture_2d(renderer_t *rend, texture_t *tex,
                       double uv[4][2], double win_pos[4][2],
                       const double view_pos[3],
                       const double color_[4], int flags)
{
    int i, ofs;
    item_t *item;
    const int16_t INDICES[6] = {0, 1, 2, 3, 2, 1 };
    double depth;
    float color[4];

    assert((bool)view_pos == (bool)(flags & PAINTER_ENABLE_DEPTH));
    vec4_to_float(color_, color);
    item = get_item(rend, ITEM_TEXTURE_2D, 4, 6, tex);
    if (item && memcmp(item->color, color, sizeof(color))) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_TEXTURE_2D;
        item->flags = flags;
        gl_buf_alloc(&item->buf, &TEXTURE_2D_BUF, 64 * 4);
        gl_buf_alloc(&item->indices, &INDICES_BUF, 64 * 6);
        item->tex = tex;
        item->tex->ref++;
        memcpy(item->color, color, sizeof(color));
        DL_APPEND(rend->items, item);
    }

    if (flags & PAINTER_ENABLE_DEPTH) {
        depth = proj_get_depth(&rend->proj, view_pos);
        rend->depth_min = min(rend->depth_min, depth);
        rend->depth_max = max(rend->depth_max, depth);
    }

    ofs = item->buf.nb;
    for (i = 0; i < 4; i++) {
        gl_buf_2f(&item->buf, -1, ATTR_WPOS, win_pos[i][0], win_pos[i][1]);
        if (view_pos)
            gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(view_pos));
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, uv[i][0], uv[i][1]);
        gl_buf_next(&item->buf);
    }
    for (i = 0; i < 6; i++) {
        gl_buf_1i(&item->indices, -1, 0, ofs + INDICES[i]);
        gl_buf_next(&item->indices);
    }
}

void render_texture(renderer_t *rend, const texture_t  *tex,
                    double uv[4][2], const double pos[2], double size,
                    const double color[4], double angle)
{
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
    }
    texture_2d(rend, tex, uv, verts, NULL, color, 0);
}

static uint8_t img_get(const uint8_t *img, int w, int h, int x, int y)
{
    if (x < 0 || x >= w || y < 0 || y >= h) return 0;
    return img[y * w + x];
}

static void blend_color(double dst[4], const double src[4])
{
    double a;
    a = (1 - src[3]) * dst[3] + src[3];
    if (a == 0) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 0;
        return;
    }
    dst[0] = ((1 - src[3]) * dst[3] * dst[0] + src[3] * src[0]) / a;
    dst[1] = ((1 - src[3]) * dst[3] * dst[1] + src[3] * src[1]) / a;
    dst[2] = ((1 - src[3]) * dst[3] * dst[2] + src[3] * src[2]) / a;
    dst[3] = a;
}

static void text_shadow_effect(const uint8_t *src, uint8_t *dst,
                               int w, int h, const double color[3])
{
    int i, j, di, dj;
    double s;
    double text_col[4], frag[4];

    for (i = 0; i < h; i++)
    for (j = 0; j < w; j++) {
        // Compute shadow blur.
        // Note: could use some weigths.
        s = 0;
        for (di = -1; di <= 1; di++)
        for (dj = -1; dj <= 1; dj++) {
            s += img_get(src, w - 2, h - 2, j + dj - 1, i + di - 1) / 255.;
        }
        s /= 9;
        vec4_set(frag, color[0] / 8, color[1] / 8, color[2] / 8, s);
        // Blend real color on top of shadow.
        vec4_set(text_col, color[0], color[1], color[2],
                 img_get(src, w - 2, h - 2, j - 1, i - 1) / 255.);
        blend_color(frag, text_col);

        dst[(i * w + j) * 4 + 0] = frag[0] * 255;
        dst[(i * w + j) * 4 + 1] = frag[1] * 255;
        dst[(i * w + j) * 4 + 2] = frag[2] * 255;
        dst[(i * w + j) * 4 + 3] = frag[3] * 255;
    }
}

// Render text using a system bakend generated texture.
static void text_using_texture(renderer_t *rend,
                               const painter_t *painter,
                               const char *text, const double win_pos[2],
                               const double view_pos[3],
                               int align, int effects, double size,
                               const double color[4], double angle,
                               double out_bounds[4])
{
    double uv[4][2], verts[4][2];
    double s[2], ofs[2] = {0, 0}, bounds[4];
    const double scale = rend->scale;
    uint8_t *img, *img_rgba;
    int i, w, h, xoff, yoff, flags;
    tex_cache_t *ctex;
    texture_t *tex;
    assert(color);

    DL_FOREACH(rend->tex_cache, ctex) {
        if (ctex->size == size && ctex->effects == effects &&
                strcmp(ctex->text, text) == 0 &&
                memcmp(ctex->color, color, sizeof(ctex->color)) == 0) break;
    }

    if (!ctex) {
        img = (void*)sys_render_text(text, size * scale, effects, &w, &h,
                                     &xoff, &yoff);
        // Shadow effect, into a texture with one pixel extra border.
        w += 2;
        h += 2;
        img_rgba = malloc(w * h * 4);
        text_shadow_effect(img, img_rgba, w, h, color);
        free(img);
        ctex = calloc(1, sizeof(*ctex));
        ctex->size = size;
        ctex->effects = effects;
        ctex->xoff = xoff;
        ctex->yoff = yoff;
        ctex->text = strdup(text);
        ctex->tex = texture_from_data(img_rgba, w, h, 4, 0, 0, w, h, 0);
        vec3_copy(color, ctex->color);
        free(img_rgba);
        DL_APPEND(rend->tex_cache, ctex);
    }

    ctex->in_use = true;

    // Compute bounds taking alignment into account.
    s[0] = ctex->tex->w / scale;
    s[1] = ctex->tex->h / scale;
    if (align & ALIGN_LEFT)     ofs[0] = +s[0] / 2;
    if (align & ALIGN_RIGHT)    ofs[0] = -s[0] / 2;
    if (align & ALIGN_TOP)      ofs[1] = +s[1] / 2;
    if (align & ALIGN_BOTTOM)   ofs[1] = -s[1] / 2;
    bounds[0] = win_pos[0] - s[0] / 2 + ofs[0] + ctex->xoff / scale;
    bounds[1] = win_pos[1] - s[1] / 2 + ofs[1] + ctex->yoff / scale;

    // Round the position to the nearest pixel.  We add a small delta to
    // fix a bug when we are exactly in between two pixels, which can happen
    // for example with the label of a centered object.
    if (!angle) bounds[0] = round(bounds[0] * scale + 0.000001) / scale;
    if (!angle) bounds[1] = round(bounds[1] * scale + 0.000001) / scale;

    bounds[2] = bounds[0] + s[0];
    bounds[3] = bounds[1] + s[1];

    if (out_bounds) {
        memcpy(out_bounds, bounds, sizeof(bounds));
        return;
    }
    tex = ctex->tex;

    /*
     * Render the texture, being careful to do the rotation centered on
     * the anchor point.
     */
    for (i = 0; i < 4; i++) {
        uv[i][0] = ((i % 2) * tex->w) / (double)tex->tex_w;
        uv[i][1] = ((i / 2) * tex->h) / (double)tex->tex_h;
        verts[i][0] = (i % 2 - 0.5) * tex->w / scale;
        verts[i][1] = (0.5 - i / 2) * tex->h / scale;
        verts[i][0] += ofs[0];
        verts[i][1] += ofs[1];
        vec2_rotate(angle, verts[i], verts[i]);
        verts[i][0] -= ofs[0];
        verts[i][1] -= ofs[1];
        verts[i][0] += (bounds[0] + bounds[2]) / 2;
        verts[i][1] += (bounds[1] + bounds[3]) / 2;
    }

    flags = painter->flags;
    texture_2d(rend, tex, uv, verts, view_pos, VEC(1, 1, 1, color[3]), flags);
}

// Render text using nanovg.
static void text_using_nanovg(renderer_t *rend,
                              const painter_t *painter,
                              const char *text,
                              const double pos[2], int align, int effects,
                              double size, const double color[4], double angle,
                              double bounds[4])
{
    item_t *item;
    float fbounds[4];
    int font = (effects & TEXT_BOLD) ? FONT_BOLD : FONT_REGULAR;

    if (strlen(text) >= sizeof(item->text.text)) {
        LOG_W("Text too large: %s", text);
        return;
    }

    if (!bounds) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_TEXT;
        item->flags = painter->flags;
        vec4_to_float(color, item->color);
        item->color[0] = clamp(item->color[0], 0.0, 1.0);
        item->color[1] = clamp(item->color[1], 0.0, 1.0);
        item->color[2] = clamp(item->color[2], 0.0, 1.0);
        item->color[3] = clamp(item->color[3], 0.0, 1.0);
        vec2_to_float(pos, item->text.pos);
        item->text.size = size;
        item->text.align = align;
        item->text.effects = effects;
        item->text.angle = angle;
        if (effects & TEXT_UPPERCASE || effects & TEXT_SMALL_CAP) {
            // Emulate Small Cap effect by doing regular capitalize
            u8_upper(item->text.text, text, sizeof(item->text.text) - 1);
        } else {
            strcpy(item->text.text, text);
        }
        DL_APPEND(rend->items, item);
    }
    if (bounds) {
        nvgSave(rend->vg);
        nvgFontFaceId(rend->vg, rend->fonts[font].id);
        nvgFontSize(rend->vg, size * rend->fonts[font].scale);
        nvgTextAlign(rend->vg, align);
        nvgTextBounds(rend->vg, pos[0], pos[1], text, NULL, fbounds);
        bounds[0] = fbounds[0];
        bounds[1] = fbounds[1];
        bounds[2] = fbounds[2];
        bounds[3] = fbounds[3];
        nvgRestore(rend->vg);
    }
}

void render_text(renderer_t *rend, const painter_t *painter,
                 const char *text, const double win_pos[2],
                 const double view_pos[3],
                 int align, int effects, double size,
                 const double color[4], double angle,
                 double bounds[4])
{
    assert(win_pos);
    assert(size);

    // Prevent overflow in nvg.
    if (fabs(win_pos[0]) > 100000 || fabs(win_pos[1]) > 100000) {
        LOG_W_ONCE("Render text far outside screen: %s, %f %f",
                   text, win_pos[0], win_pos[1]);
        if (bounds) {
            bounds[0] = win_pos[0];
            bounds[1] = win_pos[1];
        }
        return;
    }

    if (sys_callbacks.render_text) {
        text_using_texture(rend, painter, text, win_pos, view_pos, align,
                           effects, size, color, angle, bounds);
    } else {
        text_using_nanovg(rend, painter, text, win_pos, align, effects, size,
                          color, angle, bounds);
    }

}

static void item_points_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    GLuint  array_buffer;
    double core_size;

    if (item->buf.nb <= 0) {
        LOG_W("Empty point buffer");
        return;
    }

    shader = shader_get("points", NULL, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE));

    if (item->flags & PAINTER_ENABLE_DEPTH)
        GL(glEnable(GL_DEPTH_TEST));
    else
        GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    gl_update_uniform(shader, "u_color", item->color);
    core_size = 1.0 / item->points.halo;
    gl_update_uniform(shader, "u_core_size", core_size);

    gl_buf_enable(&item->buf);
    GL(glDrawArrays(GL_POINTS, 0, item->buf.nb));
    gl_buf_disable(&item->buf);

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDisable(GL_DEPTH_TEST));
}

static void item_points_3d_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    GLuint  array_buffer;
    double core_size;
    projection_t proj;
    float matf[16];

    if (item->buf.nb <= 0)
        return;

    shader_define_t defines[] = {
        {"IS_3D", true},
        {"PROJ", rend->proj.klass->id},
        {}
    };

    shader = shader_get("points", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE));

    if (item->flags & PAINTER_ENABLE_DEPTH)
        GL(glEnable(GL_DEPTH_TEST));
    else
        GL(glDisable(GL_DEPTH_TEST));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, item->buf.nb * item->buf.info->size,
                    item->buf.data, GL_DYNAMIC_DRAW));

    gl_update_uniform(shader, "u_color", item->color);
    core_size = 1.0 / item->points.halo;
    gl_update_uniform(shader, "u_core_size", core_size);

    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    gl_buf_enable(&item->buf);
    GL(glDrawArrays(GL_POINTS, 0, item->buf.nb));
    gl_buf_disable(&item->buf);

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDisable(GL_DEPTH_TEST));
}

static void draw_buffer(const gl_buf_t *buf, const gl_buf_t *indices,
                        GLuint gl_mode)
{
    GLuint  array_buffer;
    GLuint  index_buffer;

    GL(glGenBuffers(1, &index_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    indices->nb * indices->info->size,
                    indices->data, GL_DYNAMIC_DRAW));

    GL(glGenBuffers(1, &array_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, buf->nb * buf->info->size,
                    buf->data, GL_DYNAMIC_DRAW));

    gl_buf_enable(buf);
    GL(glDrawElements(gl_mode, indices->nb, GL_UNSIGNED_SHORT, 0));
    gl_buf_disable(buf);

    GL(glDeleteBuffers(1, &array_buffer));
    GL(glDeleteBuffers(1, &index_buffer));
}

static void item_mesh_render(renderer_t *rend, const item_t *item)
{
    // XXX: almost the same as item_lines_render.
    gl_shader_t *shader;
    int gl_mode;
    float matf[16];
    float fbo_size[2] = {rend->fb_size[0] / rend->scale,
                         rend->fb_size[1] / rend->scale};
    projection_t proj;

    gl_mode = item->mesh.mode == 0 ? GL_TRIANGLES :
              item->mesh.mode == 1 ? GL_LINES :
              item->mesh.mode == 2 ? GL_POINTS : 0;

    shader_define_t defines[] = {
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("mesh", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));

    GL(glLineWidth(item->mesh.stroke_width));

    // For the moment we disable culling for mesh.  We should reintroduce it
    // by making sure we use the proper value depending on the render
    // culling and frame.
    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ZERO, GL_ONE));

    // Stencil hack to remove projection deformations artifacts.
    if (item->mesh.use_stencil) {
        GL(glClear(GL_STENCIL_BUFFER_BIT));
        GL(glEnable(GL_STENCIL_TEST));
        GL(glStencilFunc(GL_NOTEQUAL, 1, 0xFF));
        GL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
    }

    gl_update_uniform(shader, "u_fbo_size", fbo_size);
    gl_update_uniform(shader, "u_proj_scaling", item->mesh.proj_scaling);

    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    draw_buffer(&item->buf, &item->indices, gl_mode);

    if (item->mesh.use_stencil) {
        GL(glDisable(GL_STENCIL_TEST));
        GL(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    }
}

// XXX: almost the same as item_mesh_render!
static void item_lines_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    float win_size[2] = {rend->fb_size[0] / rend->scale,
                         rend->fb_size[1] / rend->scale};
    float matf[16];
    projection_t proj;

    shader_define_t defines[] = {
        {"DASH", item->lines.dash_length && (item->lines.dash_ratio < 1.0)},
        {"FADE", item->lines.fade_dist_min ? 1 : 0},
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("lines", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));

    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ZERO, GL_ONE));
    if (item->flags & PAINTER_ENABLE_DEPTH)
        GL(glEnable(GL_DEPTH_TEST));

    gl_update_uniform(shader, "u_line_width", item->lines.width);
    gl_update_uniform(shader, "u_line_glow", item->lines.glow);
    gl_update_uniform(shader, "u_color", item->color);
    gl_update_uniform(shader, "u_win_size", win_size);

    gl_update_uniform(shader, "u_dash_length", item->lines.dash_length);
    gl_update_uniform(shader, "u_dash_ratio", item->lines.dash_ratio);

    if (item->lines.fade_dist_min) {
        gl_update_uniform(shader, "u_fade_dist_min", item->lines.fade_dist_min);
        gl_update_uniform(shader, "u_fade_dist_max", item->lines.fade_dist_max);
    }

    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glDisable(GL_DEPTH_TEST));
}

static void item_vg_render(renderer_t *rend, const item_t *item)
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
    nvgStrokeWidth(rend->vg, item->vg.stroke_width);
    nvgStroke(rend->vg);
    nvgRestore(rend->vg);
    nvgEndFrame(rend->vg);

    // Reset colormask to its original value.
    GL(glColorMask(true, true, true, false));
}

static void item_text_render(renderer_t *rend, const item_t *item)
{
    int font = (item->text.effects & TEXT_BOLD) ? FONT_BOLD : FONT_REGULAR;
    nvgBeginFrame(rend->vg, rend->fb_size[0] / rend->scale,
                            rend->fb_size[1] / rend->scale, rend->scale);
    nvgSave(rend->vg);
    nvgTranslate(rend->vg, item->text.pos[0], item->text.pos[1]);
    nvgRotate(rend->vg, item->text.angle);

    nvgFontFaceId(rend->vg, rend->fonts[font].id);

    if (sys_lang_supports_spacing() && item->text.effects & TEXT_SPACED)
        nvgTextLetterSpacing(rend->vg, round(item->text.size *
                             rend->fonts[font].scale * 0.2));
    if (sys_lang_supports_spacing() && item->text.effects & TEXT_SEMI_SPACED)
        nvgTextLetterSpacing(rend->vg, round(item->text.size *
                             rend->fonts[font].scale * 0.05));
    nvgFontSize(rend->vg, item->text.size * rend->fonts[font].scale);
    nvgFillColor(rend->vg, nvgRGBA(item->color[0] * 255,
                                   item->color[1] * 255,
                                   item->color[2] * 255,
                                   item->color[3] * 255));
    nvgTextAlign(rend->vg, item->text.align);
    nvgText(rend->vg, 0, 0, item->text.text, NULL);

    // Uncomment to see labels bounding box
    if ((0)) {
        float bounds[4];
        nvgTextBounds(rend->vg, 0, 0, item->text.text, NULL, bounds);
        nvgBeginPath(rend->vg);
        nvgRect(rend->vg, bounds[0],bounds[1], bounds[2]-bounds[0],
                bounds[3]-bounds[1]);
        nvgStrokeColor(rend->vg, nvgRGBA(item->color[0] * 255,
                       item->color[1] * 255,
                       item->color[2] * 255,
                       item->color[3] * 255));
        nvgStroke(rend->vg);
    }

    nvgRestore(rend->vg);
    nvgEndFrame(rend->vg);
}

static void item_fog_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    float matf[16];
    projection_t proj;

    shader_define_t defines[] = {
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("fog", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(rend->cull_flipped ? GL_FRONT : GL_BACK));
    GL(glEnable(GL_BLEND));
    GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ZERO, GL_ONE));
    GL(glDisable(GL_DEPTH_TEST));

    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);
    gl_update_uniform(shader, "u_color", item->color);

    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glCullFace(GL_BACK));
}

static void item_atmosphere_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    float tm[3];
    float matf[16];
    projection_t proj;

    shader_define_t defines[] = {
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("atmosphere", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(rend->cull_flipped ? GL_FRONT : GL_BACK));

    GL(glEnable(GL_BLEND));
    if (color_is_white(item->color)) {
        GL(glBlendFunc(GL_ONE, GL_ONE));
    } else {
        GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE));
        GL(glBlendColor(item->color[0] * item->color[3],
                        item->color[1] * item->color[3],
                        item->color[2] * item->color[3],
                        item->color[3]));
    }

    gl_update_uniform(shader, "u_color", item->color);
    gl_update_uniform(shader, "u_atm_p", item->atm.p);
    gl_update_uniform(shader, "u_sun", item->atm.sun);
    // XXX: the tonemapping args should be copied before rendering!
    tm[0] = core->tonemapper.p;
    tm[1] = core->tonemapper.lwmax;
    tm[2] = core->tonemapper.exposure;
    gl_update_uniform(shader, "u_tm", tm);

    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glCullFace(GL_BACK));
}

static void item_texture_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    float matf[16];
    projection_t proj;

    shader_define_t defines[] = {
        {"TEXTURE_LUMINANCE", item->tex->format == GL_LUMINANCE &&
                              !(item->flags & PAINTER_ADD)},
        {"PROJ", item->type == ITEM_TEXTURE ? rend->proj.klass->id : 0},
        {}
    };
    shader = shader_get("blit", defines, ATTR_NAMES, init_shader);

    GL(glUseProgram(shader->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(rend->cull_flipped ? GL_FRONT : GL_BACK));

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

    gl_update_uniform(shader, "u_color", item->color);
    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glCullFace(GL_BACK));
}

static void item_texture_2d_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    float matf[16];
    projection_t proj;
    float win_size[2] = {rend->fb_size[0] / rend->scale,
                         rend->fb_size[1] / rend->scale};
    shader_define_t defines[] = {
        {"TEXTURE_LUMINANCE", item->tex->format == GL_LUMINANCE &&
                              !(item->flags & PAINTER_ADD)},
        {"HAS_VIEW_POS", (bool)(item->flags & PAINTER_ENABLE_DEPTH)},
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("texture_2d", defines, ATTR_NAMES, init_shader);
    GL(glUseProgram(shader->prog));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));
    if (item->tex->format == GL_RGB && item->color[3] == 1.0) {
        GL(glDisable(GL_BLEND));
    } else {
        GL(glEnable(GL_BLEND));
        GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                               GL_ZERO, GL_ONE));
    }
    if (item->flags & PAINTER_ENABLE_DEPTH)
        GL(glEnable(GL_DEPTH_TEST));
    gl_update_uniform(shader, "u_color", item->color);
    gl_update_uniform(shader, "u_win_size", win_size);
    proj = rend_get_proj(rend, item->flags);
    mat4_to_float(proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);
    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glDisable(GL_DEPTH_TEST));
}

static void item_planet_render(renderer_t *rend, const item_t *item)
{
    gl_shader_t *shader;
    bool is_moon;
    shader_define_t defines[] = {
        {"HAS_SHADOW", item->planet.shadow_spheres_nb > 0},
        {"PROJ", rend->proj.klass->id},
        {}
    };
    shader = shader_get("planet", defines, ATTR_NAMES, init_shader);

    GL(glUseProgram(shader->prog));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, item->tex->id));

    GL(glActiveTexture(GL_TEXTURE1));
    if (item->planet.normalmap) {
        GL(glBindTexture(GL_TEXTURE_2D, item->planet.normalmap->id));
        gl_update_uniform(shader, "u_has_normal_tex", 1);
    } else {
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));
        gl_update_uniform(shader, "u_has_normal_tex", 0);
    }

    GL(glActiveTexture(GL_TEXTURE2));
    if (item->planet.shadow_color_tex &&
            texture_load(item->planet.shadow_color_tex, NULL))
        GL(glBindTexture(GL_TEXTURE_2D, item->planet.shadow_color_tex->id));
    else
        GL(glBindTexture(GL_TEXTURE_2D, rend->white_tex->id));

    if (item->flags & PAINTER_RING_SHADER) {
        GL(glDisable(GL_CULL_FACE));
    } else {
        GL(glEnable(GL_CULL_FACE));
        GL(glCullFace(rend->cull_flipped ? GL_FRONT : GL_BACK));
    }

    if (item->tex->format == GL_RGB && item->color[3] == 1.0) {
        GL(glDisable(GL_BLEND));
    } else {
        GL(glEnable(GL_BLEND));
        GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                               GL_ZERO, GL_ONE));
    }
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthMask(GL_TRUE));

    // Set all uniforms.
    is_moon = item->flags & PAINTER_IS_MOON;
    gl_update_uniform(shader, "u_color", item->color);
    gl_update_uniform(shader, "u_contrast", item->planet.contrast);
    gl_update_uniform(shader, "u_sun", item->planet.sun);
    gl_update_uniform(shader, "u_light_emit", item->planet.light_emit);
    gl_update_uniform(shader, "u_min_brightness", item->planet.min_brightness);
    gl_update_uniform(shader, "u_material", item->planet.material);
    gl_update_uniform(shader, "u_is_moon", is_moon ? 1 : 0);
    gl_update_uniform(shader, "u_mv", item->planet.mv);
    gl_update_uniform(shader, "u_shadow_spheres_nb",
                      item->planet.shadow_spheres_nb);
    gl_update_uniform(shader, "u_shadow_spheres", item->planet.shadow_spheres);
    gl_update_uniform(shader, "u_tex_transf", item->planet.tex_transf);
    gl_update_uniform(shader, "u_normal_tex_transf",
                      item->planet.normal_tex_transf);

    float matf[16];
    mat4_to_float(rend->proj.mat, matf);
    gl_update_uniform(shader, "u_proj_mat", matf);

    draw_buffer(&item->buf, &item->indices, GL_TRIANGLES);
    GL(glCullFace(GL_BACK));
    GL(glDepthMask(GL_FALSE));
    GL(glDisable(GL_DEPTH_TEST));
}

static void item_gltf_render(renderer_t *rend, const item_t *item)
{
    double proj[4][4], nearval, farval;
    mat4_copy(item->gltf.proj_mat, proj);

    // Fix the depth range of the projection to the current frame values.
    if (item->flags & PAINTER_ENABLE_DEPTH) {
        nearval = rend->depth_min * DAU2M;
        farval = rend->depth_max * DAU2M;
        proj[2][2] = (farval + nearval) / (nearval - farval);
        proj[3][2] = 2. * farval * nearval / (nearval - farval);
    }

    gltf_render(item->gltf.model, item->gltf.model_mat, item->gltf.view_mat,
                proj, item->gltf.light_dir, item->gltf.args);
}

static void rend_flush(renderer_t *rend)
{
    item_t *item, *tmp;

    // Compute depth range.
    if (rend->depth_min == DBL_MAX) {
        rend->depth_min = 0;
        rend->depth_max = 1;
    }
    rend->depth_min = max(rend->depth_min, 10 * DM2AU);

    // Add a small margin.  Note: we increase the max depth a lot since this
    // doesn't affect the precision that much and it fixes some errors with
    // far away points.
    rend->depth_min *= 0.99;
    rend->depth_max *= 2.00;
    proj_set_depth_range(&rend->proj, rend->depth_min, rend->depth_max);

    // Set default OpenGL state.
    // Make sure we clear everything.
    GL(glClearColor(0.0, 0.0, 0.0, 1.0));
    GL(glColorMask(true, true, true, true));
    GL(glDepthMask(true));
    GL(glStencilMask(0xff));
    GL(glClear(GL_COLOR_BUFFER_BIT |
               GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));

    GL(glViewport(0, 0, rend->fb_size[0], rend->fb_size[1]));
    GL(glDepthMask(GL_FALSE));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glColorMask(true, true, true, false)); // Do not change the alpha.

    // On OpenGL Desktop, we have to enable point sprite support.
#ifndef GLES2
    GL(glEnable(GL_PROGRAM_POINT_SIZE));
    GL(glEnable(GL_POINT_SPRITE));
#endif

    DL_FOREACH_SAFE(rend->items, item, tmp) {
        switch (item->type) {
        case ITEM_LINES:
            item_lines_render(rend, item);
            break;
        case ITEM_MESH:
            item_mesh_render(rend, item);
            break;
        case ITEM_POINTS:
            item_points_render(rend, item);
            break;
        case ITEM_POINTS_3D:
            item_points_3d_render(rend, item);
            break;
        case ITEM_TEXTURE:
            item_texture_render(rend, item);
            break;
        case ITEM_TEXTURE_2D:
            item_texture_2d_render(rend, item);
            break;
        case ITEM_ATMOSPHERE:
            item_atmosphere_render(rend, item);
            break;
        case ITEM_FOG:
            item_fog_render(rend, item);
            break;
        case ITEM_PLANET:
            item_planet_render(rend, item);
            break;
        case ITEM_VG_ELLIPSE:
        case ITEM_VG_RECT:
        case ITEM_VG_LINE:
            item_vg_render(rend, item);
            break;
        case ITEM_TEXT:
            item_text_render(rend, item);
            break;
        case ITEM_GLTF:
            item_gltf_render(rend, item);
            break;
        default:
            assert(false);
        }

        DL_DELETE(rend->items, item);
        texture_release(item->tex);
        if (item->type == ITEM_PLANET)
            texture_release(item->planet.normalmap);
        if (item->type == ITEM_GLTF)
            json_builder_free(item->gltf.args);
        gl_buf_release(&item->buf);
        gl_buf_release(&item->indices);
        free(item);
    }
    // Reset to default OpenGL settings.
    GL(glDepthMask(GL_TRUE));
    GL(glColorMask(true, true, true, true));
}

void render_finish(renderer_t *rend)
{
    rend_flush(rend);
}

void render_line(renderer_t *rend, const painter_t *painter,
                 const double (*line)[3], const double (*win)[3], int size)
{
    line_mesh_t *mesh;
    int i, ofs;
    float color[4];
    double depth;
    item_t *item;
    const int SIZE = 2048;

    if (size <= 1) return;
    assert(painter->lines.glow); // Only glowing lines supported for now.
    vec4_to_float(painter->color, color);
    mesh = line_to_mesh(line, win, size, max(10, painter->lines.width + 2));

    if (mesh->indices_count >= SIZE || mesh->verts_count >= SIZE) {
        LOG_W("Too many points in lines! (size: %d)", size);
        goto end;
    }

    // Get the item.
    item = get_item(rend, ITEM_LINES, mesh->verts_count,
                    mesh->indices_count, NULL);
    if (item && memcmp(item->color, color, sizeof(color))) item = NULL;
    if (item && item->lines.dash_length != painter->lines.dash_length)
        item = NULL;
    if (item && item->lines.dash_ratio != painter->lines.dash_ratio)
        item = NULL;
    if (item && item->lines.width != painter->lines.width)
        item = NULL;
    if (item && item->flags != painter->flags)
        item = NULL;
    if (item && painter->lines.fade_dist_min != item->lines.fade_dist_min)
        item = NULL;
    if (item && painter->lines.fade_dist_max != item->lines.fade_dist_max)
        item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_LINES;
        item->flags = painter->flags;
        gl_buf_alloc(&item->buf, &LINES_BUF, SIZE);
        gl_buf_alloc(&item->indices, &INDICES_BUF, SIZE);
        item->lines.width = painter->lines.width;
        item->lines.glow = painter->lines.glow;
        item->lines.dash_length = painter->lines.dash_length;
        item->lines.dash_ratio = painter->lines.dash_ratio;
        item->lines.fade_dist_min = painter->lines.fade_dist_min;
        item->lines.fade_dist_max = painter->lines.fade_dist_max;
        memcpy(item->color, color, sizeof(color));
        DL_APPEND(rend->items, item);
    }

    if (item->flags & PAINTER_ENABLE_DEPTH) {
        // Compute the depth range.
        for (i = 0; i < size; i++) {
            depth = proj_get_depth(painter->proj, line[i]);
            rend->depth_min = min(rend->depth_min, depth);
            rend->depth_max = max(rend->depth_max, depth);
        }
    }

    // Append the mesh to the buffer.
    ofs = item->buf.nb;
    for (i = 0; i < mesh->verts_count; i++) {
        gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(mesh->verts[i].pos));
        gl_buf_2f(&item->buf, -1, ATTR_WPOS, VEC2_SPLIT(mesh->verts[i].win));
        gl_buf_2f(&item->buf, -1, ATTR_TEX_POS, VEC2_SPLIT(mesh->verts[i].uv));
        gl_buf_next(&item->buf);
    }
    for (i = 0; i < mesh->indices_count; i++) {
        gl_buf_1i(&item->indices, -1, 0, mesh->indices[i] + ofs);
        gl_buf_next(&item->indices);
    }

end:
    line_mesh_delete(mesh);
}

void render_mesh(renderer_t *rend, const painter_t *painter,
                 int frame, int mode, int verts_count,
                 const double verts[][3], int indices_count,
                 const uint16_t indices[], bool use_stencil)
{
    int i, ofs;
    double pos[4] = {};
    uint8_t color[4];
    item_t *item;

    color[0] = painter->color[0] * 255;
    color[1] = painter->color[1] * 255;
    color[2] = painter->color[2] * 255;
    color[3] = painter->color[3] * 255;
    if (!color[3]) return;

    item = get_item(rend, ITEM_MESH, verts_count, indices_count, NULL);
    if (item && (use_stencil != item->mesh.use_stencil)) item = NULL;
    if (item && item->mesh.mode != mode) item = NULL;
    if (item && item->mesh.stroke_width != painter->lines.width) item = NULL;

    if (!item) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_MESH;
        item->mesh.mode = mode;
        item->mesh.stroke_width = painter->lines.width;
        item->mesh.use_stencil = use_stencil;
        gl_buf_alloc(&item->buf, &MESH_BUF, max(verts_count, 1024));
        gl_buf_alloc(&item->indices, &INDICES_BUF, max(indices_count, 1024));
        DL_APPEND(rend->items, item);
    }

    ofs = item->buf.nb;

    for (i = 0; i < verts_count; i++) {
        vec3_copy(verts[i], pos);
        pos[3] = 0.0;
        vec3_normalize(pos, pos);
        convert_frame(painter->obs, frame, FRAME_VIEW, true, pos, pos);
        gl_buf_3f(&item->buf, -1, ATTR_POS, VEC3_SPLIT(pos));
        gl_buf_4i(&item->buf, -1, ATTR_COLOR, VEC4_SPLIT(color));
        gl_buf_next(&item->buf);
    }

    // Fill the indice buffer.
    for (i = 0; i < indices_count; i++) {
        gl_buf_1i(&item->indices, -1, 0, indices[i] + ofs);
        gl_buf_next(&item->indices);
    }
}

void render_ellipse_2d(renderer_t *rend, const painter_t *painter,
                       const double pos[2], const double size[2],
                       double angle, double dashes)
{
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_ELLIPSE;
    vec2_to_float(pos, item->vg.pos);
    vec2_to_float(size, item->vg.size);
    vec4_to_float(painter->color, item->color);
    item->vg.angle = angle;
    item->vg.dashes = dashes;
    item->vg.stroke_width = painter->lines.width;
    DL_APPEND(rend->items, item);
}

void render_rect_2d(renderer_t *rend, const painter_t *painter,
                    const double pos[2], const double size[2],
                    double angle)
{
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_RECT;
    vec2_to_float(pos, item->vg.pos);
    vec2_to_float(size, item->vg.size);
    vec4_to_float(painter->color, item->color);
    item->vg.angle = angle;
    item->vg.stroke_width = painter->lines.width;
    DL_APPEND(rend->items, item);
}

void render_line_2d(renderer_t *rend, const painter_t *painter,
                    const double p1[2], const double p2[2])
{
    item_t *item;
    item = calloc(1, sizeof(*item));
    item->type = ITEM_VG_LINE;
    vec2_to_float(p1, item->vg.pos);
    vec2_to_float(p2, item->vg.pos2);
    vec4_to_float(painter->color, item->color);
    item->vg.stroke_width = painter->lines.width;
    DL_APPEND(rend->items, item);
}

static void get_model_depth_range(
        const painter_t *painter, const char *model,
        const double model_mat[4][4], const double view_mat[4][4],
        double out_range[2])
{
    /*
     * Note: in theory this should just be the range of depth computed on
     * the eight corners of the bounding box, but since the depth function
     * of most projection is the distance (and not just -z) this doesn't work
     * well in practice.
     *
     * Here we first compute the largest diagonal of the model, then add
     * and subscract if from the center position to get the min and max
     * depth.
     */
    double bounds[2][3];
    int i, r;
    double size, dist, p[3], p2[3];
    r = painter_get_3d_model_bounds(painter, model, bounds);
    (void)r;
    assert(r == 0);

    size = 0;
    for (i = 0; i < 8; i++) {
        p[0] = bounds[(i >> 0) & 1][0];
        p[1] = bounds[(i >> 1) & 1][1];
        p[2] = bounds[(i >> 2) & 1][2];
        mat4_mul_dir3(model_mat, p, p);
        size = max(size, vec3_norm2(p));
    }
    size = sqrt(size);

    mat4_mul_vec3(model_mat, VEC(0, 0, 0), p);
    mat4_mul_vec3(view_mat, p, p);
    dist = vec3_norm(p);

    vec3_mul((dist - size) / dist, p, p2);
    out_range[0] = proj_get_depth(painter->proj, p2) * DM2AU;
    vec3_mul((dist + size) / dist, p, p2);
    out_range[1] = proj_get_depth(painter->proj, p2) * DM2AU;
}

void render_model_3d(renderer_t *rend, const painter_t *painter,
                     const char *model, const double model_mat[4][4],
                     const double view_mat[4][4], const double proj_mat[4][4],
                     const double light_dir[3], json_value *args)
{
    item_t *item;
    double depth_range[2];

    item = calloc(1, sizeof(*item));
    item->type = ITEM_GLTF;
    item->gltf.model = model;
    item->flags = painter->flags;
    mat4_copy(model_mat, item->gltf.model_mat);
    mat4_copy(view_mat, item->gltf.view_mat);
    mat4_copy(proj_mat, item->gltf.proj_mat);
    vec3_copy(light_dir, item->gltf.light_dir);

    item->flags |= PAINTER_ENABLE_DEPTH;
    get_model_depth_range(painter, model, model_mat, view_mat, depth_range);
    rend->depth_min = min(rend->depth_min, depth_range[0]);
    rend->depth_max = max(rend->depth_max, depth_range[1]);

    if (args) item->gltf.args = json_copy(args);
    DL_APPEND(rend->items, item);
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

EMSCRIPTEN_KEEPALIVE
void core_add_font(renderer_t *rend, const char *name,
                   const char *url, const uint8_t *data,
                   int size, float scale)
{
    int id;
    int font;
    rend = rend ?: (void*)core->rend;

    if (!data) {
        data = asset_get_data(url, &size, NULL);
        assert(data);
    }

    if (strcmp(name, "regular") == 0)
        font = FONT_REGULAR;
    else if (strcmp(name, "bold") == 0)
        font = FONT_BOLD;
    else {
        assert(false);
        return;
    }

    id = nvgCreateFontMem(rend->vg, name, data, size, 0);
    if (!rend->fonts[font].id || rend->fonts[font].is_default_font) {
        rend->fonts[font].id = id;
        rend->fonts[font].scale = scale;
        rend->fonts[font].is_default_font = false;
    } else {
        nvgAddFallbackFontId(rend->vg, rend->fonts[font].id, id);
    }
}

static void set_default_fonts(renderer_t *rend)
{
    const float scale = 1.38;
    core_add_font(rend, "regular", "asset://font/NotoSans-Regular.ttf",
                  NULL, 0, scale);
    core_add_font(rend, "bold", "asset://font/NotoSans-Bold.ttf",
                  NULL, 0, scale);
    rend->fonts[FONT_REGULAR].is_default_font = true;
    rend->fonts[FONT_BOLD].is_default_font = true;
}

renderer_t* render_create(void)
{
    renderer_t *rend;
    GLint range[2];

#ifdef WIN32
    glewInit();
#endif

    rend = calloc(1, sizeof(*rend));
    rend->white_tex = create_white_texture(16, 16);
#ifdef GLES2
    rend->vg = nvgCreateGLES2(NVG_ANTIALIAS);
#else
    rend->vg = nvgCreateGL2(NVG_ANTIALIAS);
#endif

    if (!sys_callbacks.render_text)
        set_default_fonts(rend);

    // Query the point size range.
    GL(glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, range));
    if (range[1] < 32)
        LOG_W("OpenGL Doesn't support large point size!");

    return rend;
}
