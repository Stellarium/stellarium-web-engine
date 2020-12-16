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
    circle_t *circle = (circle_t *)obj;
    vec4_set(circle->pos, 0, 0, 1, 0);
    vec2_set(circle->size, 5 * DD2R, 5 * DD2R);
    circle->frame = FRAME_ICRF;
    vec4_set(circle->color, 1, 1, 1, 0.25);
    vec4_set(circle->border_color, 1, 1, 1, 1);
    return 0;
}

static void circle_project(const uv_map_t *map,
                           const double v[2], double out[4])
{
    circle_t *circle = map->user;
    double theta, r, mat[3][3], p[4] = {1, 0, 0, 0}, ra, dec;
    bool right_handed = circle->frame != FRAME_OBSERVED &&
                        circle->frame != FRAME_MOUNT;

    theta = v[0] * 2 * M_PI;
    r = v[1] * circle->size[0] / 2.0;

    eraC2s(circle->pos, &ra, &dec);
    mat3_set_identity(mat);
    mat3_rz(ra, mat, mat);
    mat3_ry(-dec, mat, mat);
    mat3_rx(circle->orientation, mat, mat);
    mat3_iscale(mat, 1.0, circle->size[1] / circle->size[0], 1.0);
    mat3_rx(theta * (right_handed ? -1 : +1), mat, mat);
    mat3_rz(r, mat, mat);

    mat3_mul_vec3(mat, p, p);
    // Apply the distance if needed.
    if (circle->pos[3] != 0) {
        vec3_mul(vec3_norm(circle->pos), p, p);
        p[3] = 1.0;
    } else {
        vec3_normalize(p, p);
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
    uv_map_t map = {
        .map  = circle_project,
        .user = obj,
    };
    const bool selected = core->selection && obj == core->selection;
    int label_effects = TEXT_FLOAT;
    double win_pos[2], win_size[2], win_angle, radius;
    const double white[4] = {1, 1, 1, 1};

    vec4_emul(painter_->color, circle->color, painter.color);
    paint_quad(&painter, circle->frame, &map, 64);
    if (selected) {
        painter.lines.width = 2;
        vec4_copy(white, painter.color);
    } else {
        vec4_emul(painter_->color, circle->border_color, painter.color);
    }
    paint_quad_contour(&painter, circle->frame, &map, 64, 4);
    circle_get_2d_ellipse(&circle->obj, painter.obs, painter.proj,
                          win_pos, win_size, &win_angle);
    areas_add_circle(core->areas, win_pos, win_size[0], obj);
    if (circle->label[0]) {
        if (selected)
            label_effects = TEXT_BOLD;
        radius = min(win_size[0], win_size[1]) +
                 fabs(cos(win_angle - M_PI_4)) *
                 fabs(win_size[0] - win_size[1]);
        labels_add_3d(circle->label, circle->frame, circle->pos, true, radius,
                      FONT_SIZE_BASE, painter.color, 0, 0,
                      label_effects, 0, &circle->obj);
    }
    return 0;
}

static int circle_get_info(const obj_t *obj, const observer_t *obs,
                           int info, void *out)
{
    double pvo[2][4];
    circle_t *circle = (circle_t*)obj;
    switch (info) {
    case INFO_PVO:
        vec3_normalize(circle->pos, pvo[0]);
        convert_frame(obs, circle->frame, FRAME_ICRF, true, pvo[0], pvo[0]);
        pvo[0][3] = 0.0;
        assert(fabs(vec3_norm2(pvo[0]) - 1.0) <= 0.000001);
        vec4_set(pvo[1], 0, 0, 0, 0);
        memcpy(out, pvo, sizeof(pvo));
        return 0;
    default:
        return 1;
    }
}

static obj_klass_t circle_klass = {
    .id         = "circle",
    .size       = sizeof(circle_t),
    .init       = circle_init,
    .render     = circle_render,
    .get_info   = circle_get_info,
    .get_2d_ellipse = circle_get_2d_ellipse,
    .attributes = (attribute_t[]) {
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
