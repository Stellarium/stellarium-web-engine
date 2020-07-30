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

static void glc_paint(const painter_t *painter, const double transf[3][3]);
static void opc_paint(const painter_t *painter, const double transf[3][3]);
static void cls_paint(const painter_t *painter, const double transf[3][3]);
static void   g_paint(const painter_t *painter, const double transf[3][3]);
static void  pn_paint(const painter_t *painter, const double transf[3][3]);
static void ism_paint(const painter_t *painter, const double transf[3][3]);
static void bne_paint(const painter_t *painter, const double transf[3][3]);
static void msh_paint(const painter_t *painter, const double transf[3][3]);

// Match the list of svg files in tool/makedata.py.
// We can probably do better than that.
static const struct {
    const char  *id;
    uint32_t    color;
    void        (*paint)(const painter_t *painter, const double transf[3][3]);
} ENTRIES[] = {
    [SYMBOL_ARTIFICIAL_SATELLITE]   = {"Ast",  0xff00ffff},
    [SYMBOL_OPEN_GALACTIC_CLUSTER]  = {"OpC" , 0xF2E9267F, opc_paint},
    [SYMBOL_GLOBULAR_CLUSTER]       = {"GlC" , 0xF2E9267F, glc_paint},
    [SYMBOL_GALAXY]                 = {"G"   , 0xFF930E7F,   g_paint},
    [SYMBOL_INTERACTING_GALAXIES]   = {"IG"  , 0xFF930E7F,   g_paint},
    [SYMBOL_PLANETARY_NEBULA]       = {"PN"  , 0xF2E9267F,  pn_paint},
    [SYMBOL_INTERSTELLAR_MATTER]    = {"ISM" , 0xF2E9267F, ism_paint},
    [SYMBOL_UNKNOWN]                = {"?"   , 0xF2E9267F, ism_paint},
    [SYMBOL_BRIGHT_NEBULA]          = {"BNe" , 0x89ff5f7f, bne_paint},
    [SYMBOL_CLUSTER_OF_STARS]       = {"Cl*" , 0x89ff5f7f, cls_paint},
    [SYMBOL_MULTIPLE_DEFAULT]       = {"mul" , 0x89ff5f7f, opc_paint},
    [SYMBOL_METEOR_SHOWER]          = {"MSh" , 0x89ff5f7f, msh_paint},
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

/*
 * Function: symbols_get_for_otype
 * Return the best available symbol we can use for a given object type.
 *
 * Parameters:
 *   type   - A simbad type name.
 *
 * Return:
 *   One of the <SYMBOL_ENUM> value, or zero if no symbol matches the type.
 */
int symbols_get_for_otype(const char *type)
{
    int i;
    while (true) {
        for (i = 1; i < ARRAY_SIZE(ENTRIES); i++) {
            if (strcmp(ENTRIES[i].id, type) == 0) return i;
        }
        type = otype_get_parent(type);
        if (!type) break;
    }
    return 0;
}

static void opc_paint(const painter_t *painter, const double transf[3][3])
{
    double dashes = M_PI * 12 / 8;
    paint_2d_ellipse(painter, transf, dashes, NULL, NULL, NULL);
}

static void cls_paint(const painter_t *painter, const double transf_[3][3])
{
    double transf[3][3];
    double dashes = M_PI * 12 * 0.8 / 8;
    mat3_copy(transf_, transf);
    paint_2d_rect(painter, transf, NULL, NULL);
    mat3_iscale(transf, 0.8, 0.8, 1);
    paint_2d_ellipse(painter, transf, dashes, NULL, NULL, NULL);
}

static void g_paint(const painter_t *painter, const double transf[3][3])
{
    // double transf[3][3];
    // mat4_copy(transf_, transf);
    // mat4_iscale(transf, 0.5, 1, 1);
    paint_2d_ellipse(painter, transf, 0, NULL, NULL, NULL);
}

static void pn_paint(const painter_t *painter, const double transf[3][3])
{
    paint_2d_line(painter, transf, VEC(-1.75, 0), VEC(-1, 0));
    paint_2d_line(painter, transf, VEC(+1, 0), VEC(+1.75, 0));
    paint_2d_line(painter, transf, VEC(0, -1), VEC(0, -1.75));
    paint_2d_line(painter, transf, VEC(0, +1), VEC(0, +1.75));
    paint_2d_ellipse(painter, transf, 0, NULL, NULL, NULL);
}

static void ism_paint(const painter_t *painter, const double transf[3][3])
{
    paint_2d_ellipse(painter, transf, 0, NULL, NULL, NULL);
}

static void bne_paint(const painter_t *painter, const double transf[3][3])
{
    paint_2d_rect(painter, transf, NULL, NULL);
}

static void glc_paint(const painter_t *painter, const double transf[3][3])
{
    paint_2d_ellipse(painter, transf, 0, NULL, NULL, NULL);
    paint_2d_line(painter, transf, VEC(-1, 0), VEC(1, 0));
    paint_2d_line(painter, transf, VEC(0, -1), VEC(0, 1));
}

static void msh_paint(const painter_t *painter, const double transf[3][3])
{
    int i, nb = 7;
    double a, p1[2], p2[2], r1, r2;
    unsigned short xsubi[3] = {0, 0, 3};
    for (i = 0; i < nb; i++) {
        a = i * 2 * M_PI / nb;
        a += (erand48(xsubi) - 0.5) * 15 * DD2R;
        r1 = mix(0.25, 0.3, erand48(xsubi));
        r2 = mix(0.75, 1.0, erand48(xsubi));
        p1[0] = r1 * cos(a);
        p1[1] = r1 * sin(a);
        p2[0] = r2 * cos(a);
        p2[1] = r2 * sin(a);
        paint_2d_line(painter, transf, p1, p2);
    }
}


int symbols_paint(const painter_t *painter_, int symbol,
                  const double pos[2], const double size[2],
                  const double color[4],
                  double angle)
{
    int i;
    double uv[4][2], c[4], transf[3][3];
    painter_t painter = *painter_;
    assert(symbol >= 0);
    if (!symbol) return 0;
    if (!color) {
        hex_to_rgba(ENTRIES[symbol].color, c);
        color = c;
    }
    // Procedural symbol.
    if (ENTRIES[symbol].paint) {
        vec4_copy(color, painter.color);
        mat3_set_identity(transf);
        mat3_itranslate(transf, pos[0], pos[1]);
        mat3_rz(angle, transf, transf);
        mat3_iscale(transf, size[0] / 2, size[1] / 2, 1);
        ENTRIES[symbol].paint(&painter, transf);
        return 0;
    }

    // Png symbol.
    for (i = 0; i < 4; i++) {
        uv[i][0] = (((symbol - 1) % 4) + ((3 - i) % 2)) / 4.0;
        uv[i][1] = (((symbol - 1) / 4) + (i / 2)) / 4.0;
    }
    return paint_texture(&painter, get_texture(), uv, pos, size[0],
                         color, angle);
}
