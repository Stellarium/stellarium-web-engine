/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static texture_t *g_tex = NULL;

static void glc_paint(const painter_t *painter, const double transf[4][4]);
static void opc_paint(const painter_t *painter, const double transf[4][4]);
static void cls_paint(const painter_t *painter, const double transf[4][4]);
static void   g_paint(const painter_t *painter, const double transf[4][4]);
static void  pn_paint(const painter_t *painter, const double transf[4][4]);
static void ism_paint(const painter_t *painter, const double transf[4][4]);
static void bne_paint(const painter_t *painter, const double transf[4][4]);

// Match the list of svg files in tool/makedata.py.
// We can probably do better than that.
static const struct {
    const char  *id;
    uint32_t    color;
    void        (*paint)(const painter_t *painter, const double transf[4][4]);
} ENTRIES[] = {
    {"POIN", 0x4CFF4CFF},
    {"POIN2",0x4CFF4CFF},

    {"OpC" , 0xF2E9267F, opc_paint},
    {"GlC" , 0xF2E9267F, glc_paint},
    {"G"   , 0xFF930E7F,   g_paint},
    {"PN"  , 0xF2E9267F,  pn_paint},
    {"ISM" , 0xF2E9267F, ism_paint},
    {"BNe" , 0x89ff5f7f, bne_paint},
    {"Cl*" , 0x89ff5f7f, cls_paint},

    {"Ast", 0xff00ffff},

    {"btn_landscape", 0xffffffff},
    {"btn_atmosphere", 0xffffffff},
    {"btn_cst_lines", 0xffffffff},
};

static texture_t *get_texture(void)
{
    if (!g_tex) {
        g_tex = texture_from_url("asset://symbols.png", TF_MIPMAP);
        assert(g_tex->tex_w == g_tex->w);
        assert(g_tex->tex_h == g_tex->h);
    }
    return g_tex;
}

static int get_index(const char *id)
{
    int i;
    assert(id);
    for (i = 0; i < ARRAY_SIZE(ENTRIES); i++)
        if (str_equ(ENTRIES[i].id, id)) return i;
    return -1;
}

texture_t *symbols_get(const char *id, double rect[2][2])
{
    int index;
    index = get_index(id);
    if (index < 0) return NULL;
    if (rect) {
        vec2_set(rect[0], (index % 8) / 8.0, (index / 8) / 8.0);
        vec2_set(rect[1], rect[0][0] + 1 / 8.0, rect[0][1] + 1 / 8.0);
    }
    return get_texture();
}

static void opc_paint(const painter_t *painter, const double transf[4][4])
{
    double dashes = M_PI * 12 / 8;
    paint_2d_ellipse(painter, transf, dashes, NULL);
}

static void cls_paint(const painter_t *painter, const double transf_[4][4])
{
    double transf[4][4];
    double dashes = M_PI * 12 * 0.8 / 8;
    mat4_copy(transf_, transf);
    paint_2d_rect(painter, transf);
    mat4_iscale(transf, 0.8, 0.8, 1);
    paint_2d_ellipse(painter, transf, dashes, NULL);
}

static void g_paint(const painter_t *painter, const double transf_[4][4])
{
    double transf[4][4];
    mat4_copy(transf_, transf);
    mat4_iscale(transf, 0.5, 1, 1);
    paint_2d_ellipse(painter, transf, 0, NULL);
}

static void pn_paint(const painter_t *painter, const double transf_[4][4])
{
    double transf[4][4];
    mat4_copy(transf_, transf);
    paint_2d_line(painter, transf, VEC(-1, 0), VEC(-0.25, 0));
    paint_2d_line(painter, transf, VEC(+1, 0), VEC(+0.25, 0));
    paint_2d_line(painter, transf, VEC(0, -1), VEC(0, -0.25));
    paint_2d_line(painter, transf, VEC(0, +1), VEC(0, +0.25));
    mat4_iscale(transf, 0.75, 0.75, 1);
    paint_2d_ellipse(painter, transf, 0, NULL);
    mat4_iscale(transf, 1. / 3, 1. / 3, 1);
    paint_2d_ellipse(painter, transf, 0, NULL);
}

static void ism_paint(const painter_t *painter, const double transf[4][4])
{
    paint_2d_ellipse(painter, transf, 0, NULL);
}

static void bne_paint(const painter_t *painter, const double transf[4][4])
{
    paint_2d_rect(painter, transf);
}

static void glc_paint(const painter_t *painter, const double transf[4][4])
{
    paint_2d_ellipse(painter, transf, 0, NULL);
    paint_2d_line(painter, transf, VEC(-1, 0), VEC(1, 0));
    paint_2d_line(painter, transf, VEC(0, -1), VEC(0, 1));
}

int symbols_paint(const painter_t *painter_,
                  const char *id,
                  const double pos[2], double size, const double color[4],
                  double angle)
{
    int index, i;
    double uv[4][2], c[4], transf[4][4];
    painter_t painter = *painter_;
    index = get_index(id);
    if (index == -1) return -1;
    assert(index >= 0);
    for (i = 0; i < 4; i++) {
        uv[i][0] = ((index % 8) + (i % 2)) / 8.0;
        uv[i][1] = ((index / 8) + (i / 2)) / 8.0;
    }
    if (!color) {
        hex_to_rgba(ENTRIES[index].color, c);
        color = c;
    }
    if (ENTRIES[index].paint) {
        vec4_copy(color, painter.color);
        mat4_set_identity(transf);
        mat4_itranslate(transf, pos[0], pos[1], 0);
        mat4_rz(angle, transf, transf);
        mat4_iscale(transf, size / 2, size / 2, 1);
        ENTRIES[index].paint(&painter, transf);
        return 0;
    }
    return paint_texture(&painter, get_texture(), uv, pos, size, color, angle);
}
