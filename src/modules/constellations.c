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
 * Enum of the label display styles.
 */
enum {
    LABEL_DISPLAY_TRANSLATED    = 0,
    LABEL_DISPLAY_NATIVE        = 1,
};

/*
 * Type: anchor_t
 * An anchor point of a constellation texture.
 */
typedef struct {
    double  uv[2];
    int     hip;
} anchor_t;

/*
 * Type: constellation_t
 * Object representing a single constellation.
 */
typedef struct constellation {
    obj_t       obj;
    constellation_infos_t info;
    char        *name;
    char        *name_translated;
    char        *description;
    int         count;
    fader_t     lines_in_view;  // When the constellation is actually visible.
    fader_t     image_in_view;  // When the image is actually visible.
    fader_t     image_loaded_fader;
    obj_t       **stars;

    // Texture and associated transformation matrix.
    struct {
        texture_t   *tex;
        anchor_t    anchors[3];
        double      mat[3][3];
        double      cap[4]; // Bounding cap of the image (ICRF)
    } img;

    int         error; // Set if we couldn't parse the stars.

    double last_update; // Last update time in TT
    double (*stars_pos)[3]; // ICRF/observer pos for all stars.
    double lines_cap[4]; // Bounding cap of the lines (ICRF)
    double pvo[2][4];
} constellation_t;

/*
 * Type: constellations_t
 * The module object.
 */
typedef struct constellations {
    obj_t       obj;
    fader_t     images_visible;
    fader_t     lines_visible;
    fader_t     bounds_visible;
    fader_t     labels_visible;
    int         labels_display_style;
    bool        lines_animation;
} constellations_t;

static int constellation_update(constellation_t *con, const observer_t *obs);


