/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static int constellations_init(obj_t *obj, json_value *args);
static int constellations_update(obj_t *obj, const observer_t *obs, double dt);
static int constellations_render(const obj_t *obj, const painter_t *painter);

static int constellation_init(obj_t *obj, json_value *args);
static int constellation_render(const obj_t *obj, const painter_t *painter);
static int constellation_update(obj_t *obj, const observer_t *obs, double dt);
static json_value *constellation_set_image(
        obj_t *obj, const attribute_t *attr, const json_value *args);

typedef struct constellation {
    obj_t       obj;
    constellation_infos_t info;
    char        *name;
    int         count;
    obj_t       **stars;
    // Texture and associated projection matrix.
    texture_t   *img;
    double      mat[3][3];
} constellation_t;

static obj_klass_t constellation_klass = {
    .id         = "constellation",
    .size       = sizeof(constellation_t),
    .init       = constellation_init,
    .update     = constellation_update,
    .render     = constellation_render,
    .attributes = (attribute_t[]) {
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
        PROPERTY("rise"),
        PROPERTY("set"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {}
    },
};
OBJ_REGISTER(constellation_klass)

static obj_t *constellations_get(const obj_t *obj, const char *id, int flags);

typedef struct constellations {
    obj_t       obj;
    fader_t     visible;
    fader_t     images_visible;
    fader_t     lines_visible;
    fader_t     bounds_visible;
} constellations_t;

static obj_klass_t constellations_klass = {
    .id = "constellations",
    .size = sizeof(constellations_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = constellations_init,
    .update = constellations_update,
    .render = constellations_render,
    .get    = constellations_get,
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
        {}
    },
};

static int constellation_init(obj_t *obj, json_value *args)
{
    int i;
    constellation_t *cons = (constellation_t *)obj;
    constellation_infos_t *info;
    char star_id[128];

    // For the moment, since we create the constellation from within C
    // only, we pass the info as a pointer to the structure!
    info = (void*)(intptr_t)json_get_attr_i(args, "info_ptr", 0);
    if (!info) return 0;

    cons->info = *info;
    cons->name = strdup(info->name);
    cons->count = info->nb_lines * 2;
    cons->stars = calloc(info->nb_lines * 2, sizeof(*cons->stars));
    for (i = 0; i < info->nb_lines * 2; i++) {
        assert(info->lines[i / 2][i % 2] != 0);
        sprintf(star_id, "HD %d", info->lines[i / 2][i % 2]);
        cons->stars[i] = obj_get(NULL, star_id, 0);
        if (!cons->stars[i])
            LOG_W("Cannot find cst star: %s, %s", info->id, star_id);
    }
    identifiers_add(cons->obj.id, "CST", info->id, info->id, info->id);
    identifiers_add(cons->obj.id, "NAME",
            info->name, info->name, info->name);
    return 0;
}

// Still experimental.
static int parse_anchors(const char *str, double mat[3][3])
{
    double uvs[3][3], tmp[3][3];
    int i, hds[3], r;
    double pos[3][3];
    char hd_s[16];
    obj_t *star;

    sscanf(str, "%lf %lf %d %lf %lf %d %lf %lf %d",
            &uvs[0][0], &uvs[0][1], &hds[0],
            &uvs[1][0], &uvs[1][1], &hds[1],
            &uvs[2][0], &uvs[2][1], &hds[2]);
    for (i = 0; i < 3; i++) {
        uvs[i][2] = 1.0;
        sprintf(hd_s, "HD %d", hds[i]);
        star = obj_get(NULL, hd_s, 0);
        if (!star) {
            LOG_W("Cannot find star HD %d", hds[i]);
            return -1;
        }
        // XXX: instead we should get the star g_ra and g_dec, since they
        // shouldn't change.
        obj_update(star, core->observer, 0);
        vec3_normalize(star->pos.pvg[0], pos[i]);
        obj_delete(star);
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
    cons->img = texture_from_url(join_paths(base_path, img), 0);
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

    pos = calloc(con->count, sizeof(*pos));
    for (i = 0; i < con->count; i++) {
        s = con->stars[i];
        obj_get_pos_observed(s, painter->obs, pos[i]);
        mat4_mul_vec3(painter->obs->ro2v, pos[i], pos[i]);
        project(painter->proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE,
                4, pos[i], pos[i]);
    }
    ret = !is_clipped(con->count, pos);
    free(pos);
    return ret;
}

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
    double pos[3] = {0, 0, 0};
    int i;

    for (i = 0; i < con->count; i++) {
        obj_update(con->stars[i], obs, 0);
        vec3_add(pos, con->stars[i]->pos.pvg[0], pos);
    }
    vec3_normalize(pos, pos);
    vec3_copy(pos, obj->pos.pvg[0]);
    obj->pos.pvg[0][3] = 0; // At infinity.
    vec4_set(obj->pos.pvg[1], 0, 0, 0, 0);

    // Compute radec and azalt.
    compute_coordinates(obs, obj->pos.pvg[0],
                        &obj->pos.ra, &obj->pos.dec,
                        &obj->pos.az, &obj->pos.alt);
    return 0;
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    eraS2c(v[0], v[1], out);
    out[3] = 0.0; // At infinity.
}

static int render_bounds(const constellation_t *con,
                         const painter_t *painter_)
{
    int i;
    const constellation_infos_t *info;
    double line[2][4] = {};
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    painter_t painter = *painter_;
    projection_t proj = {
        .backward = spherical_project,
    };

    painter.color[3] *= cons->bounds_visible.value;
    painter.lines_stripes = 10.0;
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


static int constellation_render(const obj_t *obj, const painter_t *painter)
{
    const constellation_t *con = (const constellation_t*)obj;
    const constellations_t *cons = (const constellations_t*)obj->parent;
    double (*lines)[4];
    double lines_color[4];
    double pos[3] = {0, 0, 0};
    double mag[2], radius[2];

    painter_t painter2 = *painter;
    int i;
    hex_to_rgba(0x6096C280, lines_color);
    painter2.lines_width = clamp(1.0 / (core->fov / (90 * DD2R)), 1.0, 16.0);
    painter2.color[3] *= cons->lines_visible.value;
    // Refraction already taken into account from stars position.
    vec4_emul(lines_color, painter2.color, painter2.color);
    if (!constellation_is_visible(&painter2, con))
        return 0;
    // XXX: we should avoid this calloc.
    lines = calloc(con->count, sizeof(*lines));
    for (i = 0; i < con->count; i++) {
        eraS2c(con->stars[i]->pos.az, con->stars[i]->pos.alt, lines[i]);
        vec3_add(pos, lines[i], pos);
    }
    for (i = 0; i < con->count; i += 2) {
        // XXX: make vmag a default attribute of all objects.
        obj_get_attr(con->stars[i + 0], "vmag", "f", &mag[0]);
        obj_get_attr(con->stars[i + 1], "vmag", "f", &mag[1]);
        mag[0] = core_get_observed_mag(mag[0]);
        mag[1] = core_get_observed_mag(mag[1]);
        core_get_point_for_mag(mag[0] - 1, &radius[0], NULL);
        core_get_point_for_mag(mag[1] - 1, &radius[1], NULL);
        line_truncate(&lines[i], radius[0], radius[1]);
    }

    vec3_normalize(pos, pos); // Middle pos.
    paint_lines(&painter2, FRAME_OBSERVED, con->count, lines, NULL, 8, 2);
    free(lines);
    render_img(con, painter);
    render_bounds(con, painter);

    if ((painter2.flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return 0;
    mat4_mul_vec3(core->observer->ro2v, pos, pos);
    if (project(painter->proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE,
                2, pos, pos)) {
        labels_add(con->name, pos, 0, 16, lines_color, ANCHOR_CENTER, 0);
    }
    return 0;
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

static int render_img(const constellation_t *con, const painter_t *painter)
{
    projection_t proj = {0};
    painter_t painter2 = *painter;
    const constellations_t *cons = (const constellations_t*)con->obj.parent;
    painter2.color[3] *= cons->images_visible.value;
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
    OBJ_ITER(obj, cons, &constellation_klass) {
        if (strcmp(cons->id, id) == 0) {
            cons->ref++; // XXX: make the object static instead?
            return cons;
        }
    }
    return NULL;
}

static int constellations_update(obj_t *obj, const observer_t *obs, double dt)
{
    bool r0, r1, r2, r3;
    obj_t *cons;
    constellations_t *constellations = (constellations_t*)obj;

    r0 = fader_update(&constellations->visible, dt);
    r1 = fader_update(&constellations->images_visible, dt);
    r2 = fader_update(&constellations->lines_visible, dt);
    r3 = fader_update(&constellations->bounds_visible, dt);

    // Skip update if not visible.
    if ((constellations->visible.value == 0.0) ||
        (constellations->images_visible.value == 0.0 &&
         constellations->lines_visible.value == 0.0 &&
         constellations->bounds_visible.value == 0.0))
        return 0;

    OBJ_ITER(obj, cons, &constellation_klass) {
        obj_update(cons, obs, dt);
    }
    return (r0 || r1 || r2 || r3) ? 1 : 0;
}

static int constellations_render(const obj_t *obj, const painter_t *painter)
{
    constellations_t *constellations = (constellations_t*)obj;
    obj_t *cons;
    if (constellations->visible.value == 0.0) return 0;
    if (constellations->lines_visible.value == 0.0 &&
        constellations->images_visible.value == 0.0 &&
        constellations->bounds_visible.value == 0.0) return 0;
    painter_t painter2 = *painter;
    painter2.color[3] *= constellations->visible.value;
    OBJ_ITER(obj, cons, &constellation_klass) {
        obj_render(cons, &painter2);
    }
    return 0;
}

OBJ_REGISTER(constellations_klass);
