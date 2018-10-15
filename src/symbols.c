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

// Match the list of svg files in tool/makedata.py.
// We can probably do better than that.
static const struct {
    const char  *id;
    uint32_t    color;
} ENTRIES[] = {
    {"POIN", 0x4CFF4CFF},
    {"POIN2",0x4CFF4CFF},

    {"OpC" , 0xF2E926FF},
    {"GlC" , 0xF2E926FF},
    {"G"   , 0xFF930EFF},
    {"PN"  , 0xF2E926FF},
    {"ISM" , 0xF2E926FF},
    {"BNe" , 0x89ff5fff},
    {"Cl*" , 0x89ff5fff},

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


int symbols_paint(const painter_t *painter,
                  const char *id,
                  const double pos[2], double size, const double color[4],
                  double angle)
{
    int index, i;
    double uv[4][2];
    double c[4];
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
    return paint_texture(painter, get_texture(), uv, pos, size, color, angle);
}
