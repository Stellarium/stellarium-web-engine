/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
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

static const step_t STEPS_RA[] = {
    {     24,  4, {2, 2, 2, 3                                    }}, //  1h
    {     72,  5, {2, 2, 2, 3, 3                                 }}, // 20m
    {    144,  6, {2, 2, 2, 2, 3, 3                              }}, // 10m
    {    288,  7, {2, 2, 2, 2, 2, 3, 3                           }}, //  5m
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5                        }}, //  1m
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, // 20s
    {   8640, 10, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5                  }}, // 10s
    {  17280, 11, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5               }}, //  5s
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5            }}, //  1s
};

static const step_t STEPS_DEC[] = {
    {     18,  3, {2, 3, 3                                       }}, //  20°
    {     36,  4, {2, 2, 3, 3                                    }}, //  10°
    {     72,  5, {2, 2, 2, 3, 3                                 }}, //   5°
    {    360,  6, {2, 2, 2, 3, 3, 5                              }}, //   1°
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5                           }}, //  20'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5                        }}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, //   5'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5                  }}, //   1'
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5               }}, //  20"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5            }}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5         }}, //   5"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5      }}, //   1"
};

static const step_t STEPS_AZ[] = {
    {     24,  4, {2, 2, 2, 3                                    }}, //  15°
    {     72,  5, {2, 2, 2, 3, 3                                 }}, //   5°
    {    360,  6, {2, 2, 2, 3, 3, 5                              }}, //   1°
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5                           }}, //  20'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5                        }}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, //   5'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5                  }}, //   1'
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5               }}, //  20"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5            }}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5         }}, //   5"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5      }}, //   1"
};

static const step_t STEPS_ALT[] = {
    {     18,  3, {2, 3, 3                                       }}, //  20°
    {     36,  4, {2, 2, 3, 3                                    }}, //  10°
    {     72,  5, {2, 2, 2, 3, 3                                 }}, //   5°
    {    360,  6, {2, 2, 2, 3, 3, 5                              }}, //   1°
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5                           }}, //  20'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5                        }}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5                     }}, //   5'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5                  }}, //   1'
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5               }}, //  20"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5            }}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5         }}, //   5"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5      }}, //   1"
};

static struct {
    const char  *name;
    const char  *id;
    uint32_t    color;
    int         frame;      // One of the FRAME_ value.
    // How to render the labels: 'h' for hour, 'd' for degree, 'n' for name.
    char        format;
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
        .color      = 0x6ab17d80,
        .frame      = FRAME_ICRF,
        .format     = 'h',
        .grid       = true,
    },
    {
        .name       = "Equatorial (of date)",
        .id         = "equatorial_jnow",
        .color      = 0x2a81ad80,
        .frame      = FRAME_JNOW,
        .format     = 'h',
        .grid       = true,
    },
    {
        .name       = "Meridian",
        .id         = "meridian",
        .color      = 0x339933ff,
        .frame      = FRAME_OBSERVED,
        .format     = 'n',
        .grid       = false,
    },
    {
        .name       = "Ecliptic",
        .id         = "ecliptic",
        .color      = 0xb33333ff,
        .frame      = FRAME_ECLIPTIC,
        .format     = 'n',
        .grid       = false,
    },
    {
        .name       = "Equator",
        .id         = "equator_line",
        .color      = 0x2a81ad80,
        .frame      = FRAME_JNOW,
        .format     = 'n',
        .grid       = false,
    },
    {
        .name       = "Boundary",
        .id         = "boundary",
        .color      = 0xffffffff,
        .frame      = FRAME_VIEW,
        .grid       = false,
    },
};

typedef struct lines lines_t;
struct lines {
    obj_t       obj;
    bool        visible;
};

typedef struct line line_t;
struct line {
    obj_t           obj;
    fader_t         visible;
    int             frame;      // One of FRAME_ value.
    char            format;     // 'd', 'h', or 0
    const char      *name;
    bool            grid;       // If true render the whole grid.
    double          color[4];
};

static void hex_to_rgba(uint32_t v, double rgba[4])
{
    rgba[0] = ((v >> 24) & 0xff) / 255.0f,
    rgba[1] = ((v >> 16) & 0xff) / 255.0f,
    rgba[2] = ((v >>  8) & 0xff) / 255.0f,
    rgba[3] = ((v >>  0) & 0xff) / 255.0f;
}

// Compute cap from a quad.
static void compute_rect_cap(const double pos[4][3], double out[4])
{
    int i;
    double d, cap[4] = {0, 0, 0, 1};

    for (i = 0; i < 4; i++) {
        vec3_add(cap, pos[i], cap);
    }
    vec3_normalize(cap, cap);
    for (i = 0; i < 4; i++) {
        d = vec3_dot(cap, pos[i]);
        if (d < cap[3])
            cap[3] = d;
    }
    vec4_copy(cap, out);
}

