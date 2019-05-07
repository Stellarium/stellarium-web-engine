/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <zlib.h> // For crc32.

/*
 * The grid lines are rendered as the sides or rectangles when we split the
 * sphere recursively.
 *
 * Since the grid lines separation is not necessarily a power of two, the
 * split recursion need to compute for each step how many splits to make.
 *
 * We pre-compute those splits into the STEPS_DEG and STEPS_HOURS arrays.
 */

typedef struct
{
    int     n;              // Number of step in the full circle.
    int     level;          // Number of iteration to reach n.
    uint8_t splits[16];     // Split decomposition at each iteration.
} step_t;

static const step_t STEPS_DEG[] = {
    {      1,  0, {                                              }}, // 360°
    {      2,  1, {2                                             }}, // 180°
    {      4,  2, {2, 2                                          }}, //  90°
    {      6,  2, {2, 3                                          }}, //  60°

    {     12,  3, {2, 2, 3                                       }}, //  30°
    {     18,  3, {2, 3, 3                                       }}, //  20°
    {     24,  4, {2, 2, 2, 3                                    }}, //  15°
    {     36,  4, {2, 2, 3, 3                                    }}, //  10°
    {     72,  5, {2, 2, 2, 3, 3                                 }}, //   5°
    {    180,  5, {2, 2, 3, 3, 5                                 }}, //   2°
    {    360,  6, {2, 2, 2, 3, 3, 5                              }}, //   1°

    {    720,  7, {2, 2, 2, 2, 3, 3, 5                           }}, //  30'
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5                           }}, //  20'
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5                        }}, //  15'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5                        }}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, //   5'
    {  10800,  9, {2, 2, 2, 2, 3, 3, 3, 5, 5                     }}, //   2'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5                  }}, //   1'

    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5               }}, //  30"
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5               }}, //  20"
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5            }}, //  15"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5            }}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5         }}, //   5"
    { 648000, 13, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5         }}, //   2"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5      }}, //   1"
};

static const step_t STEPS_HOUR[] = {
    {      1,  0, {                                              }}, // 24h
    {      2,  1, {2                                             }}, // 12h
    {      3,  1, {3                                             }}, //  8h
    {      6,  2, {2, 3                                          }}, //  4h
    {     12,  3, {2, 2, 3                                       }}, //  2h
    {     24,  4, {2, 2, 2, 3                                    }}, //  1h

    {     48,  5, {2, 2, 2, 2, 3                                 }}, // 30m
    {     72,  5, {2, 2, 2, 3, 3                                 }}, // 20m
    {     96,  6, {2, 2, 2, 2, 2, 3                              }}, // 15m
    {    144,  6, {2, 2, 2, 2, 3, 3                              }}, // 10m
    {    288,  7, {2, 2, 2, 2, 2, 3, 3                           }}, //  5m
    {    720,  7, {2, 2, 2, 2, 3, 3, 5                           }}, //  2m
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5                        }}, //  1m

    {   2880,  9, {2, 2, 2, 2, 2, 2, 3, 3, 5                     }}, // 30s
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, // 20s
    {   5760, 10, {2, 2, 2, 2, 2, 2, 2, 3, 3, 5                  }}, // 15s
    {   8640, 10, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5                  }}, // 10s
    {  17280, 11, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5               }}, //  5s
    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5               }}, //  2s
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5            }}, //  1s
};

static struct {
    const char  *name;
    const char  *id;
    uint32_t    color;
    int         frame;      // One of the FRAME_ value.
    char        format;     // 'h' for hour, 'd' for degree. 0 for no labels.
    bool        grid;       // If true render the whole grid.
} LINES[] = {
    {
        .name       = "Azimuthal",
        .id         = "azimuthal",
        .color      = 0x6c4329ff,
        .frame      = FRAME_OBSERVED,
        .format     = 'd',
        .grid       = true,
    },
    {
        .name       = "Equatorial",
        .id         = "equatorial",
        .color      = 0x2a81ad80,
        .frame      = FRAME_ICRF,
        .format     = 'h',
        .grid       = true,
    },
    {
        .name       = "Meridian",
        .id         = "meridian",
        .color      = 0x339933ff,
        .frame      = FRAME_OBSERVED,
        .grid       = false,
    },
    {
        .name       = "Ecliptic",
        .id         = "ecliptic",
        .color      = 0xb33333ff,
        .frame      = FRAME_OBSERVED, // XXX: probably need to change that.
        .grid       = false,
    },
    {
        .name       = "Equator",
        .id         = "equator_line",
        .color      = 0x2a81ad80,
        .frame      = FRAME_ICRF,
        .grid       = false,
    },
};

