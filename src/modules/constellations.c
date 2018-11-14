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
 * Type: constellation_t
 * Object representing a single constellation.
 */
typedef struct constellation {
    obj_t       obj;
    constellation_infos_t info;
    char        *name;
    int         count;
    fader_t     visible;
    obj_t       **stars;
    // Texture and associated projection matrix.
    texture_t   *img;
    double      mat[3][3];
    int         error; // Set if we couldn't parse the stars.
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
} constellations_t;


static int constellation_init(obj_t *obj, json_value *args)
{
    constellation_t *cons = (constellation_t *)obj;
    constellation_infos_t *info;

    fader_init(&cons->visible, true);

    // For the moment, since we create the constellation from within C
    // only, we pass the info as a pointer to the structure!
    info = (void*)(intptr_t)json_get_attr_i(args, "info_ptr", 0);
    if (!info) return 0;
    cons->info = *info;
    cons->name = strdup(info->name);
    cons->obj.oid = oid_create("CST ",
                            crc32(0, (void*)info->id, strlen(info->id)));
    identifiers_add("CST", info->id, cons->obj.oid, 0, "CST ", 0,
                    NULL, NULL);
    identifiers_add("NAME", info->name, cons->obj.oid, 0, "CST ", 0,
                    NULL, NULL);
    return 0;
}

// Get the list of the constellation stars.
static int constellation_create_stars(constellation_t *cons)
{
    int i, err = 0;
    char star_id[128];
    if (cons->stars) return 0;
    cons->count = cons->info.nb_lines * 2;
    cons->stars = calloc(cons->info.nb_lines * 2, sizeof(*cons->stars));
    for (i = 0; i < cons->info.nb_lines * 2; i++) {
        assert(cons->info.lines[i / 2][i % 2] != 0);
        sprintf(star_id, "HIP %d", cons->info.lines[i / 2][i % 2]);
        cons->stars[i] = obj_get(NULL, star_id, 0);
        if (!cons->stars[i]) {
            LOG_W("Cannot find cst star: %s, %s", cons->info.id, star_id);
            err = 1;
        }
    }
    return err;
}

