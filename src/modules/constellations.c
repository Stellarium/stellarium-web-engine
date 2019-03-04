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
    int         count;
    fader_t     visible;
    fader_t     image_loaded_fader;
    obj_t       **stars;
    // Texture and associated anchors and transformation matrix.
    texture_t   *img;
    anchor_t    anchors[3];
    double      mat[3][3];
    // Set to true if the img matrix need to be rescaled to the image size.
    // This happens if we get image anchors in pixel size before we know
    // the size of the image.
    bool        img_need_rescale;

    int         error; // Set if we couldn't parse the stars.
    double last_update; // Last update time in TT
    double bounding_cap[4]; // Bounding cap in ICRS
} constellation_t;

/*
 * Type: constellations_t
 * The module object.
 */
typedef struct constellations {
    obj_t       obj;
    fader_t     visible;
    fader_t     images_visible;
    fader_t     lines_visible;
    fader_t     bounds_visible;
    bool        show_all;
    int         labels_display_style;
} constellations_t;

static int constellation_update(obj_t *obj, const observer_t *obs, double dt);

/*
 * Function: join_path
 * Join two file paths.
 */
const char *join_path(const char *base, const char *path, char *buf)
{
    if (!base) {
        sprintf(buf, "%s", path);
    } else {
        sprintf(buf, "%s/%s", base, path);
    }
    return buf;
}

static int constellation_init(obj_t *obj, json_value *args)
{
    constellation_t *cons = (constellation_t *)obj;
    constellation_infos_t *info;

    fader_init(&cons->visible, true);
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
    constellation_update(obj, core->observer, 0);
    return 0;
}

// Get the list of the constellation stars.
static int constellation_create_stars(constellation_t *cons)
{
    int i, err = 0, hip;
    uint64_t oid;
    if (cons->stars) return 0;
    cons->count = cons->info.nb_lines * 2;
    cons->stars = calloc(cons->info.nb_lines * 2, sizeof(*cons->stars));
    for (i = 0; i < cons->info.nb_lines * 2; i++) {
        hip = cons->info.lines[i / 2][i % 2];
        assert(hip);
        oid = oid_create("HIP", hip);
        cons->stars[i] = obj_get_by_oid(NULL, oid, 0);
        if (!cons->stars[i]) {
            LOG_W("Cannot find cst star: %s, HIP %d", cons->info.id, hip);
            err = 1;
        }
    }
    return err;
}

static int compute_img_mat(const anchor_t anchors[static 3], double mat[3][3])
{
    int i, r;
    uint64_t oid;
    double pos[3][3];
    double uvs[3][3];
    double tmp[3][3];
    obj_t *star;
    for (i = 0; i < 3; i++) {
        vec2_copy(anchors[i].uv, uvs[i]);
        uvs[i][2] = 1.0;
        oid = oid_create("HIP", anchors[i].hip);
        star = obj_get_by_oid(NULL, oid, 0);
        if (!star) {
            LOG_W("Cannot find star HIP %d", anchors[i].hip);
            return -1;
        }
        // XXX: instead we should get the star g_ra and g_dec, since they
        // shouldn't change.
        obj_update(star, core->observer, 0);
        vec3_normalize(star->pvo[0], pos[i]);
        obj_release(star);
    }
    // Compute the transformation matrix M from uv to ICRS:
    // M . uv = pos  =>   M = pos * inv(uv)
    r = mat3_invert(uvs, tmp);
    (void)r;
    assert(r);
    mat3_mul(pos, tmp, mat);
    return 0;
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
    const char *img, *anchors, *base_path;
    constellation_t *cons = (void*)obj;
    char buf[1024];

    img = json_get_attr_s(args, "img");
    anchors = json_get_attr_s(args, "anchors");
    base_path = json_get_attr_s(args, "base_path");

    if (parse_anchors(anchors, cons->anchors) != 0) goto error;
    cons->img = texture_from_url(join_path(base_path, img, buf), TF_LAZY_LOAD);
    if (json_get_attr_b(args, "uv_in_pixel", false))
        cons->img_need_rescale = true;
    assert(cons->img);
    // Compute the image transformation matrix
    int err = compute_img_mat(cons->anchors, cons->mat);
    if (err)
        cons->error = -1;
    cons->image_loaded_fader.target = false;
    cons->image_loaded_fader.value = 0;
    return 0;

error:
    LOG_W("Cannot add img to constellation %s", cons->obj.id);
    return -1;
}

static int render_lines(const constellation_t *con, const painter_t *painter);
static int render_img(constellation_t *con, const painter_t *painter);

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

