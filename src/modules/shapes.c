/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// ### Circle

typedef struct circle {
    obj_t       obj;
    double      size[2];
    double      pos[4];
    int         frame;
    double      orientation;
    double      color[4];
    double      border_color[4];
    char        label[255];
} circle_t;

static int circle_init(obj_t *obj, json_value *args)
{
    static uint32_t count = 0;
    circle_t *circle = (circle_t *)obj;
    vec4_set(circle->pos, 0, 0, 1, 0);
    vec2_set(circle->size, 5 * DD2R, 5 * DD2R);
    circle->frame = FRAME_ICRF;
    vec4_set(circle->color, 1, 1, 1, 0.25);
    vec4_set(circle->border_color, 1, 1, 1, 1);
    obj->oid = oid_create("SHAP", count++);
    return 0;
}

static void circle_project(const projection_t *proj, int flags,
                           const double *v, double *out)
{
    circle_t *circle = proj->user;
    double theta, r, mat[3][3], p[4] = {1, 0, 0, 0}, ra, dec;

    theta = v[0] * 2 * M_PI;
    r = v[1] * circle->size[0] / 2.0;

    eraC2s(circle->pos, &ra, &dec);
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-dec, mat, mat);
    mat3_rx(circle->orientation, mat, mat);
    mat3_iscale(mat, 1.0, circle->size[1] / circle->size[0], 1.0);
    mat3_rx(-theta, mat, mat);
    mat3_rz(r, mat, mat);

    mat3_mul_vec3(mat, p, p);
    // Apply the distance if needed.
    if (circle->pos[3] != 0) {
        vec3_mul(vec3_norm(circle->pos), p, p);
        p[3] = 1.0;
    }
    vec4_copy(p, out);
}

static void circle_get_2d_ellipse(const obj_t *obj, const observer_t *obs,
                               const projection_t* proj,
                               double win_pos[2], double win_size[2],
                               double* win_angle)
{
    const circle_t *circle = (circle_t*)obj;
    double ra, de;

    painter_t tmp_painter;
    tmp_painter.obs = obs;
    tmp_painter.proj = proj;
    eraC2s(circle->pos, &ra, &de);
    painter_project_ellipse(&tmp_painter, circle->frame, ra, de, 0,
            circle->size[0], circle->size[1], win_pos, win_size, win_angle);
    win_size[0] /= 2.0;
    win_size[1] /= 2.0;
}

static int circle_render(const obj_t *obj, const painter_t *painter_)
{
    circle_t *circle = (circle_t *)obj;
    painter_t painter = *painter_;
    projection_t proj = {
        .backward   = circle_project,
        .user       = obj,
    };
    const bool selected = core->selection && obj->oid == core->selection->oid;
    int label_flags;
    double win_pos[2], win_size[2], win_angle;
    static const double white[4] = {1, 1, 1, 1};


    vec4_copy(circle->color, painter.color);
    paint_quad(&painter, circle->frame, NULL, &proj, 64);
    if (selected) {
        painter.lines_width = 2;
        vec4_copy(white, painter.color);
    } else {
        vec4_copy(circle->border_color, painter.color);
    }
    paint_quad_contour(&painter, circle->frame, &proj, 64, 4);
    circle_get_2d_ellipse(&circle->obj, painter.obs, painter.proj,
                          win_pos, win_size, &win_angle);
    areas_add_circle(core->areas, win_pos, win_size[0], obj->oid, 0);
    if (circle->label[0]) {
        label_flags = LABEL_AROUND;
        if (selected)
            label_flags |= LABEL_BOLD;
        double radius = min(win_size[0], win_size[1]) +
                fabs(cos(win_angle - M_PI_4)) *
                fabs(win_size[0] - win_size[1]);
        labels_add_3d(circle->label, circle->frame, circle->pos, true, radius,
                      13, painter.color, 0, label_flags, 0, circle->obj.oid);
    }
    return 0;
}

static int circle_update(obj_t *obj, const observer_t *obs, double dt)
{
    circle_t *circle = (circle_t*)obj;
    vec3_normalize(circle->pos, circle->pos);
    convert_frame(obs, circle->frame, FRAME_ICRF, true, circle->pos,
                  obj->pvo[0]);
    obj->pvo[0][3] = 0.0;
    assert(fabs(vec3_norm2(obj->pvo[0]) - 1.0) <= 0.000001);
    obj->vmag = 99;
    return 0;
}