// Still experimental.
static int parse_anchors(const char *str, double mat[3][3])
{
    double uvs[3][3], tmp[3][3];
    int i, hips[3], r;
    double pos[3][3];
    char hip_s[16];
    obj_t *star;

    sscanf(str, "%lf %lf %d %lf %lf %d %lf %lf %d",
            &uvs[0][0], &uvs[0][1], &hips[0],
            &uvs[1][0], &uvs[1][1], &hips[1],
            &uvs[2][0], &uvs[2][1], &hips[2]);
    for (i = 0; i < 3; i++) {
        uvs[i][2] = 1.0;
        sprintf(hip_s, "HIP %d", hips[i]);
        star = obj_get(NULL, hip_s, 0);
        if (!star) {
            LOG_W("Cannot find star HIP %d", hips[i]);
            return -1;
        }
        // XXX: instead we should get the star g_ra and g_dec, since they
        // shouldn't change.
        obj_update(star, core->observer, 0);
        vec3_normalize(star->pvg[0], pos[i]);
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

static json_value *constellation_set_image(
        obj_t *obj, const attribute_t *attr, const json_value *args)
{
    const char *img, *anchors, *base_path;
    constellation_t *cons = (void*)obj;

    img = json_get_attr_s(args, "img");
    anchors = json_get_attr_s(args, "anchors");
    base_path = json_get_attr_s(args, "base_path");

    if (parse_anchors(anchors, cons->mat) != 0) goto error;
    cons->img = texture_from_url(join_paths(base_path, img), TF_LAZY_LOAD);
    assert(cons->img);
    return NULL;

error:
    LOG_W("Cannot add img to constellation %s", cons->obj.id);
    return NULL;
}

static bool constellation_is_visible(const painter_t *painter,
                                     const constellation_t *con)
{
    int i;
    bool ret;
    double (*pos)[4];
    const obj_t *s;

    if (con->error) return false;
    pos = calloc(con->count, sizeof(*pos));
    for (i = 0; i < con->count; i++) {
        s = con->stars[i];
        obj_get_pos_observed(s, painter->obs, pos[i]);
        mat4_mul_vec3(painter->obs->ro2v, pos[i], pos[i]);
        project(painter->proj, PROJ_TO_NDC_SPACE, 4, pos[i], pos[i]);
    }
    ret = !is_clipped(con->count, pos);
    free(pos);
    return ret;
}

static int render_lines(const constellation_t *con, const painter_t *painter);
static int render_img(const constellation_t *con, const painter_t *painter);

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
    double pos[4] = {0, 0, 0, 0};
    int i, err;
    if (con->error) return 0;
    // Optimization: don't update invisible constellation.
    if (con->visible.value == 0 && !con->visible.target) goto end;

    err = constellation_create_stars(con);
    if (err) {
        con->error = err;
        return 0;
    }
    for (i = 0; i < con->count; i++) {
        obj_update(con->stars[i], obs, 0);
        vec3_add(pos, con->stars[i]->pvg[0], pos);
    }
    vec3_normalize(pos, pos);
    vec3_copy(pos, obj->pvg[0]);
    obj->pvg[0][3] = 0; // At infinity.
    vec4_set(obj->pvg[1], 0, 0, 0, 0);
end:
    con->visible.target = cons->show_all ||
                          (strcasecmp(obs->pointer.cst, con->info.id) == 0) ||
                          ((obj_t*)con == core->selection);
    return fader_update(&con->visible, dt * 0.1) ? 1 : 0;
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    // Rotation matrix from 1875.0 to J2000.  Computed with erfa:
    //     eraEpb2jd(1875.0, &djm0, &djm);
    //     eraPnm06a(djm0, djm, rnpb);
    const double rnpb[3][3] = {
        {0.999535, 0.027963, 0.012159},
        {-0.027962, 0.999609, -0.000209},
        {-0.012160, -0.000131, 0.999926},
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
        paint_lines(&painter, FRAME_ICRS, 2, line, &proj, 8, 2);
    }
    return 0;
}

static bool constellation_is_selected(const constellation_t *con)
{
    if (!core->selection) return false;
    return strcmp(core->selection->id, con->obj.id) == 0;
}

static int constellation_render(const obj_t *obj, const painter_t *_painter)
{
    int err;
    constellation_t *con = (const constellation_t*)obj;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    painter_t painter = *_painter, painter2;
    bool selected = constellation_is_selected(con);

    if (!selected)
        painter.color[3] *= cons->visible.value * con->visible.value;
    if (painter.color[3] == 0.0) return 0;

    err = constellation_create_stars(con);
    if (err) {
        con->error = err;
        return 0;
    }

    if (!constellation_is_visible(&painter, con))
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
    double lines_color[4];
    double pos[3] = {0, 0, 0};
    double mag[2], radius[2];

    if (painter.color[3] == 0.0) return 0;
    hex_to_rgba(0x6096C2B0, lines_color);
    painter.lines_width *= clamp(1.0 / (core->fov / (90 * DD2R)), 1.0, 16.0);
    // Refraction already taken into account from stars position.
    vec4_emul(lines_color, painter.color, painter.color);

    lines = calloc(con->count, sizeof(*lines));
    for (i = 0; i < con->count; i++) {
        convert_coordinates(painter.obs, FRAME_ICRS, FRAME_OBSERVED, 0,
                            con->stars[i]->pvg[0], lines[i]);
        lines[i][3] = 0; // To infinity.
        vec3_add(pos, lines[i], pos);
    }
    for (i = 0; i < con->count; i += 2) {
        mag[0] = core_get_observed_mag(con->stars[i + 0]->vmag);
        mag[1] = core_get_observed_mag(con->stars[i + 1]->vmag);
        core_get_point_for_mag(mag[0], &radius[0], NULL);
        core_get_point_for_mag(mag[1], &radius[1], NULL);
        // Add some space, using ad-hoc formula.
        line_truncate(&lines[i], radius[0] * 2 + 0.25 * DD2R,
                                 radius[1] * 2 + 0.25 * DD2R);
    }

    vec3_normalize(pos, pos); // Middle pos.
    paint_lines(&painter, FRAME_OBSERVED, con->count, lines, NULL, 8, 2);
    free(lines);

    if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return 0;
    mat4_mul_vec3(core->observer->ro2v, pos, pos);
    if (project(painter.proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_WINDOW_SPACE,
                2, pos, pos)) {
        labels_add(con->name, pos, 0, 16, lines_color, 0, ANCHOR_CENTER, 0);
    }
    return 0;
}

static int render_img(const constellation_t *con, const painter_t *painter)
{
    projection_t proj = {0};
    painter_t painter2 = *painter;
    if (!painter2.color[3]) return 0;
    if (!con->img || !texture_load(con->img, NULL)) return 0;

    painter2.flags |= PAINTER_ADD;
    vec3_set(painter2.color, 1, 1, 1);
    painter2.color[3] *= 0.5;
    mat3_copy(con->mat, proj.mat3);
    proj.backward = proj_backward;
    paint_quad(&painter2, FRAME_ICRS, con->img, NULL, NULL, &proj, 4);
    return 0;
}


static int constellations_init(obj_t *obj, json_value *args)
{
    constellations_t *conss = (void*)obj;
    conss->show_all = true;
    obj_add_sub(&conss->obj, "images");
    obj_add_sub(&conss->obj, "lines");
    obj_add_sub(&conss->obj, "bounds");
    fader_init(&conss->visible, true);
    fader_init(&conss->lines_visible, false);
    fader_init(&conss->images_visible, false);
    fader_init(&conss->bounds_visible, false);
    return 0;
}

static obj_t *constellations_get(const obj_t *obj, const char *id, int flags)
{
    obj_t *cons;
    OBJ_ITER(obj, cons, "constellation") {
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

    OBJ_ITER(obj, con, "constellation") {
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

    OBJ_ITER(obj, con, "constellation") {
        obj_render((obj_t*)con, painter);
    }
    return 0;
}

static obj_t *constellations_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *con;
    OBJ_ITER(obj, con, "constellation") {
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
    .attributes     = (attribute_t[]) {
        FUNCTION("set_image", .fn = constellation_set_image),
        // Default properties.
        PROPERTY("name"),
        PROPERTY("distance"),
        PROPERTY("ra"),
        PROPERTY("dec"),
        PROPERTY("alt"),
        PROPERTY("az"),
        PROPERTY("radec"),
        PROPERTY("azalt"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {}
    },
};
OBJ_REGISTER(constellation_klass)

static obj_klass_t constellations_klass = {
    .id = "constellations",
    .size = sizeof(constellations_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = constellations_init,
    .update = constellations_update,
    .render = constellations_render,
    .get    = constellations_get,
    .get_by_oid = constellations_get_by_oid,
    .render_order = 25,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b",
                 MEMBER(constellations_t, lines_visible.target),
                 .sub = "lines"),
        PROPERTY("visible", "b",
                 MEMBER(constellations_t, images_visible.target),
                 .sub = "images"),
        PROPERTY("visible", "b",
                 MEMBER(constellations_t, bounds_visible.target),
                 .sub = "bounds"),
        PROPERTY("show_all", "b", MEMBER(constellations_t, show_all)),
        {}
    },
};
OBJ_REGISTER(constellations_klass);