static int constellation_update(obj_t *obj, const observer_t *obs, double dt)
{
    // The position of a constellation is its middle point.
    constellation_t *con = (constellation_t*)obj;
    constellations_t *cons = (constellations_t*)obj->parent;
    double pos[4] = {0, 0, 0, 0}, max_cosdist, d;
    int i, err;
    if (con->error) return 0;
    // Optimization: don't update invisible constellation.
    if (con->visible.value == 0 && !con->visible.target) goto end;

    err = constellation_create_stars(con);
    if (err) {
        con->error = err;
        return 0;
    }
    if (con->count == 0) return 0;

    if (obs->tt - con->last_update < 1.0) {
        // Constellation shape change cannot be seen over the course of
        // one day
        goto end;
    }
    con->last_update = obs->tt;

    for (i = 0; i < con->count; i++) {
        obj_update(con->stars[i], obs, 0);
        vec3_add(pos, con->stars[i]->pvo[0], pos);
    }
    vec3_normalize(pos, pos);
    vec3_copy(pos, obj->pvo[0]);
    obj->pvo[0][3] = 0; // At infinity.
    vec4_set(obj->pvo[1], 0, 0, 0, 0);

    // Compute bounding cap
    vec3_copy(pos, con->bounding_cap);
    max_cosdist = 1.0;
    for (i = 0; i < con->count; i++) {
        d = vec3_dot(con->bounding_cap, con->stars[i]->pvo[0]);
        max_cosdist = min(max_cosdist, d);
    }
    con->bounding_cap[3] = max_cosdist;

end:
    // Rescale the image matrix once we got the texture if the anchors
    // coordinates were in pixels.
    if (con->img_need_rescale && con->img && con->img->w) {
        assert(con->mat[2][2]);
        mat3_iscale(con->mat, con->img->w, con->img->h, 1.0);
        con->img_need_rescale = false;

        // Update bounding cap to also include image vertices
        // I don't know how to do this properly..
        // In the mean time, just add a 30% margin
        con->bounding_cap[3] = cos(acos(con->bounding_cap[3]) * 1.30);
    }

    con->visible.target = cons->show_all ||
                          (strcasecmp(obs->pointer.cst, con->info.id) == 0) ||
                          ((obj_t*)con == core->selection);
    fader_update(&con->image_loaded_fader, dt);
    return fader_update(&con->visible, dt) ? 1 : 0;
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
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
                         const painter_t *painter_)
{
    int i;
    const constellation_infos_t *info;
    double line[2][4] = {};
    painter_t painter = *painter_;
    projection_t proj = {
        .backward = spherical_project,
    };

    painter.lines_stripes = 10.0; // Why not working anymore?
    painter.color[3] *= 0.5;
    if (!painter.color[3]) return 0;
    info = &con->info;
    if (!info) return 0;
    for (i = 0; i < info->nb_edges; i++) {
        memcpy(line[0], info->edges[i][0], 2 * sizeof(double));
        memcpy(line[1], info->edges[i][1], 2 * sizeof(double));
        if (line[1][0] < line[0][0]) line[1][0] += 2 * M_PI;
        paint_lines(&painter, FRAME_ICRF, 2, line, &proj, 8, 2);
    }
    return 0;
}

static int constellation_render(const obj_t *obj, const painter_t *_painter)
{
    constellation_t *con = (const constellation_t*)obj;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    painter_t painter = *_painter, painter2;
    const bool selected = core->selection && obj->oid == core->selection->oid;

    if (con->error)
        return 0;

    if (!selected)
        painter.color[3] *= cons->visible.value * con->visible.value;
    if (painter.color[3] == 0.0) return 0;

    // Check that it's intersecting with current viewport
    if (painter_is_cap_clipped(&painter, FRAME_ICRF, con->bounding_cap, false))
        return 0;

    painter2 = painter;
    if (!selected) painter2.color[3] *= cons->lines_visible.value;
    else painter2.lines_width *= 2;
    render_lines(con, &painter2);

    painter2 = painter;
    if (!selected) painter2.color[3] *= cons->images_visible.value;
    render_img(con, &painter2);

    painter2 = painter;
    if (!selected) painter2.color[3] *= cons->bounds_visible.value;
    render_bounds(con, &painter2);

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
    texture_release(con->img);
    for (i = 0; i < con->count; i++) {
        obj_release(con->stars[i]);
    }
    free(con->stars);
    free(con->name);
    free(con->name_translated);
}

static void constellation_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    constellation_t *cst = (void*)obj;
    f(obj, user, "NAME", cst->name);
}

// Project from uv to the sphere.
static void proj_backward(const projection_t *proj, int flags,
                          const double *v, double *out)
{
    double uv[3] = {v[0], v[1], 1.0};
    mat3_mul_vec3(proj->mat3, uv, out);
    vec3_normalize(out, out);
    out[3] = 0;
}