static obj_klass_t circle_klass = {
    .id         = "circle",
    .size       = sizeof(circle_t),
    .init       = circle_init,
    .render     = circle_render,
    .update     = circle_update,
    .get_2d_ellipse = circle_get_2d_ellipse,
    .attributes = (attribute_t[]) {
        PROPERTY(radec),
        PROPERTY(size, TYPE_V2, MEMBER(circle_t, size)),
        PROPERTY(pos, TYPE_V4, MEMBER(circle_t, pos)),
        PROPERTY(frame, TYPE_ENUM, MEMBER(circle_t, frame)),
        PROPERTY(orientation, TYPE_ANGLE, MEMBER(circle_t, orientation)),
        PROPERTY(color, TYPE_COLOR, MEMBER(circle_t, color)),
        PROPERTY(border_color, TYPE_COLOR, MEMBER(circle_t, border_color)),
        PROPERTY(label, TYPE_STRING, MEMBER(circle_t, label)),
        {},
    },
};
OBJ_REGISTER(circle_klass)

// ### Rect

typedef struct rect {
    obj_t       obj;
    double      size[2]; // x and y size in radiant.
    double      pos[4];
    int         frame;
    double      orientation;
    double      color[4];
    double      border_color[4];
} rect_t;

static int rect_init(obj_t *obj, json_value *args)
{
    rect_t *rect = (rect_t *)obj;
    vec4_set(rect->pos, 0, 0, 1, 0);
    vec2_set(rect->size, 5 * DD2R, 5 * DD2R);
    rect->frame = FRAME_ICRF;
    vec4_set(rect->color, 1, 1, 1, 0.25);
    vec4_set(rect->border_color, 1, 1, 1, 1);
    return 0;
}

static void rect_project(const projection_t *proj, int flags,
                         const double *v, double *out)
{
    rect_t *rect = proj->user;
    double theta, phi, ra, dec;
    double mat[3][3];
    double p[4] = {1, 0, 0, 0};

    eraC2s(rect->pos, &ra, &dec);
    phi =   (0.5 - v[0]) * rect->size[0];
    theta = (v[1] - 0.5) * rect->size[1];
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-dec, mat, mat);
    mat3_rx(rect->orientation, mat, mat);
    mat3_rz(phi, mat, mat);
    mat3_ry(theta, mat, mat);
    mat3_mul_vec3(mat, p, p);
    // Apply the distance if needed.
    if (rect->pos[3] != 0) {
        vec3_mul(vec3_norm(rect->pos), p, p);
        p[3] = 1.0;
    }
    vec4_copy(p, out);
}

static int rect_render(const obj_t *obj, const painter_t *painter_)
{
    rect_t *rect = (rect_t *)obj;
    painter_t painter = *painter_;
    projection_t proj = {
        .backward   = rect_project,
        .user       = obj,
    };
    vec4_copy(rect->color, painter.color);
    paint_quad(&painter, rect->frame, NULL, &proj, 8);
    vec4_copy(rect->border_color, painter.color);
    paint_quad_contour(&painter, rect->frame, &proj, 8, 15);
    return 0;
}

static obj_klass_t rect_klass = {
    .id         = "rect",
    .size       = sizeof(rect_t),
    .init       = rect_init,
    .render     = rect_render,
    .attributes = (attribute_t[]) {
        PROPERTY(size, TYPE_V2, MEMBER(rect_t, size)),
        PROPERTY(pos, TYPE_V4, MEMBER(rect_t, pos)),
        PROPERTY(frame, TYPE_ENUM, MEMBER(rect_t, frame)),
        PROPERTY(orientation, TYPE_ANGLE, MEMBER(rect_t, orientation)),
        PROPERTY(color, TYPE_COLOR, MEMBER(circle_t, color)),
        PROPERTY(border_color, TYPE_COLOR, MEMBER(circle_t, border_color)),
        {},
    },
};
OBJ_REGISTER(rect_klass)