/*
 * Check if a position in windows coordinates is visible or not.
 */
static bool is_visible_win(const double pos[3], const double win_size[2])
{
    return pos[0] >= 0 && pos[0] < win_size[0] &&
           pos[1] >= 0 && pos[1] < win_size[1] &&
           pos[2] >= 0 && pos[2] <= 1;
}

static int lines_init(obj_t *obj, json_value *args)
{
    lines_t *lines = (void*)obj;
    int i;
    line_t *line;

    lines->visible = true;
    for (i = 0; i < ARRAY_SIZE(LINES); i++) {
        line = (line_t*)module_add_new(&lines->obj, "line", NULL);
        line->obj.id = LINES[i].id;
        line->frame = LINES[i].frame;
        line->grid = LINES[i].grid;
        hex_to_rgba(LINES[i].color, line->color);
        line->format = LINES[i].format;
        line->name = LINES[i].name;
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

static int lines_render(obj_t *obj, const painter_t *painter)
{
    const lines_t *lines = (const lines_t*)obj;
    obj_t *line;
    if (!lines->visible) return 0;
    MODULE_ITER(lines, line, "line")
        line->klass->render(line, painter);
    return 0;
}

static void lines_gui(obj_t *obj, int location)
{
    int i;
    obj_t *m;
    bool visible;

    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Grids")) {
        for (i = 0; i < ARRAY_SIZE(LINES); i++) {
            m = module_get_child(obj, LINES[i].id);
            obj_get_attr(m, "visible", &visible);
            if (gui_toggle(LINES[i].name, &visible))
                obj_set_attr(m, "visible", visible);
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

static bool get_line_screen_intersection(
        const painter_t *painter, int frame, const double line[2][4],
        const uv_map_t *map, double out_pos[2], double out_dir[2],
        double border_dir[2])
{
    double p[4], win[2][3];
    int i;
    double mid[4], line2[2][4];
    const double ws[2] = {painter->proj->window_size[0],
                          painter->proj->window_size[1]};

    for (i = 0; i < 2; i++) {
        uv_map(map, line[i], p, NULL);
        convert_frame(painter->obs, frame, FRAME_VIEW, true, p, p);
        project_to_win(painter->proj, p, win[i]);
    }
    if (    is_visible_win(win[0], painter->proj->window_size) ==
            is_visible_win(win[1], painter->proj->window_size))
        return false;

    if (vec2_dist2(win[0], win[1]) < 0.5) {
        vec2_mix(win[0], win[1], 0.5, out_pos);
        vec2_sub(win[1], win[0], out_dir);
        vec2_normalize(out_dir, out_dir);
        if (    (win[0][0] < 0 && win[1][0] >= 0) ||
                (win[1][0] < 0 && win[0][0] >= 0))
            vec2_set(border_dir, 1, 0);
        else if ((win[0][0] < ws[0] && win[1][0] >= ws[0]) ||
                 (win[1][0] < ws[0] && win[0][0] >= ws[0]))
            vec2_set(border_dir, -1, 0);
        else if ((win[0][1] < 0 && win[1][1] >= 0) ||
                 (win[1][1] < 0 && win[0][1] >= 0))
            vec2_set(border_dir, 0, 1);
        else if ((win[0][1] < ws[1] && win[1][1] >= ws[1]) ||
                 (win[1][1] < ws[1] && win[0][1] >= ws[1]))
            vec2_set(border_dir, 0, -1);
        return true;
    }

    vec2_mix(line[0], line[1], 0.5, mid);
    vec2_copy(line[0], line2[0]);
    vec2_copy(mid, line2[1]);
    if (get_line_screen_intersection(
                painter, frame, line2, map, out_pos, out_dir, border_dir))
        return true;

    vec2_copy(mid, line2[0]);
    vec2_copy(line[1], line2[1]);
    if (get_line_screen_intersection(
                painter, frame, line2, map, out_pos, out_dir, border_dir))
        return true;

    LOG_D("FAIL");
    return false;
}


static void spherical_project(
        const uv_map_t *map, const double v[2], double out[3])
{
    const double (*rot)[3][3] = map->user;
    double az, al;
    az = v[0] * 360 * DD2R;
    al = (v[1] - 0.5) * 180 * DD2R;
    vec3_from_sphe(az, al, out);
    mat3_mul_vec3(*rot, out, out);
}

/*
 * Function: render_label
 * Render the border label
 *
 * Parameters:
 *   p      - Position of the border intersection.
 *   n      - Direction of the line.
 *   v      - Normal of the window border inward.
 *   dir    - 0: alt, 1: az
 */
static void render_label(const double p[2], const double u[2],
                         const double v[2], const double uv[2],
                         int dir, const line_t *line, int step,
                         const painter_t *painter_)
{
    char buf[32];
    double pos[2];
    double n[2];
    double a, label_angle;
    char s;
    int h[4];
    double bounds[4], size[2];
    const double text_size = 12;
    painter_t painter = *painter_;

    painter.color[3] = line->visible.value;

    // Give up if angle with screen is too acute.
    if (fabs(vec2_dot(u, v)) < 0.25) return;
    // Hints the renderer that we can move the labels after the lines to
    // optimize batching.
    painter.flags |= PAINTER_ALLOW_REORDER;

    vec2_copy(u, n);
    if (vec2_dot(n, v) < 0) {
        vec2_mul(-1, n, n);
    }

    label_angle = atan2(n[1], n[0]);
    if (fabs(label_angle) > M_PI / 2) label_angle -= M_PI;

    if (dir == 0) a = mix(-90, +90 , uv[1]) * DD2R;
    else          a = mix(  0, +360, uv[0]) * DD2R;

    // Compute label according to the 'format' attribute.
    if (line->format == 'd' || (line->format == 'h' && dir == 0)) {
        if (step <= 360) {
            eraA2af(-4, a, &s, h);
            if (dir == 1 && s == '+') s = ' ';
            if (h[0] == 0) s = ' ';
            snprintf(buf, sizeof(buf), "%c%d°", s, h[0]);
        } else if (step <= 21600) {
            eraA2af(-2, a, &s, h);
            if (dir == 1 && s == '+') s = ' ';
            if (h[0] == 0 && h[1] == 0) s = ' ';
            snprintf(buf, sizeof(buf), "%c%d°%02d'", s, h[0], h[1]);
        } else {
            eraA2af(0, a, &s, h);
            if (dir == 1 && s == '+') s = ' ';
            if (h[0] == 0 && h[1] == 0 && h[2] == 0) s = ' ';
            snprintf(buf, sizeof(buf), "%c%d°%02d'%02d\"", s, h[0], h[1], h[2]);
        }
    } else if (line->format == 'h') {
        if (step <= 24) {
            eraA2tf(-4, a, &s, h);
            snprintf(buf, sizeof(buf), "%dh", h[0]);
        } else if (step <= 1440) {
            eraA2tf(-2, a, &s, h);
            snprintf(buf, sizeof(buf), "%dh%02d", h[0], h[1]);
        } else {
            eraA2tf(0, a, &s, h);
            snprintf(buf, sizeof(buf), "%dh%02dm%02ds", h[0], h[1], h[2]);
        }
    } else if (line->format == 'n') {
        snprintf(buf, sizeof(buf), "%s", sys_translate("gui", line->name));
    } else {
        assert(false);
    }

    paint_text_bounds(&painter, buf, p, ALIGN_CENTER | ALIGN_MIDDLE, 0,
                      text_size, bounds);
    size[0] = bounds[2] - bounds[0];
    size[1] = bounds[3] - bounds[1];

    double h_offset = size[0] / 2;
    if ((fabs(v[1]) < 0.001 && n[1] < 0) || fabs(v[1]) > 0.999)
        h_offset += fmax(0, size[1] * tan(acos(vec2_dot(n, v))));
    pos[0] = p[0] + n[0] * h_offset;
    pos[1] = p[1] + n[1] * h_offset;

    // Offset to put the text above the line
    double n3[3] = {n[0], n[1], 0};
    double up[3] = {0, 0, n[0] > 0 ? 1 : -1};
    vec3_cross(n3, up, n3);
    pos[0] += n3[0] * size[1] / 2;
    pos[1] += n3[1] * size[1] / 2;

    paint_text(&painter, buf, pos, NULL, ALIGN_CENTER | ALIGN_MIDDLE, 0,
               text_size, label_angle);
}

/*
 * Render a grid/line, by splitting the sphere into parts until we reach
 * the resolution of the grid.
 *
 * Parameters:
 *   splits     - Represents the size of the quad in U and V as number of
 *                splits from the full 360° circle.
 *   uv_i       - Position of the quad as split index in U and V.
 *   steps      - The target steps for the line rendering.
 */
static void render_recursion(
        const line_t *line, const painter_t *painter,
        const double rot[3][3],
        int level,
        const int splits[2],
        const int uv_i[2],
        const step_t *steps[2],
        bool skip_half)
{
    int i, j, dir;
    int split_az, split_al, new_splits[2], new_pos[2];
    double p[4], lines[4][4] = {}, u[2], v[2];
    double pos_view[4][3], cap[4];
    double uv[4][2] = {{0.0, 1.0}, {1.0, 1.0}, {0.0, 0.0}, {1.0, 0.0}};
    double mat[3][3] = MAT3_IDENTITY;
    uv_map_t map = {
        .map   = spherical_project,
        .user  = rot,
    };

    // Compute transformation matrix from full sphere uv to the quad uv.
    mat3_iscale(mat, 1. / splits[0], 1. / splits[1], 0);
    mat3_itranslate(mat, uv_i[0], uv_i[1]);

    // Compute quad corners in clipping space.
    for (i = 0; i < 4; i++) {
        mat3_mul_vec2(mat, uv[i], p);
        spherical_project(&map, p, p);
        convert_frame(painter->obs, line->frame, FRAME_VIEW, true, p, p);
        vec3_copy(p, pos_view[i]);
    }
    // If the quad is clipped we stop the recursion.
    // We only start to test after a certain level to prevent distortion
    // error with big quads at low levels.
    if (level > 2) {
        compute_rect_cap(pos_view, cap);
        if (painter_is_cap_clipped(painter, FRAME_VIEW, cap))
            return;
    }

    // Nothing to render yet.
    if (level < steps[0]->level || level < steps[1]->level)
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
        // Single line are just grid with most segments masked.
        if (!line->grid && dir == 0) continue;
        if (!line->grid && uv_i[1] != splits[1] / 2 - 1) continue;

        // Don't render last latitude, zero diameter circle at the north pole.
        if (dir == 1 && uv_i[1] == splits[1] - 1) continue;
        // Skip every other lines of Az if required.
        if (dir == 1 && skip_half && (uv_i[1] % 2)) continue;

        // Limit to 4 meridian lines around the poles.
        if (    line->grid && dir == 0 &&
                (uv_i[0] % (splits[0] / 4) != 0) &&
                (uv_i[1] == 0 || uv_i[1] == splits[1] - 1))
            continue;

        paint_line(painter, line->frame, lines + dir * 2, &map, 8, 0);
        if (!line->format) continue;
        if (get_line_screen_intersection(
                    painter, line->frame, lines + dir * 2, &map, p, u, v)) {
            render_label(p, u, v, uv[0], 1 - dir, line,
                         splits[dir] * (dir + 1), painter);
        }
    }
    return;

keep_going:
    // Split this quad into smaller pieces.
    split_az = steps[0]->splits[level] ?: 1;
    split_al = steps[1]->splits[level + 1] ?: 1;
    new_splits[0] = splits[0] * split_az;
    new_splits[1] = splits[1] * split_al;

    for (i = 0; i < split_al; i++)
    for (j = 0; j < split_az; j++) {
        new_pos[0] = uv_i[0] * split_az + j;
        new_pos[1] = uv_i[1] * split_al + i;
        render_recursion(line, painter, rot, level + 1, new_splits, new_pos,
                         steps, skip_half);
    }
}

/*
 * Compute an estimation of the visible range of azimuthal and altitude angles.
 *
 * Note: this is not really azimuth and altitude, but the phi and theta range
 * in the given frame.  I call it azalt here to make it easier to think about
 * it in this frame.
 *
 * This returns an estimation of the max angular separation of two points on
 * the screen in both direction.  So for example at the pole the azfov should
 * go up to 260°.
 */
static void get_azalt_fov(const painter_t *painter, int frame,
                          double *azfov, double *altfov)
{
    double w = painter->proj->window_size[0];
    double h = painter->proj->window_size[1];
    double p[4] = {w / 2, h / 2, 0, 0};
    double theta0, phi0, theta, phi;
    double theta_max = 0, theta_min = 0;
    double phi_max = 0, phi_min = 0;
    int i;
    const int N = 3;

    /*
     * This works by unprojection all the points of an NxN grid from the screen
     * into the frame and testing the maximum and minimum distance to the
     * central point for each of them.
     */
    unproject(painter->proj, p, p);
    convert_frame(painter->obs, FRAME_VIEW, frame, true, p, p);
    vec3_to_sphe(p, &theta0, &phi0);

    for (i = 0; i < N * N; i++) {
        p[0] = (i % N) / (double)(N - 1) * w;
        p[1] = (i / N) / (double)(N - 1) * h;
        unproject(painter->proj, p, p);
        convert_frame(painter->obs, FRAME_VIEW, frame, true, p, p);
        vec3_to_sphe(p, &theta, &phi);

        theta = eraAnpm(theta - theta0);
        theta_max = fmax(theta_max, theta);
        theta_min = fmin(theta_min, theta);

        phi = eraAnpm(phi - phi0);
        phi_max = fmax(phi_max, phi);
        phi_min = fmin(phi_min, phi);
    }
    *azfov = theta_max - theta_min;
    *altfov = phi_max - phi_min;
}

// Get the step with the closest value to a given angle.
static const step_t *steps_lookup(const step_t *steps, int size, double a)
{
    int i;

    a = 2 * M_PI / a; // Put angle in splits.
    for (i = 0; i < size - 1; i++) {
        if (steps[i].n >= a)
            break;
    }

    if ((i > 0) && fabs(a - steps[i - 1].n) < fabs(a - steps[i].n))
        return &steps[i - 1];

    return &steps[i];
}


static void get_steps(char type, int frame,
                      const painter_t *painter,
                      const step_t *steps[2])
{
    double a;   // Target angle.
    double azfov, altfov;
    const int NB_DIVS = 6;
    const double MAX_SEP = 15 * DD2R;

    get_azalt_fov(painter, frame, &azfov, &altfov);

    // First step.
    a = azfov / NB_DIVS;
    a = fmin(a, MAX_SEP);
    if (type == 'd') {
        steps[0] = steps_lookup(STEPS_AZ, ARRAY_SIZE(STEPS_AZ), a);
    } else {
        steps[0] = steps_lookup(STEPS_RA, ARRAY_SIZE(STEPS_RA), a);
    }

    // Second step.
    a = altfov / NB_DIVS;
    a = fmin(a, MAX_SEP);
    if (type == 'd') {
        steps[1] = steps_lookup(STEPS_ALT, ARRAY_SIZE(STEPS_ALT), a);
    } else {
        steps[1] = steps_lookup(STEPS_DEC, ARRAY_SIZE(STEPS_DEC), a);
    }
}

/* Mapping function that render the antimeridian line twice.
 * We add a small offset on the left and right to show it on both sides of
 * the antimeridian
 */
static void antimeridian_map(const uv_map_t *uv,
                             const double v[2], double out[4])
{
    double t = v[0];
    double r[4][4] = MAT4_IDENTITY;
    double lon = 0, lat;
    const double epsilon = 0.0001;
    lon += v[1] ? epsilon : -epsilon;
    lat = mix(-90, 90, t) * DD2R;
    vec3_from_sphe(lon, lat, out);
    out[3] = 0;
    mat4_rx(M_PI / 2, r, r);
    mat4_rz(M_PI / 2, r, r);
    mat4_mul_vec4(r, out, out);
}


// Render the projection boundary.
static int render_boundary(const painter_t *painter)
{
    const uv_map_t map = { .map = antimeridian_map };
    double lines[4][4] = {
        {0, 0}, {1, 0},
        {0, 1}, {1, 1},
    };
    if (!(painter->proj->flags & PROJ_HAS_DISCONTINUITY))
        return 0;
    paint_line(painter, FRAME_VIEW, lines + 0, &map, 64, 0);
    paint_line(painter, FRAME_VIEW, lines + 2, &map, 64, 0);
    return 0;
}


static int line_render(obj_t *obj, const painter_t *painter_)
{
    const line_t *line = (const line_t*)obj;
    double rot[3][3] = MAT3_IDENTITY;
    const step_t *steps[2];
    int splits[2] = {1, 1};
    int pos[2] = {0, 0};
    bool skip_half = false;
    painter_t painter = *painter_;

    // XXX: probably need to use enum id for the different lines/grids.
    if (strcmp(line->obj.id, "meridian") == 0) {
        mat3_rx(M_PI / 2, rot, rot);
    }

    if (line->visible.value == 0.0) return 0;

    vec4_copy(line->color, painter.color);
    painter.color[3] *= line->visible.value;

    // The boundary line has its own code.
    if (strcmp(line->obj.id, "boundary") == 0) {
        return render_boundary(&painter);
    }

    // Compute the number of divisions of the grid.
    get_steps(line->format, line->frame, &painter, steps);

    // For the Az (or Dec) step, if we are at 20°, use 10° and skip every
    // other lines (mod = 2)
    if (steps[1]->n == 18) {
        steps[1]++;
        skip_half = true;
    }

    render_recursion(line, &painter, rot, 0, splits, pos, steps, skip_half);
    return 0;
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
        PROPERTY(color, TYPE_V4, MEMBER(line_t, color)),
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
    .render_order = 34, // just before atmosphere.
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(lines_t, visible)),
        {}
    },
};
OBJ_REGISTER(lines_klass)