// Test if a shape in clipping coordinates is clipped or not.
// Support margins.
static bool is_clipped(int n, double (*pos)[4], double mx, double my)
{
    // The six planes equations:
    const double P[6][4] = {
        {-1, 0, 0, -1 + mx}, {1, 0, 0, -1 + mx},
        {0, -1, 0, -1 + my}, {0, 1, 0, -1 + my},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    int i, p;
    for (p = 0; p < 6; p++) {
        for (i = 0; i < n; i++) {
            if (    P[p][0] * pos[i][0] +
                    P[p][1] * pos[i][1] +
                    P[p][2] * pos[i][2] +
                    P[p][3] * pos[i][3] <= 0) {
                break;
            }
        }
        if (i == n) // All the points are outside a clipping plane.
            return true;
    }
    return false;
}

/*
 * Function: join_path
 * Join two file paths.
 */
const char *join_path(const char *base, const char *path, char *buf, int len)
{
    if (!base) {
        snprintf(buf, len, "%s", path);
    } else {
        snprintf(buf, len, "%s/%s", base, path);
    }
    return buf;
}

static int constellation_init(obj_t *obj, json_value *args)
{
    constellation_t *cons = (constellation_t *)obj;
    constellation_infos_t *info;

    fader_init2(&cons->lines_in_view, false, 1);
    fader_init2(&cons->image_in_view, false, 1);
    fader_init2(&cons->image_loaded_fader, false, 1);

    // For the moment, since we create the constellation from within C
    // only, we pass the info as a pointer to the structure!
    info = (void*)(intptr_t)json_get_attr_i(args, "info_ptr", 0);
    if (!info) return 0;
    cons->info = *info;
    cons->name = strdup(info->name);
    cons->name_translated = *info->name_translated ?
        strdup(info->name_translated) : strdup(info->name);
    strcpy(cons->obj.type, "Con");
    cons->obj.oid = oid_create("CST",
                            crc32(0, (void*)info->id, strlen(info->id)));
    cons->description = info->description ? strdup(info->description) : NULL;
    return 0;
}

static int constellation_get_info(const obj_t *obj, const observer_t *obs,
                                  int info, void *out)
{
    constellation_t *con = (constellation_t*)obj;
    constellation_update(con, obs);
    switch (info) {
    case INFO_PVO:
        memcpy(out, con->pvo, sizeof(con->pvo));
        return 0;
    default:
        return 1;
    }
}

// Get the list of the constellation stars.
// Return 0 if all the stars have been loaded (even if we had errors).
static int constellation_create_stars(constellation_t *cons)
{
    int i, nb_err = 0, hip, code;
    if (cons->stars) return 0; // Already created.
    cons->count = cons->info.nb_lines * 2;
    cons->stars = calloc(cons->info.nb_lines * 2, sizeof(*cons->stars));
    for (i = 0; i < cons->info.nb_lines * 2; i++) {
        hip = cons->info.lines[i / 2][i % 2];
        assert(hip);
        cons->stars[i] = obj_get_by_hip(hip, &code);
        if (code == 0) goto still_loading;
        if (!cons->stars[i]) nb_err++;
    }
    cons->stars_pos = calloc(cons->count, sizeof(*cons->stars_pos));
    if (nb_err) {
        LOG_W("%d stars not found in constellation %s (%s)",
              nb_err, cons->info.name, cons->info.id);
    }
    return 0;

still_loading:
    // Release everything.
    for (i = 0; i < cons->info.nb_lines * 2; i++)
        obj_release(cons->stars[i]);
    free(cons->stars);
    cons->stars = NULL;
    cons->count = 0;
    return 1;
}

// Extends a cap to include a given point, without changing the cap direction.
static void cap_extends(double cap[4], double p[static 3])
{
    double n[3];
    vec3_normalize(p, n);
    cap[3] = min(cap[3], vec3_dot(cap, n));
}

// Compute the cap of an image from its 3d mat.
static void compute_image_cap(const double mat[3][3], double cap[4])
{
    int i;
    double p[4];

    // Center point (UV = [0.5, 0.5])
    mat3_mul_vec3(mat, VEC(0.5, 0.5, 1.0), p);
    vec3_normalize(p, cap);
    cap[3] = 1.0;
    for (i = 0; i < 4; i++) {
        mat3_mul_vec3(mat, VEC(i / 2, i % 2, 1.0), p);
        vec3_normalize(p, p);
        cap_extends(cap, p);
    }
}

static void update_image_mat(constellation_t *cons)
{
    int i, r, code;
    double pos[3][3];
    double uvs[3][3];
    double tmp[3][3];
    double pvo[2][4];
    obj_t *star;

    if (!cons->img.tex || cons->img.mat[2][2]) return;
    for (i = 0; i < 3; i++) {
        vec2_copy(cons->img.anchors[i].uv, uvs[i]);
        uvs[i][2] = 1.0;
        star = obj_get_by_hip(cons->img.anchors[i].hip, &code);
        if (code == 0) return; // Still loading.
        if (!star) {
            LOG_W("Cannot find star HIP %d", cons->img.anchors[i].hip);
            goto error;
        }
        // XXX: instead we should get the star g_ra and g_dec, since they
        // shouldn't change.
        obj_get_pvo(star, core->observer, pvo);
        vec3_normalize(pvo[0], pos[i]);
        obj_release(star);
    }
    // Compute the transformation matrix M from uv to ICRS:
    // M . uv = pos  =>   M = pos * inv(uv)
    r = mat3_invert(uvs, tmp);
    if (!r) goto error;
    mat3_mul(pos, tmp, cons->img.mat);
    compute_image_cap(cons->img.mat, cons->img.cap);
    return;

error:
    LOG_W("Cannot compute image for constellation %s", cons->name);
    texture_release(cons->img.tex);
    cons->img.tex = NULL;
}

// Still experimental.
static int parse_anchors(const char *str, anchor_t anchors[static 3])
{
    if (sscanf(str, "%lf %lf %d %lf %lf %d %lf %lf %d",
            &anchors[0].uv[0], &anchors[0].uv[1], &anchors[0].hip,
            &anchors[1].uv[0], &anchors[1].uv[1], &anchors[1].hip,
            &anchors[2].uv[0], &anchors[2].uv[1], &anchors[2].hip) != 9) {
        LOG_E("Cannot parse constellation anchors: %s", str);
        return -1;
    }
    return 0;
}

// Called by skyculture after we enable a new culture.
int constellation_set_image(obj_t *obj, const json_value *args)
{
    const char *img, *anchors_str, *base_path;
    constellation_t *cons = (void*)obj;
    char path[1024];

    if (cons->img.tex) return 0; // Already set.
    img = json_get_attr_s(args, "img");
    anchors_str = json_get_attr_s(args, "anchors");
    base_path = json_get_attr_s(args, "base_path");

    if (parse_anchors(anchors_str, cons->img.anchors) != 0) goto error;
    join_path(base_path, img, path, sizeof(path));
    cons->img.tex = texture_from_url(path, TF_LAZY_LOAD);
    assert(cons->img.tex);

    cons->image_loaded_fader.target = false;
    cons->image_loaded_fader.value = 0;
    return 0;

error:
    LOG_W("Cannot add img to constellation %s", cons->obj.id);
    texture_release(cons->img.tex);
    cons->img.tex = NULL;
    return -1;
}


// Make a line shorter so that we don't hide the star.
static void line_truncate(double pos[2][4], double a0, double a1)
{
    double rv[3], rv2[3];       // Rotation vector.
    double u;
    double rvm[3][3];   // Rotation matrix.
    eraPxp(pos[0], pos[1], rv);
    eraPn(rv, &u, rv);

    eraSxp(-a0, rv, rv2);
    eraRv2m(rv2, rvm);
    eraRxp(rvm, pos[0], pos[0]);

    eraSxp(+a1, rv, rv2);
    eraRv2m(rv2, rvm);
    eraRxp(rvm, pos[1], pos[1]);
}

/*
 * Function: line_animation_effect
 *
 * Apply an animation effect to a line.  The effect here is simply to grow
 * the line from one point to the other
 *
 * Parameters:
 *   pos    - The line two points positions.
 *   k      - The effect time from 0 (start) to 1 (end).
 */
static void line_animation_effect(double pos[2][4], double k)
{
    double p[3];
    k = smoothstep(0.0, 1.0, k); // Smooth transition speed.
    vec3_mix(pos[0], pos[1], k, p);
    vec3_normalize(p, pos[1]);
}

/*
 * Load the stars and the image matrix.
 * Return 0 if we are ready to render.
 */
static int constellation_update(constellation_t *con, const observer_t *obs)
{
    // The position of a constellation is its middle point.
    double pvo[2][4], pos[4] = {0, 0, 0, 0};
    int i;
    if (con->error) return -1;

    if (constellation_create_stars(con)) return 1;
    if (con->count == 0) return 1;

    if (obs->tt - con->last_update < 1.0) {
        // Constellation shape change cannot be seen over the course of
        // one day
        goto end;
    }
    con->last_update = obs->tt;

    for (i = 0; i < con->count; i++) {
        if (!con->stars[i]) continue;
        obj_get_pvo(con->stars[i], obs, pvo);
        vec3_normalize(pvo[0], con->stars_pos[i]);
        vec3_add(pos, con->stars_pos[i], pos);
    }
    if (vec3_norm2(pos) == 0) return 1; // No stars loaded yet.

    vec3_normalize(pos, pos);
    vec3_copy(pos, con->pvo[0]);
    con->pvo[0][3] = 0; // At infinity.
    vec4_set(con->pvo[1], 0, 0, 0, 0);

    // Compute bounding cap
    vec3_copy(pos, con->lines_cap);
    con->lines_cap[3] = 1.0;

    for (i = 0; i < con->count; i++) {
        if (!con->stars[i]) continue;
        cap_extends(con->lines_cap, con->stars_pos[i]);
    }

end:
    update_image_mat(con);
    return 0;
}

static void spherical_project(
        const uv_map_t *map, const double v[2], double out[4])
{
    // Rotation matrix from 1875.0 to J2000.  Computed with erfa:
    //     eraEpb2jd(1875.0, &djm0, &djm);
    //     eraPnm06a(djm0, djm, rnpb);
    const double rnpb[3][3] = {
        {0.999535020565168, 0.027962538774844, 0.012158909862936},
        {-0.027962067406873, 0.999608963139696, -0.000208799220464},
        {-0.012159993837296, -0.000131286124061, 0.999926055923052},
    };

    eraS2c(v[0], v[1], out);
    out[3] = 0.0; // At infinity.

    // Since the edges coordinates are in B1875.0, they are not exaclty
    // aligned with the meridians and parallels we need to apply the
    // rotation to J2000.
    mat3_mul_vec3(rnpb, out, out);
}

static int render_bounds(const constellation_t *con,
                         const painter_t *painter_,
                         bool selected)
{
    int i;
    const constellation_infos_t *info;
    double line[2][4] = {};
    painter_t painter = *painter_;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    uv_map_t map = {
        .map = spherical_project,
    };

    if (!selected) {
        painter.color[3] *= cons->bounds_visible.value *
                            con->lines_in_view.value;
    }
    if (!painter.color[3]) return 0;
    if (painter_is_cap_clipped(&painter, FRAME_ICRF, con->lines_cap))
        return 0;

    if (selected)
        vec4_set(painter.color, 1.0, 0.84, 0.84, 0.5 * painter.color[3]);
    else
        vec4_set(painter.color, 0.8, 0.34, 0.34, 0.2 * painter.color[3]);
    painter.lines.dash_ratio = 0.85;
    painter.lines.dash_length = 8;
    info = &con->info;
    if (!info) return 0;
    for (i = 0; i < info->nb_edges; i++) {
        memcpy(line[0], info->edges[i][0], 2 * sizeof(double));
        memcpy(line[1], info->edges[i][1], 2 * sizeof(double));
        if (line[1][0] < line[0][0]) line[1][0] += 2 * M_PI;
        paint_line(&painter, FRAME_ICRF, line, &map, 0,
                   PAINTER_SKIP_DISCONTINUOUS);
    }
    return 0;
}

/*
 * Function: constellation_lines_in_view
 * Check if the constellation is visible.
 *
 * We add a small margin on the side so that we don't render the
 * constellations when they are too close to the side of the screen.
 *
 * We need this test so that the constellation visible fader gets properly
 * updated as soon as the constellation get in and out of the screen.
 */
static bool constellation_lines_in_view(const constellation_t *con,
                                        const painter_t *painter)
{
    int i, nb = 0;
    double (*pos)[4], mx, my;
    bool ret;
    const double m = 100; // Border margins (windows unit).

    // First fast tests for the case when the constellation is not in the
    // screen at all.
    if (painter_is_cap_clipped(painter, FRAME_ICRF, con->lines_cap))
        return false;

    // Clipping test.
    pos = calloc(con->count, sizeof(*pos));
    for (i = 0; i < con->count; i++) {
        if (!con->stars[i]) continue;
        convert_frame(painter->obs, FRAME_ICRF, FRAME_VIEW, true,
                      con->stars_pos[i], pos[nb]);
        project(painter->proj, 0, pos[nb], pos[nb]);
        nb++;
    }
    if (nb == 0) {
        free(pos);
        return true;
    }
    // Compute margins in NDC.
    mx = m * painter->pixel_scale / painter->fb_size[0] * 2;
    my = m * painter->pixel_scale / painter->fb_size[1] * 2;
    mx = min(mx, 0.5);
    my = min(my, 0.5);

    ret = !is_clipped(con->count, pos, mx, my);
    free(pos);
    return ret;
}

/*
 * Function: constellation_image_in_view
 * Check if the constellation image is visible.
 *
 * We add a small margin on the side so that we don't render the
 * constellations when they are too close to the side of the screen.
 */
static bool constellation_image_in_view(const constellation_t *con,
                                        const painter_t *painter)
{
    int i;
    double pos[4][4], mx, my;
    bool ret;
    const double m = 100; // Border margins (windows unit).
    const observer_t *obs = painter->obs;

    // Check that the image is loaded and the mat computed.
    if (!con->img.tex || !texture_load(con->img.tex, NULL)) return false;
    if (!con->img.mat[2][2]) return false;

    // First fast tests for the case when the constellation is not in the
    // screen at all.
    if (painter_is_cap_clipped(painter, FRAME_ICRF, con->img.cap))
        return false;

    // Clipping test.
    for (i = 0; i < 4; i++) {
        vec4_set(pos[i], i / 2, i % 2, 1, 0);
        mat3_mul_vec3(con->img.mat, pos[i], pos[i]);
        vec3_normalize(pos[i], pos[i]);
        convert_frame(obs, FRAME_ICRF, FRAME_VIEW, true, pos[i], pos[i]);
        project(painter->proj, 0, pos[i], pos[i]);
    }

    // Compute margins in NDC.
    mx = m * painter->pixel_scale / painter->fb_size[0] * 2;
    my = m * painter->pixel_scale / painter->fb_size[1] * 2;
    mx = min(mx, 0.5);
    my = min(my, 0.5);

    ret = !is_clipped(4, pos, mx, my);
    return ret;
}

static int render_lines(constellation_t *con, const painter_t *_painter,
                        bool selected)
{
    painter_t painter = *_painter;
    int i;
    double (*lines)[4];
    double lines_color[4];
    double mag[2], radius[2], visible;
    observer_t *obs = painter.obs;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;

    visible = cons->lines_visible.value * con->lines_in_view.value;
    if (selected) {
        visible = 1;
        painter.lines.width *= 2;
    }
    if (painter.color[3] == 0.0 || visible == 0.0) return 0;
    if (painter_is_cap_clipped(&painter, FRAME_ICRF, con->lines_cap))
        return 0;

    vec4_set(lines_color, 0.65, 1.0, 1.0, 0.4);
    vec4_emul(lines_color, painter.color, painter.color);

    lines = calloc(con->count, sizeof(*lines));
    for (i = 0; i < con->count; i++) {
        if (!con->stars[i]) continue;
        vec3_copy(con->stars_pos[i], lines[i]);
        lines[i][3] = 0; // To infinity.
    }

    for (i = 0; i < con->count; i += 2) {
        if (!con->stars[i + 0] || !con->stars[i + 1]) continue;
        obj_get_info(con->stars[i + 0], obs, INFO_VMAG, &mag[0]);
        obj_get_info(con->stars[i + 1], obs, INFO_VMAG, &mag[1]);
        core_get_point_for_mag(mag[0], &radius[0], NULL);
        core_get_point_for_mag(mag[1], &radius[1], NULL);
        radius[0] = core_get_apparent_angle_for_point(painter.proj, radius[0]);
        radius[1] = core_get_apparent_angle_for_point(painter.proj, radius[1]);
        // Add some space, using ad-hoc formula.
        line_truncate(&lines[i], radius[0] * 1.0 + 0.2 * DD2R,
                                 radius[1] * 1.0 + 0.2 * DD2R);
        if (cons->lines_animation)
            line_animation_effect(&lines[i], visible * 2);
    }

    for (i = 0; i < con->count; i += 2) {
        if (!con->stars[i + 0] || !con->stars[i + 1]) continue;
        paint_line(&painter, FRAME_ICRF, lines + i, NULL, 1,
                   PAINTER_SKIP_DISCONTINUOUS);
    }

    free(lines);
    return 0;
}

// Project from uv to the sphere.
static void img_map(const uv_map_t *map, const double v[2], double out[4])
{
    double uv[3] = {v[0], v[1], 1.0};
    mat3_mul_vec3(map->mat, uv, out);
    vec3_normalize(out, out);
    out[3] = 0;
}

static int render_img(constellation_t *con, const painter_t *painter_,
                      bool selected)
{
    uv_map_t map = {0};
    painter_t painter = *painter_;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;

    if (!selected) {
        painter.color[3] *= cons->images_visible.value *
                            con->image_in_view.value;
    }
    if (!painter.color[3]) return 0;
    // Skip if not ready yet.
    if (!con->img.tex || !texture_load(con->img.tex, NULL)) return 0;
    if (!con->img.mat[2][2]) return 0;

    if (painter_is_cap_clipped(&painter, FRAME_ICRF, con->img.cap))
        return 0;

    con->image_loaded_fader.target = true;

    painter.flags |= PAINTER_ADD;
    vec3_set(painter.color, 1, 1, 1);
    painter.color[3] *= (selected ? 0.6 : 0.3) * con->image_loaded_fader.value;
    mat3_copy(con->img.mat, map.mat);
    map.map = img_map;
    painter_set_texture(&painter, PAINTER_TEX_COLOR, con->img.tex, NULL);
    paint_quad(&painter, FRAME_ICRF, &map, 4);
    return 0;
}


static int render_label(constellation_t *con, const painter_t *painter_,
                        bool selected)
{
    painter_t painter = *painter_;
    double label_pixel_length, pixel_angular_resolution;
    double win_pos[2], p[3], label_cap[4], names_color[4];
    const char *label;
    constellations_t *cons = (constellations_t*)con->obj.parent;

    if (!selected) {
        painter.color[3] *= cons->labels_visible.value *
                            con->lines_in_view.value;
    }
    if (painter.color[3] == 0.0) return 0;
    if (painter_is_cap_clipped(&painter, FRAME_ICRF, con->lines_cap))
        return 0;

    // Render label only if its center is visible
    if (painter_is_point_clipped_fast(&painter, FRAME_ICRF, con->lines_cap,
                                      true))
        return 0;

    if (!selected)
        vec4_set(names_color, 0.65, 1.0, 1.0, 0.6);
    else
        vec4_set(names_color, 1.0, 1.0, 1.0, 1.0);
    // Estimate the label bouding cap
    painter_project(&painter, FRAME_ICRF, con->lines_cap, true, false,
                    win_pos);
    win_pos[0] += 1;
    painter_unproject(&painter, FRAME_ICRF, win_pos, p);
    pixel_angular_resolution = acos(vec3_dot(con->lines_cap, p));

    label = cons->labels_display_style == LABEL_DISPLAY_NATIVE ?
                con->name : con->name_translated;
    label_pixel_length = 0.5 * FONT_SIZE_BASE * 1.4 * strlen(label);
    vec3_copy(con->lines_cap, label_cap);
    label_cap[3] = cos(label_pixel_length / 2 * pixel_angular_resolution);

    if (!cap_contains_cap(con->lines_cap, label_cap))
        return 0;

    labels_add_3d(sys_translate("skyculture", label), FRAME_ICRF,
                  con->lines_cap, true, 0, FONT_SIZE_BASE,
                  names_color, 0, ALIGN_CENTER | ALIGN_MIDDLE,
                  TEXT_UPPERCASE | TEXT_SPACED | (selected ? TEXT_BOLD : 0),
                  0, con->obj.oid);

    return 0;
}


static int constellation_render(const obj_t *obj, const painter_t *_painter)
{
    constellation_t *con = (const constellation_t*)obj;
    painter_t painter = *_painter;
    const bool selected = core->selection && obj->oid == core->selection->oid;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    assert(cons);

    if (constellation_update(con, painter.obs))
        return 0;
    if (con->error)
        return 0;

    // Early exit if nothing should be rendered.
    if (    !selected
            && cons->lines_visible.value == 0.0
            && cons->labels_visible.value == 0.0
            && cons->images_visible.value == 0.0
            && cons->bounds_visible.value == 0.0)
        return 0;

    con->lines_in_view.target = constellation_lines_in_view(con, &painter);
    con->image_in_view.target = constellation_image_in_view(con, &painter);

    render_lines(con, &painter, selected);
    render_label(con, &painter, selected);
    render_img(con, &painter, selected);
    render_bounds(con, &painter, selected);

    return 0;
}

static int constellation_render_pointer(
        const obj_t *obj, const painter_t *painter)
{
    // Do nothing: selected constellations are simply rendered with different
    // colors.
    return 0;
}

static void constellation_del(obj_t *obj)
{
    int i;
    constellation_t *con = (constellation_t*)obj;
    texture_release(con->img.tex);
    for (i = 0; i < con->count; i++) {
        obj_release(con->stars[i]);
    }
    free(con->stars);
    free(con->stars_pos);
    free(con->name);
    free(con->name_translated);
}

static void constellation_get_2d_ellipse(const obj_t *obj,
                               const observer_t *obs,
                               const projection_t* proj,
                               double win_pos[2], double win_size[2],
                               double* win_angle)
{
    const constellation_t *con = (constellation_t*)obj;
    double ra, de;

    painter_t tmp_painter;
    tmp_painter.obs = obs;
    tmp_painter.proj = proj;
    eraC2s(con->lines_cap, &ra, &de);
    double size_x = acos(con->lines_cap[3]) * 2;
    painter_project_ellipse(&tmp_painter, FRAME_ICRF, ra, de, 0,
                            size_x, size_x, win_pos, win_size, win_angle);
    win_size[0] /= 2.0;
    win_size[1] /= 2.0;
}

static void constellation_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    constellation_t *cst = (void*)obj;
    f(obj, user, "NAME", cst->name);
}