static int render_lines(const constellation_t *con, const painter_t *_painter)
{
    painter_t painter = *_painter;
    int i;
    double (*lines)[4];
    double lines_color[4], names_color[4];
    double win_pos[2];
    double mag[2], radius[2];
    const char *label;
    double half_cap[4];
    constellations_t *cons = (constellations_t*)con->obj.parent;

    if (painter.color[3] == 0.0) return 0;
    vec4_set(lines_color, 0.2, 0.4, 0.7, 0.5);
    vec4_set(names_color, 0.2, 0.4, 0.7, 1);
    painter.lines_width = 1.0;
    vec4_emul(lines_color, painter.color, painter.color);

    lines = calloc(con->count, sizeof(*lines));
    for (i = 0; i < con->count; i++) {
        vec3_copy(con->stars[i]->pvo[0], lines[i]);
        lines[i][3] = 0; // To infinity.
    }
    for (i = 0; i < con->count; i += 2) {
        mag[0] = con->stars[i + 0]->vmag;
        mag[1] = con->stars[i + 1]->vmag;
        core_get_point_for_mag(mag[0], &radius[0], NULL);
        core_get_point_for_mag(mag[1], &radius[1], NULL);
        radius[0] = core_get_apparent_angle_for_point(painter.proj, radius[0]);
        radius[1] = core_get_apparent_angle_for_point(painter.proj, radius[1]);
        // Add some space, using ad-hoc formula.
        line_truncate(&lines[i], radius[0] * 2 + 0.25 * DD2R,
                                 radius[1] * 2 + 0.25 * DD2R);
    }

    paint_lines(&painter, FRAME_ICRF, con->count, lines, NULL, 1, 2);
    free(lines);

    // Render label only if we are not too far from the observer view
    // direction, so that we don't show too many labels when zoomed out.
    vec4_copy(painter.viewport_caps[FRAME_ICRF], half_cap);
    half_cap[3] = cos(40.0 * M_PI / 180);
    if (!cap_contains_vec3(half_cap, con->bounding_cap))
        return 0;

    if (!painter_project(&painter, FRAME_ICRF, con->bounding_cap, true, true,
                         win_pos))
        return 0;

    label = cons->labels_display_style == LABEL_DISPLAY_NATIVE ?
                con->name : con->name_translated;
    labels_add_3d(label, FRAME_ICRF, con->bounding_cap, true, 0, 13,
                  names_color, 0, ALIGN_CENTER | ALIGN_MIDDLE | LABEL_UPPERCASE,
                  0, con->obj.oid);

    return 0;
}

static int render_img(constellation_t *con, const painter_t *painter)
{
    projection_t proj = {0};
    painter_t painter2 = *painter;
    if (!painter2.color[3]) return 0;
    if (!con->img || !texture_load(con->img, NULL)) return 0;
    if (!con->mat[2][2]) return 0; // Not computed yet.

    con->image_loaded_fader.target = true;

    painter2.flags |= PAINTER_ADD;
    vec3_set(painter2.color, 1, 1, 1);
    painter2.color[3] *= 0.4 * con->image_loaded_fader.value;
    mat3_copy(con->mat, proj.mat3);
    proj.backward = proj_backward;
    painter_set_texture(&painter2, PAINTER_TEX_COLOR, con->img, NULL);
    paint_quad(&painter2, FRAME_ICRF, &proj, 4);
    return 0;
}


static int constellations_init(obj_t *obj, json_value *args)
{
    constellations_t *conss = (void*)obj;
    conss->show_all = true;
    fader_init(&conss->visible, true);
    fader_init(&conss->lines_visible, false);
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

static int constellations_update(obj_t *obj, const observer_t *obs, double dt)
{
    int ret = 0;
    constellation_t *con;
    constellations_t *cons = (constellations_t*)obj;

    if (fader_update(&cons->visible, dt)) ret = 1;
    if (fader_update(&cons->images_visible, dt)) ret = 1;
    if (fader_update(&cons->lines_visible, dt)) ret = 1;
    if (fader_update(&cons->bounds_visible, dt)) ret = 1;

    // Skip update if not visible.
    if (cons->visible.value == 0.0) return 0;
    if (cons->lines_visible.value == 0.0 &&
        cons->images_visible.value == 0.0 &&
        cons->bounds_visible.value == 0.0 &&
        (!core->selection || core->selection->parent != obj)) return 0;

    MODULE_ITER(obj, con, "constellation") {
        ret |= obj_update((obj_t*)con, obs, dt);
    }
    return ret;
}

static int constellations_render(const obj_t *obj, const painter_t *painter)
{
    constellations_t *cons = (constellations_t*)obj;
    constellation_t *con;
    if (cons->visible.value == 0.0) return 0;
    if (cons->lines_visible.value == 0.0 &&
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
    .update         = constellation_update,
    .render         = constellation_render,
    .render_pointer = constellation_render_pointer,
    .del            = constellation_del,
    .get_designations = constellation_get_designations,
    .attributes     = (attribute_t[]) {
        // Default properties.
        INFO(name),
        INFO(distance),
        INFO(radec),
        INFO(vmag),
        INFO(type),
        {}
    },
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
        PROPERTY(images_visible, TYPE_BOOL,
                 MEMBER(constellations_t, images_visible.target)),
        PROPERTY(bounds_visible, TYPE_BOOL,
                 MEMBER(constellations_t, bounds_visible.target)),
        PROPERTY(show_all, TYPE_BOOL, MEMBER(constellations_t, show_all)),
        PROPERTY(labels_display_style, TYPE_ENUM,
                 MEMBER(constellations_t, labels_display_style)),
        {}
    },
};
OBJ_REGISTER(constellations_klass);
