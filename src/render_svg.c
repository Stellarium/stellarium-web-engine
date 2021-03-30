/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Support for svg rendering.
// Still experimental.

typedef struct {
    renderer_t  rend;
    FILE        *out;
} renderer_svg_t;

static void prepare(renderer_t *rend_, double w, double h, double scale,
                    bool cull_flipped)
{
    renderer_svg_t *rend = (void*)rend_;
    fprintf(rend->out,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n");
}

static void finish(renderer_t *rend_)
{
    renderer_svg_t *rend = (void*)rend_;
    fprintf(rend->out, "</svg>\n");
    fclose(rend->out);
}

static void points(renderer_t *rend_, const painter_t *painter,
                   int n,
                   const point_t *points)
{
    renderer_svg_t *rend = (void*)rend_;
    int i;
    double pos[4];
    point_t p;
    const double scale = 320;
    for (i = 0; i < n; i++) {
        p = points[i];
        if (!project(painter->proj, 0, pos, pos)) continue;
        pos[0] = (pos[0] + 1.0) * scale;
        pos[1] = (pos[1] + 1.0) * scale;
        fprintf(rend->out,
                "<circle cx='%f' cy='%f' r='%f' fill='black' />\n",
                pos[0], pos[1],
                tan(p.size / 2) / painter->proj->scaling[0] * scale);
    }
}

static void text(renderer_t *rend_, const painter_t *painter,
                 const char *text, const double pos[2],
                 int align, int effects, double size, const double color[4],
                 double angle, double bounds[4])
{
    double p[2];
    renderer_svg_t *rend = (void*)rend_;
    const double scale = 320;

    if (bounds) {
        // XXX: not true!
        bounds[0] = 0;
        bounds[1] = 0;
        bounds[2] = u8_len(text) * 8;
        bounds[3] = 8;
    }
    if (bounds) return;

    p[0] = (pos[0] + 1.0) * scale;
    p[1] = (pos[1] + 1.0) * scale;
    fprintf(rend->out,
            "<text x='%f' y='%f' fill='black'>%s</text>\n",
            p[0], p[1], text);
}

static void line(renderer_t           *rend_,
                 const painter_t      *painter,
                 const double         (*line)[3],
                 int size)
{
    // Not supported yet.
}

renderer_t *render_svg_create(const char *out)
{
    renderer_svg_t *rend = calloc(1, sizeof(*rend));
    rend->out = fopen(out, "w");
    assert(rend->out);

    rend->rend.prepare = prepare;
    rend->rend.finish = finish;
    rend->rend.points_2d = points;
    rend->rend.text = text;
    rend->rend.line = line;
    return &rend->rend;
}


