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

static void prepare(renderer_t *rend_, int w, int h)
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
                   int frame,
                   int n,
                   const point_t *points)
{
    renderer_svg_t *rend = (void*)rend_;
    int i;
    double pos[3];
    point_t p;
    const double scale = 320;
    for (i = 0; i < n; i++) {
        p = points[i];
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0, p.pos, p.pos);
        if (!project(painter->proj, PROJ_TO_NDC_SPACE, 2, pos, pos)) continue;
        pos[0] = (pos[0] + 1.0) * scale;
        pos[1] = (pos[1] + 1.0) * scale;
        fprintf(rend->out,
                "<circle cx='%f' cy='%f' r='%f' fill='black' />\n",
                pos[0], pos[1],
                tan(p.size / 2) / painter->proj->scaling[0] * scale);
    }
}

static void text(renderer_t *rend_, const char *text, const double pos[2],
                 double size, const double color[4], double angle,
                 int out_size[2])
{
    double p[2];
    renderer_svg_t *rend = (void*)rend_;
    const double scale = 320;

    if (out_size) {
        out_size[0] = u8_len(text) * 8;
        out_size[1] = 8;
    }
    if (!pos) return;

    p[0] = (pos[0] + 1.0) * scale;
    p[1] = (pos[1] + 1.0) * scale;
    fprintf(rend->out,
            "<text x='%f' y='%f' fill='black'>%s</text>\n",
            p[0], p[1], text);
}

static void line(renderer_t           *rend_,
                 const painter_t      *painter,
                 int                  frame,
                 double               line[2][4],
                 int                  nb_segs,
                 const projection_t   *line_proj)
{
    renderer_svg_t *rend = (void*)rend_;
    int i, n;
    double k, pos[4], (*segs)[2][2];
    bool *segs_clipped;
    const double scale = 320;

    if (nb_segs == 0) return;
    n = nb_segs * 2;
    segs = calloc(nb_segs, sizeof(*segs));
    segs_clipped = calloc(nb_segs, sizeof(*segs_clipped));
    for (i = 0; i < n; i++) {
        k = ((i / 2) + (i % 2)) / (double)nb_segs;
        vec3_mix(line[0], line[1], k, pos);
        if (line_proj)
            project(line_proj, PROJ_BACKWARD, 4, pos, pos);
        mat4_mul_vec3(*painter->transform, pos, pos);
        convert_coordinates(painter->obs, frame, FRAME_VIEW, 0, pos, pos);
        if (!project(painter->proj, PROJ_TO_NDC_SPACE, 2, pos, pos))
            segs_clipped[i / 2] = true;
        pos[0] = (pos[0] + 1.0) * scale;
        pos[1] = (pos[1] + 1.0) * scale;
        vec2_copy(pos, segs[i / 2][i % 2]);
    }
    for (i = 0; i < nb_segs; i++) {
        if (segs_clipped[i]) continue;
        fprintf(rend->out,
                "<line x1='%f' y1='%f' x2='%f' y2='%f' "
                "style='stroke:rgb(0,0,0);stroke-width:1'"
                "/>\n",
                segs[i][0][0], segs[i][0][1], segs[i][1][0], segs[i][1][1]);
    }
    free(segs);
    free(segs_clipped);
}

renderer_t *render_svg_create(const char *out)
{
    renderer_svg_t *rend = calloc(1, sizeof(*rend));
    rend->out = fopen(out, "w");
    assert(rend->out);

    rend->rend.prepare = prepare;
    rend->rend.finish = finish;
    rend->rend.points = points;
    rend->rend.text = text;
    rend->rend.line = line;
    return &rend->rend;
}