typedef struct lines lines_t;
struct lines {
    obj_t       obj;
};

typedef struct line line_t;
struct line {
    obj_t           obj;
    fader_t         visible;
    int             frame;      // One of FRAME_ value.
    char            format;     // 'd', 'h', or 0
    bool            grid;       // If true render the whole grid.
    double          color[4];
};

// Test if a shape in clipping coordinates is clipped or not.
static bool is_clipped(const double pos[4][4], double clip[4][4])
{
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    int i, p;
    for (p = 0; p < 6; p++) {
        for (i = 0; i < 4; i++) {
            if (    P[p][0] * clip[i][0] +
                    P[p][1] * clip[i][1] +
                    P[p][2] * clip[i][2] +
                    P[p][3] * clip[i][3] <= 0) {
                break;
            }
        }
        if (i == 4) // All the points are outside a clipping plane.
            return true;
    }

    /*
     * Special case: if all the points are behind us and none are visible
     * on screen, we assume the tile is clipped.  This fix the problem
     * that some projections as defined at the moment don't make the clipping
     * test very accurate.
     *
     * This is the same trick used in painter.c to check if an healpix tile
     * is clipped or not.
     */
    for (i = 0; i < 4; i++) {
        if (clip[i][0] >= -clip[i][3] && clip[i][0] <= +clip[i][3] &&
            clip[i][1] >= -clip[i][3] && clip[i][1] <= +clip[i][3] &&
            clip[i][2] >= -clip[i][3] && clip[i][2] <= +clip[i][3])
                return false;
        if (pos[i][2] < 0) return false;
    }
    return true;
}

static int lines_init(obj_t *obj, json_value *args)
{
    obj_t *lines = obj;
    int i;
    line_t *line;

    for (i = 0; i < ARRAY_SIZE(LINES); i++) {
        line = (line_t*)obj_create("line", LINES[i].id, lines, NULL);
        line->frame = LINES[i].frame;
        line->grid = LINES[i].grid;
        hex_to_rgba(LINES[i].color, line->color);
        line->format = LINES[i].format;
        fader_init(&line->visible, false);
    }
    return 0;
}

static int lines_update(obj_t *obj, double dt)
{
    obj_t *line;
    int ret = 0;
    DL_FOREACH(obj->children, line)
        ret |= line->klass->update(line, dt);
    return ret;
}

static int lines_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(lines_render, 0);
    obj_t *line;
    MODULE_ITER(obj, line, "line")
        line->klass->render(line, painter);
    return 0;
}

static void lines_gui(obj_t *obj, int location)
{
    int i;
    obj_t *m;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Grids")) {
        for (i = 0; i < ARRAY_SIZE(LINES); i++) {
            m = module_get_child(obj, LINES[i].id);
            gui_item(&(gui_item_t){
                    .label = LINES[i].name,
                    .obj = m,
                    .attr = "visible"});
            obj_release(m);
        }
        gui_tab_end();
    }
}

static int line_update(obj_t *obj, double dt)
{
    bool changed = false;
    line_t *line = (line_t*)obj;
    changed |= fader_update(&line->visible, dt);
    return changed ? 1 : 0;
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double az, al;
    az = v[0] * 360 * DD2R;
    al = (v[1] - 0.5) * 180 * DD2R;
    eraS2c(az, al, out);
    out[3] = 0; // Project to infinity.
}

static bool check_borders(const double a[3], const double b[3],
                          const projection_t *proj,
                          double p[2], double u[2],
                          double v[2]);

/*
 * Function: render_label
 * Render the border label
 *
 * Parameters:
 *   p      - Position of the border intersection.
 *   u      - Direction of the line.
 *   v      - Normal of the window border inward.
 *   dir    - 0: alt, 1: az
 */
