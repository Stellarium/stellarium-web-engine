/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

typedef struct
{
    int     n;              // Number of step in the full circle.
    int     level;          // Split level at which to render.
    uint8_t splits[16];     // Split decomposition to reach the step.
} step_t;

static const step_t STEPS_DEG[] = {
    {      1,  0, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 360°
    {      2,  1, {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 180°
    {      4,  2, {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  90°
    {      6,  2, {2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  60°

    {     12,  3, {2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  30°
    {     18,  3, {2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  20°
    {     24,  4, {2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  15°
    {     36,  4, {2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  10°
    {     72,  5, {2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   5°
    {    180,  5, {2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   2°
    {    360,  6, {2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   1°

    {    720,  7, {2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  30'
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  20'
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  15'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, //   5'
    {  10800,  9, {2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1, 1, 1}}, //   2'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1, 1}}, //   1'

    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  30"
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  20"
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  15"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1}}, //   5"
    { 648000, 13, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5, 1, 1, 1}}, //   2"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5, 1, 1}}, //   1"
};

static const step_t STEPS_HOUR[] = {
    {      1,  0, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 24h
    {      2,  1, {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 12h
    {      3,  1, {3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  8h
    {      6,  2, {2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  4h
    {     12,  3, {2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  2h
    {     24,  4, {2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  1h

    {     48,  5, {2, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 30m
    {     72,  5, {2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 20m
    {     96,  6, {2, 2, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 15m
    {    144,  6, {2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 10m
    {    288,  7, {2, 2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  5m
    {    720,  7, {2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  2m
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  1m

    {   2880,  9, {2, 2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, // 30s
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, // 20s
    {   5760, 10, {2, 2, 2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1}}, // 15s
    {   8640, 10, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1}}, // 10s
    {  17280, 11, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1}}, //  5s
    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  2s
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  1s
};

static struct {
    const char  *name;
    const char  *id;
    uint32_t    color;
    int         frame;      // One of the FRAME_ value.
    char        format;     // 'h' for hour, 'd' for degree. 0 for no labels.
} LINES[] = {
    {
        .name       = "Azimuthal",
        .id         = "azimuthal",
        .color      = 0x4c3319ff,
        .frame      = FRAME_OBSERVED,
        .format     = 'd',
    },
    {
        .name       = "Equatorial",
        .id         = "equatorial",
        .color      = 0x2a81ad80,
        .frame      = FRAME_CIRS,
        .format     = 'h',
    },
    {
        .name       = "Meridian",
        .id         = "meridian",
        .color      = 0x339933ff,
        .frame      = FRAME_OBSERVED,
    },
    {
        .name       = "Ecliptic",
        .id         = "ecliptic",
        .color      = 0xb33333ff,
        .frame      = FRAME_OBSERVED, // XXX: probably need to change that.
    },
};

typedef struct lines lines_t;
struct lines {
    obj_t       obj;
};

static int lines_init(obj_t *obj, json_value *args);
static int lines_update(obj_t *obj, const observer_t *obs, double dt);
static int lines_render(const obj_t *obj, const painter_t *painter);
static void lines_gui(obj_t *obj, int location);
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

typedef struct line line_t;
struct line {
    obj_t           obj;
    fader_t         visible;
    fader_t         lines_visible;
    int             frame;      // One of FRAME_ value.
    char            format;     // 'd', 'h', or 0
    double          color[4];
};

static int line_update(obj_t *obj, const observer_t *obs, double dt);
static int line_render(const obj_t *obj, const painter_t *painter);
static obj_klass_t line_klass = {
    .id = "line",
    .size = sizeof(line_t),
    .flags = OBJ_IN_JSON_TREE,
    .update = line_update,
    .render = line_render,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(line_t, visible.target)),
        PROPERTY("visible", "b", MEMBER(line_t, lines_visible.target),
                 .sub = "lines"),
        {}
    },
};

OBJ_REGISTER(line_klass)

static int lines_init(obj_t *obj, json_value *args)
{
    obj_t *lines = obj;
    int i;
    line_t *line;

    for (i = 0; i < ARRAY_SIZE(LINES); i++) {
        line = (line_t*)obj_create("line", LINES[i].id, lines, NULL);
        obj_add_sub(&line->obj, "lines");
        line->frame = LINES[i].frame;
        hex_to_rgba(LINES[i].color, line->color);
        line->format = LINES[i].format;
        fader_init(&line->lines_visible, true);
    }
    return 0;
}

static int lines_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *line;
    int ret = 0;
    DL_FOREACH(obj->children, line)
        ret |= line->klass->update(line, obs, dt);
    return ret;
}

static int lines_render(const obj_t *obj, const painter_t *painter)
{
    obj_t *line;
    OBJ_ITER(obj, line, &line_klass)
        line->klass->render(line, painter);
    return 0;
}

static void lines_gui(obj_t *obj, int location)
{
    if (!DEFINED(EPH_GUI)) return;
    int i;
    if (gui_tab("Grids")) {
        for (i = 0; i < ARRAY_SIZE(LINES); i++) {
            gui_item(&(gui_item_t){
                    .label = LINES[i].name,
                    .obj = obj_get(obj, LINES[i].id, 0),
                    .attr = "visible"});
        }
        gui_tab_end();
    }
}

static int line_update(obj_t *obj, const observer_t *obs, double dt)
{
    bool changed = false;
    line_t *line = (line_t*)obj;
    changed |= fader_update(&line->visible, dt);
    changed |= fader_update(&line->lines_visible, dt);
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

static bool check_borders(double a[3], double b[3],
                          const projection_t *proj,
                          double p[2], double u[2],
                          double v[2]);

static void render_label(double p[2], double u[2], double v[2], double uv[2],
                         int i, line_t *line, int step,
                         const painter_t *painter)
{
    char buff[32];
    double pos[2];
    double a, color[4];
    char s;
    int h[4];
    double n[2];

    vec2_normalize(u, n);
    if (fabs(vec2_dot(n, v)) < 0.5) return;

    if (i == 0) a = mix(-90, +90 , uv[1]) * DD2R;
    else        a = mix(  0, +360, uv[0]) * DD2R;
    if (i == 0 || line->format == 'd') {
        eraA2af(1, a, &s, h);
        if (step <= 360)
            sprintf(buff, "%c%d°", s, h[0]);
        else if (step <= 21600)
            sprintf(buff, "%c%d°%2d'", s, h[0], h[1]);
        else
            sprintf(buff, "%c%d°%2d'%2d\"", s, h[0], h[1], h[2]);
    } else {
        eraA2tf(1, a, &s, h);
        if (step <= 24)
            sprintf(buff, "%c%dh", s, h[0]);
        else  if (step <= 1440)
            sprintf(buff, "%c%dh%2d", s, h[0], h[1]);
        else
            sprintf(buff, "%c%dh%2dm%2ds", s, h[0], h[1], h[2]);
    }
    vec2_addk(p, v, 0.05, pos);
    vec4_copy(painter->color, color);
    color[3] = 1.0;
    labels_add(buff, pos, 0, 13, color, ANCHOR_FIXED, 0);
}

int on_quad(int step, qtree_node_t *node,
            const double uv[4][2], const double pos[4][4],
            const painter_t *painter,
            void *user, int s[2])
{
    double lines[4][4] = {};
    double p[2], u[2], v[2]; // For the border labels.
    projection_t *proj_spherical = ((void**)user)[0];
    line_t *line = ((void**)user)[1];
    step_t **steps = ((void**)user)[2];
    int i;

    // Compute the next split.
    if (step == 0) {
        s[0] = steps[0]->splits[node->level];
        s[1] = steps[1]->splits[node->level + 1];
        if (s[0] == 1 && s[1] == 1) {
            s[0] = s[1] = 2;
        }
        if (node->level == 0) return 1;
        return 2;
    }

    if (step == 1) { // After visibility check.
        if (node->level < min(steps[0]->level, steps[1]->level)) return 1;
        if (node->level < 2) return 1;
        return 2;
    }
    vec2_copy(uv[0], lines[0]);
    vec2_copy(uv[2], lines[1]);
    vec2_copy(uv[0], lines[2]);
    vec2_copy(uv[1], lines[3]);

    bool visible[2] = {false, false};
    assert(node->c < 3);

    for (i = 0; i < 2; i++) {
        if (node->c & (1 << i)) continue; // Do we need that?
        visible[i] = (node->level >= steps[i]->level) &&
            node->xy[i] % (1 << (node->level - steps[i]->level)) == 0;
        if (!visible[i]) continue;
        node->c |= (1 << i);
        if (line->lines_visible.value) {
            // XXX: I should use a macro to do the painter copy.
            painter_t painter2 = *painter;
            painter2.color[3] *= line->lines_visible.value;
            paint_lines(&painter2, line->frame, 2, lines + i * 2,
                        proj_spherical, 8, 0);
        }
        if (!line->format) continue;
        if (check_borders(pos[0], pos[2 - i], painter->proj, p, u, v))
                render_label(p, u, v, uv[0], 1 - i, line,
                             node->s[i] * (i + 1),
                             painter);
    }
    return (node->level >= max(steps[0]->level, steps[1]->level)) ? 0 : 1;
}

// Compute an estimation of the visible range of azimuthal angle.  If we look
// at the pole it can go up to 360°.
static double get_theta_range(const painter_t *painter)
{
    const double inf = INFINITY;
    double p[4] = {0, 0, 0, 0};
    double m[4][4];
    double theta, phi;
    double theta_max = -inf, theta_min = +inf;
    int i;

    mat4_mul(painter->obs->ro2v, painter->obs->ri2h, m);
    mat4_invert(m, m);
    for (i = 0; i < 4; i++) {
        p[0] = 2 * ((i % 2) - 0.5);
        p[1] = 2 * ((i / 2) - 0.5);
        project(painter->proj, PROJ_BACKWARD, 4, p, p);
        mat4_mul_vec3(m, p, p);
        eraC2s(p, &theta, &phi);
        theta_max = max(theta_max, theta);
        theta_min = min(theta_min, theta);
    }
    return theta_max - theta_min;
}


// XXX: make it better.
static void get_steps(double fov, char type, const painter_t *painter,
                      const step_t *steps[2])
{
    double a = fov / 8;
    int i;
    double theta_range = get_theta_range(painter);
    if (type == 'd') {
        i = (int)round(1.7 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_DEG) - 1);
        if (STEPS_DEG[i].n % 4) i++;
        steps[0] = &STEPS_DEG[i];
        steps[1] = &STEPS_DEG[i];
    } else {
        i = (int)round(1.7 * log(2 * M_PI / a));
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

static int line_render(const obj_t *obj, const painter_t *painter)
{
    line_t *line = (line_t*)obj;
    double UV[4][2] = {{0.0, 1.0}, {1.0, 1.0},
                       {0.0, 0.0}, {1.0, 0.0}};
    double transform[4][4];
    mat4_set_identity(transform);

    if (strcmp(line->obj.id, "ecliptic") == 0) {
        mat4_copy(core->observer->re2h, transform);
        mat4_rx(M_PI / 2, transform, transform);
    }

    qtree_node_t nodes[128]; // Too little?
    projection_t proj_spherical = {
        .name       = "spherical",
        .backward   = spherical_project,
    };
    painter_t painter2 = *painter;
    const step_t *steps[2];
    void *user[3] = {&proj_spherical, line, steps};

    if (line->visible.value == 0.0) return 0;

    vec4_copy(line->color, painter2.color);
    painter2.color[3] *= line->visible.value;
    painter2.transform = &transform;
    // Compute the number of divisions of the grid.
    if (line->format)
        get_steps(core->fov, line->format, &painter2, steps);
    else {
        steps[0] = &STEPS_DEG[1];
        steps[1] = &STEPS_DEG[0];
    }
    traverse_surface(nodes, ARRAY_SIZE(nodes), UV, &proj_spherical,
                     &painter2, line->frame, 1, &user[0], on_quad);
    return 0;
}


// Check if a line interect the normalized viewport.
// If there is an intersection, `border` will be set with the border index
// from 0 to 3.
static double seg_intersect(const double a[2], const double b[2], int *border)
{
    double idx = 1.0 / (b[0] - a[0]);
    double tx1 = (-1 == a[0] ? -INFINITY : (-1 - a[0]) * idx);
    double tx2 = (+1 == a[0] ?  INFINITY : (+1 - a[0]) * idx);
    double txmin = min(tx1, tx2);
    double txmax = max(tx1, tx2);
    double idy = 1.0 / (b[1] - a[1]);
    double ty1 = (-1 == a[1] ? -INFINITY : (-1 - a[1]) * idy);
    double ty2 = (+1 == a[1] ?  INFINITY : (+1 - a[1]) * idy);
    double tymin = min(ty1, ty2);
    double tymax = max(ty1, ty2);
    double ret = INFINITY;
    if (tymin <= txmax && txmin <= tymax) {
        double vmin = max(txmin, tymin);
        double vmax = min(txmax, tymax);
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

static bool check_borders(double a[3], double b[3],
                          const projection_t *proj,
                          double p[2], // NDC pos on the border.
                          double u[2], // NDC direction of the line.
                          double v[2]) // NDC Norm of the border.
{
    double pos[2][3], q;
    bool visible[2];
    int border;
    const double VS[4][2] = {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
    visible[0] = project(proj, PROJ_TO_NDC_SPACE, 3, a, pos[0]);
    visible[1] = project(proj, PROJ_TO_NDC_SPACE, 3, b, pos[1]);
    if (visible[0] != visible[1]) {
        q = seg_intersect(pos[0], pos[1], &border);
        if (q == INFINITY) return false;
        vec2_mix(pos[0], pos[1], q, p);
        vec2_sub(pos[1], pos[0], u);
        vec2_copy(VS[border], v);
        return true;
    }
    return false;
}