static int constellations_init(obj_t *obj, json_value *args)
{
    constellations_t *conss = (void*)obj;
    conss->lines_animation = true;
    fader_init(&conss->lines_visible, false);
    fader_init(&conss->labels_visible, false);
    fader_init(&conss->images_visible, false);
    fader_init(&conss->bounds_visible, false);
    return 0;
}

static obj_t *constellations_get(const obj_t *obj, const char *id, int flags)
{
    obj_t *cons;
    MODULE_ITER(obj, cons, "constellation") {
        if (strcmp(cons->id, id) == 0) {
            cons->ref++; // XXX: make the object static instead?
            return cons;
        }
    }
    return NULL;
}

static int constellations_update(obj_t *obj, double dt)
{
    constellation_t *con;
    constellations_t *cons = (constellations_t*)obj;

    fader_update(&cons->images_visible, dt);
    fader_update(&cons->lines_visible, dt);
    fader_update(&cons->labels_visible, dt);
    fader_update(&cons->bounds_visible, dt);

    // Skip update if not visible.
    if (cons->lines_visible.value == 0.0 &&
        cons->images_visible.value == 0.0 &&
        cons->bounds_visible.value == 0.0 &&
        cons->labels_visible.value == 0.0 &&
        (!core->selection || core->selection->parent != obj)) return 0;

    MODULE_ITER(obj, con, "constellation") {
        fader_update(&con->image_loaded_fader, dt);
        fader_update(&con->lines_in_view, dt);
        fader_update(&con->image_in_view, dt);
    }
    return 0;
}