static void render_label(const double p[2], const double u[2],
                         const double v[2], const double uv[2],
                         int dir, line_t *line, int step,
                         const painter_t *painter)
{
    char buff[32];
    double pos[2];
    double a, color[4], label_angle;
    char s;
    int h[4];
    double n[2];
    double bounds[4], size[2];
    const double text_size = 12;

    vec2_normalize(u, n);

    // Give up if angle with screen is too acute.
    if (fabs(vec2_dot(n, v)) < 0.25) return;

    if (vec2_dot(n, v) < 0) {
        vec2_mul(-1, u, u);
        vec2_mul(-1, n, n);
    }

    label_angle = atan2(u[1], u[0]);
    if (fabs(label_angle) > M_PI / 2) label_angle -= M_PI;

    if (dir == 0) a = mix(-90, +90 , uv[1]) * DD2R;
    else          a = mix(  0, +360, uv[0]) * DD2R;
    if (dir == 0 || line->format == 'd') {
        eraA2af(1, a, &s, h);
        if (step <= 360)
            sprintf(buff, "%c%d°", s, h[0]);
        else if (step <= 21600)
            sprintf(buff, "%c%d°%02d'", s, h[0], h[1]);
        else
            sprintf(buff, "%c%d°%02d'%02d\"", s, h[0], h[1], h[2]);
    } else {
        eraA2tf(1, a, &s, h);
        if (step <= 24)
            sprintf(buff, "%c%dh", s, h[0]);
        else  if (step <= 1440)
            sprintf(buff, "%c%dh%02d", s, h[0], h[1]);
        else
            sprintf(buff, "%c%dh%02dm%02ds", s, h[0], h[1], h[2]);
    }

    paint_text_bounds(painter, buff, p, ALIGN_CENTER | ALIGN_MIDDLE, 0,
                      text_size, bounds);
    size[0] = bounds[2] - bounds[0];
    size[1] = bounds[3] - bounds[1];

    vec2_normalize(u, n);
    double h_offset = size[0] / 2;
    if ((fabs(v[1]) < 0.001 && n[1] < 0) || fabs(v[1]) > 0.999)
        h_offset += max(0, size[1] * tan(acos(vec2_dot(n, v))));
    pos[0] = p[0] + n[0] * h_offset;
    pos[1] = p[1] + n[1] * h_offset;

    // Offset to put the text above the line
    double n3[3] = {n[0], n[1], 0};
    double up[3] = {0, 0, n[0] > 0 ? 1 : -1};
    vec3_cross(n3, up, n3);
    pos[0] += n3[0] * size[1] / 2;
    pos[1] += n3[1] * size[1] / 2;

    vec4_copy(painter->color, color);

    color[3] = 1.0;
    paint_text(painter, buff, pos, ALIGN_CENTER | ALIGN_MIDDLE, 0,
               text_size, color, label_angle);
}

/*
 * Render a grid/line, by splitting the sphere into parts until we reach
 * the resolution of the grid.
 */
static void render_recursion(
        const line_t *line, const painter_t *painter,
        int level,
        const int splits[2],
        const double mat[3][3],
        const step_t *steps[2],
        int done_mask)
{
    int i, j, dir;
    int split_az, split_al, new_splits[2];
    double new_mat[3][3], p[4], lines[4][4] = {}, u[2], v[2];
    double pos[4][4], pos_clip[4][4];
    double uv[4][2] = {{0.0, 1.0}, {1.0, 1.0}, {0.0, 0.0}, {1.0, 0.0}};
    projection_t proj_spherical = {
        .name       = "spherical",
        .backward   = spherical_project,
    };

    if (done_mask == 3) return; // Already done.

    // Compute quad corners in clipping space.
    for (i = 0; i < 4; i++) {
        mat3_mul_vec2(mat, uv[i], p);
        spherical_project(NULL, 0, p, p);
        mat4_mul_vec4(*painter->transform, p, p);
        convert_framev4(painter->obs, line->frame, FRAME_VIEW, p, p);
        vec4_copy(p, pos[i]);
        project(painter->proj, 0, 4, p, p);
        vec4_copy(p, pos_clip[i]);
    }
    // If the quad is clipped we stop the recursion.
    // We only start to test after a certain level to prevent distortion
    // error with big quads at low levels.
    if (level > 2 && is_clipped(pos, pos_clip))
        return;

    // Nothing to render yet.
    if (level < steps[0]->level && level < steps[1]->level)
        goto keep_going;
    // To get a good enough resolution of lines, don't attempt to render
    // before level 2.
    if (level < 2) goto keep_going;

    for (i = 0; i < 4; i++) mat3_mul_vec2(mat, uv[i], uv[i]);
    vec2_copy(uv[0], lines[0]);
    vec2_copy(uv[2], lines[1]);
    vec2_copy(uv[0], lines[2]);
    vec2_copy(uv[1], lines[3]);

    for (dir = 0; dir < 2; dir++) {
        if (!line->grid && dir == 1) break;
        if (done_mask & (1 << dir)) continue; // Marked as done already.
        if (splits[dir] != steps[dir]->n / (dir ? 2 : 1)) continue;
        done_mask |= (1 << dir);
        paint_lines(painter, line->frame, 2, lines + dir * 2,
                    &proj_spherical, 8, PAINTER_SKIP_DISCONTINUOUS);
        if (!line->format) continue;
        if (check_borders(pos[0], pos[2 - dir], painter->proj, p, u, v)) {
            render_label(p, u, v, uv[0], 1 - dir, line,
                         splits[dir] * (dir + 1), painter);
        }
    }

    // Nothing left to render.
    if (level >= steps[0]->level && level >= steps[1]->level) return;

keep_going:
    // Split this quad into smaller pieces.
    split_az = steps[0]->splits[level] ?: 1;
    split_al = steps[1]->splits[level + 1] ?: 1;
    new_splits[0] = splits[0] * split_az;
    new_splits[1] = splits[1] * split_al;

    for (i = 0; i < split_al; i++)
    for (j = 0; j < split_az; j++) {
        mat3_copy(mat, new_mat);
        mat3_iscale(new_mat, 1. / split_az, 1. / split_al, 1.0);
        mat3_itranslate(new_mat, j, i);
        render_recursion(line, painter, level + 1, new_splits, new_mat,
                         steps, done_mask);
    }
}