static int constellations_render(const obj_t *obj, const painter_t *painter)
{
    constellations_t *cons = (constellations_t*)obj;
    constellation_t *con;
    if (cons->lines_visible.value == 0.0 &&
        cons->labels_visible.value == 0.0 &&
        cons->images_visible.value == 0.0 &&
        cons->bounds_visible.value == 0.0 &&
        (!core->selection || core->selection->parent != obj)) return 0;

    MODULE_ITER(obj, con, "constellation") {
        obj_render((obj_t*)con, painter);
    }
    return 0;
}

static obj_t *constellations_get_by_oid(const obj_t *obj, uint64_t oid,
                                        uint64_t hint)
{
    obj_t *con;
    MODULE_ITER(obj, con, "constellation") {
        if (con->oid == oid) {
            con->ref++;
            return con;
        }
    }
    return NULL;
}

/*
 * Meta class declarations.
 */

static obj_klass_t constellation_klass = {
    .id             = "constellation",
    .size           = sizeof(constellation_t),
    .init           = constellation_init,
    .get_info       = constellation_get_info,
    .render         = constellation_render,
    .render_pointer = constellation_render_pointer,
    .del            = constellation_del,
    .get_designations = constellation_get_designations,
    .get_2d_ellipse = constellation_get_2d_ellipse,
    .attributes = (attribute_t[]) {
        PROPERTY(description, TYPE_STRING_PTR,
                 MEMBER(constellation_t, description)),
        {}
    }
};
OBJ_REGISTER(constellation_klass)

static obj_klass_t constellations_klass = {
    .id = "constellations",
    .size = sizeof(constellations_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE | OBJ_LISTABLE,
    .init = constellations_init,
    .update = constellations_update,
    .render = constellations_render,
    .get    = constellations_get,
    .get_by_oid = constellations_get_by_oid,
    .render_order = 25,
    .attributes = (attribute_t[]) {
        PROPERTY(lines_visible, TYPE_BOOL,
                 MEMBER(constellations_t, lines_visible.target)),
        PROPERTY(labels_visible, TYPE_BOOL,
                 MEMBER(constellations_t, labels_visible.target)),
        PROPERTY(images_visible, TYPE_BOOL,
                 MEMBER(constellations_t, images_visible.target)),
        PROPERTY(bounds_visible, TYPE_BOOL,
                 MEMBER(constellations_t, bounds_visible.target)),
        PROPERTY(labels_display_style, TYPE_ENUM,
                 MEMBER(constellations_t, labels_display_style)),
        PROPERTY(lines_animation, TYPE_BOOL,
                 MEMBER(constellations_t, lines_animation)),
        {}
    },
};
OBJ_REGISTER(constellations_klass);