// Compute an estimation of the visible range of azimuthal angle.  If we look
// at the pole it can go up to 360°.
static double get_theta_range(const painter_t *painter, int frame)
{
    double p[4] = {0, 0, 0, 0};
    double theta, phi;
    double theta_max = -DBL_MAX, theta_min = DBL_MAX;
    int i;

    /*
     * This works by unprojection the four screen corners into the grid
     * frame and testing the maximum and minimum distance to the meridian
     * for each of them.
     */
    for (i = 0; i < 4; i++) {
        p[0] = 2 * ((i % 2) - 0.5);
        p[1] = 2 * ((i / 2) - 0.5);
        project(painter->proj, PROJ_BACKWARD, 4, p, p);
        convert_frame(painter->obs, FRAME_VIEW, frame, true, p, p);
        eraC2s(p, &theta, &phi);
        theta_max = max(theta_max, theta);
        theta_min = min(theta_min, theta);
    }
    return theta_max - theta_min;
}


// XXX: make it better.
static void get_steps(double fov, char type, int frame,
                      const painter_t *painter,
                      const step_t *steps[2])
{
    double a = fov / 8;
    int i;
    double theta_range = get_theta_range(painter, frame);
    if (type == 'd') {
        i = (int)round(1.5 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_DEG) - 1);
        if (STEPS_DEG[i].n % 4) i++;
        steps[0] = &STEPS_DEG[i];
        steps[1] = &STEPS_DEG[i];
    } else {
        i = (int)round(1.5 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_DEG) - 1);
        if (STEPS_DEG[i].n % 4) i++;
        steps[1] = &STEPS_DEG[i];
        i = (int)round(1.5 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_HOUR) - 1);
        steps[0] = &STEPS_HOUR[i];
    }
    // Make sure that we don't render more than 15 azimuthal lines.
    while (steps[0]->n > 24 && theta_range / (M_PI * 2 / steps[0]->n) > 15)
        steps[0]--;
}

static int line_render(const obj_t *obj, const painter_t *painter_)
{
    line_t *line = (line_t*)obj;
    double transform[4][4] = MAT4_IDENTITY;
    const step_t *steps[2];
    int splits[2] = {1, 1};
    double mat[3][3];
    painter_t painter = *painter_;
    mat4_set_identity(transform);

    // XXX: probably need to use enum id for the different lines/grids.
    if (strcmp(line->obj.id, "ecliptic") == 0) {
        mat3_to_mat4(core->observer->re2h, transform);
        mat4_rx(M_PI / 2, transform, transform);
    }
    if (strcmp(line->obj.id, "equator_line") == 0) {
        mat4_rx(M_PI / 2, transform, transform);
    }

    if (line->visible.value == 0.0) return 0;

    vec4_copy(line->color, painter.color);
    painter.color[3] *= line->visible.value;
    painter.transform = &transform;
    // Compute the number of divisions of the grid.
    if (line->grid) {
        get_steps(core->fov, line->format, line->frame, &painter, steps);
    } else {
        // Lines are the same as a grid with a split of 180° in one direction.
        steps[0] = &STEPS_DEG[1]; // 180°
        steps[1] = &STEPS_DEG[4]; //  20°: enough to avoid clipping errors.
    }
    mat3_set_identity(mat);
    render_recursion(line, &painter, 0, splits, mat, steps, 0);
    return 0;
}


// Check if a line interect the normalized viewport.
// If there is an intersection, `border` will be set with the border index
// from 0 to 3.
static double seg_intersect(const double a[2], const double b[2], int *border)
{
    // We use the common 'slab' algo for AABB/seg intersection.
    // With some care to make sure we never use infinite values.
    double idx, idy,
           tx1 = -DBL_MAX, tx2 = +DBL_MAX,
           ty1 = -DBL_MAX, ty2 = +DBL_MAX,
           txmin = -DBL_MAX, txmax = +DBL_MAX,
           tymin = -DBL_MAX, tymax = +DBL_MAX,
           ret = DBL_MAX, vmin, vmax;
    if (a[0] != b[0]) {
        idx = 1.0 / (b[0] - a[0]);
        tx1 = (-1 == a[0] ? -DBL_MAX : (-1 - a[0]) * idx);
        tx2 = (+1 == a[0] ?  DBL_MAX : (+1 - a[0]) * idx);
        txmin = min(tx1, tx2);
        txmax = max(tx1, tx2);
    }
    if (a[1] != b[1]) {
        idy = 1.0 / (b[1] - a[1]);
        ty1 = (-1 == a[1] ? -DBL_MAX : (-1 - a[1]) * idy);
        ty2 = (+1 == a[1] ?  DBL_MAX : (+1 - a[1]) * idy);
        tymin = min(ty1, ty2);
        tymax = max(ty1, ty2);
    }
    ret = DBL_MAX;
    if (tymin <= txmax && txmin <= tymax) {
        vmin = max(txmin, tymin);
        vmax = min(txmax, tymax);
        if (0.0 <= vmax && vmin <= 1.0) {
            ret = vmin >= 0 ? vmin : vmax;
        }
    }
    *border = (ret == tx1) ? 0 :
              (ret == tx2) ? 1 :
              (ret == ty1) ? 2 :
                             3;

    return ret;
}

static void ndc_to_win(const projection_t *proj, const double ndc[2],
                       double win[2])
{
    win[0] = (+ndc[0] + 1) / 2 * proj->window_size[0];
    win[1] = (-ndc[1] + 1) / 2 * proj->window_size[1];
}

static bool check_borders(const double a[3], const double b[3],
                          const projection_t *proj,
                          double p[2], // Window pos on the border.
                          double u[2], // Window direction of the line.
                          double v[2]) // Window Norm of the border.
{
    double pos[2][4], q;
    bool visible[2];
    int border;
    const double VS[4][2] = {{+1, 0}, {-1, 0}, {0, -1}, {0, +1}};
    visible[0] = project(proj, PROJ_TO_NDC_SPACE, 3, a, pos[0]);
    visible[1] = project(proj, PROJ_TO_NDC_SPACE, 3, b, pos[1]);
    if (visible[0] != visible[1]) {
        q = seg_intersect(pos[0], pos[1], &border);
        if (q == DBL_MAX) return false;
        vec2_mix(pos[0], pos[1], q, p);
        ndc_to_win(proj, p, p);
        ndc_to_win(proj, pos[0], pos[0]);
        ndc_to_win(proj, pos[1], pos[1]);
        vec2_sub(pos[1], pos[0], u);
        vec2_copy(VS[border], v);
        return true;
    }
    return false;
}

/*
 * Meta class declarations.
 */

static obj_klass_t line_klass = {
    .id = "line",
    .size = sizeof(line_t),
    .flags = OBJ_IN_JSON_TREE,
    .update = line_update,
    .render = line_render,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(line_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(line_klass)


static obj_klass_t lines_klass = {
    .id = "lines",
    .size = sizeof(lines_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = lines_init,
    .update = lines_update,
    .render = lines_render,
    .gui = lines_gui,
    .render_order = 40,
};
OBJ_REGISTER(lines_klass)
