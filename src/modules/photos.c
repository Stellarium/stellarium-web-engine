/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Support embedding online photos in the sky.

typedef struct photo {
    obj_t       obj;
    texture_t   *img;
    fader_t     visible;
    // Only render the shape if set.
    // Note: we could have more control, like rendering both the pic and
    // the shape at the same time.
    bool        render_shape;

    // calibration value as returned by astrometry.net api, but with the
    // units changed to rad.
    struct {
        double orientation;
        double pixscale;
        double ra;
        double dec;
    } calibration;
    // Projection uv -> sphere.  Computed from the calibration data.
    double      mat[4][4];
} photo_t;


static json_value *photo_fn_url(obj_t *obj, const attribute_t *attr,
                                const json_value *args)
{
    photo_t *photo = (void*)obj;
    char url[1024];
    if (args->u.array.length) {
        texture_release(photo->img);
        args_get(args, TYPE_STRING, &url);
        photo->img = texture_from_url(url, 0);
    }
    if (!photo->img) return NULL;
    return args_value_new(TYPE_STRING, photo->img->url);
}

static json_value *photo_fn_calibration(obj_t *obj, const attribute_t *attr,
                                        const json_value *args)
{
    photo_t *photo = (void*)obj;
    typeof(&photo->calibration) cal = &photo->calibration;
    json_value *val;
    double orientation, pixscale, ra, dec;

    val = args->u.array.length ? args->u.array.values[0] : NULL;
    if (val) {
        orientation = json_get_attr_f(val, "orientation", 0);
        pixscale = json_get_attr_f(val, "pixscale", 0);
        ra = json_get_attr_f(val, "ra", 0);
        dec = json_get_attr_f(val, "dec", 0);
        cal->orientation = orientation * DD2R;
        cal->pixscale = pixscale / 60 / 60 * DD2R;
        cal->ra = ra * DD2R;
        cal->dec = dec * DD2R;
    }

    val = json_object_new(0);
    json_object_push(val, "orientation",
                     json_double_new(cal->orientation * DR2D));
    json_object_push(val, "pixscale",
                     json_double_new(cal->pixscale * DR2D * 60 * 60));
    json_object_push(val, "ra", json_double_new(cal->ra * DR2D));
    json_object_push(val, "dec", json_double_new(cal->dec * DR2D));
    return val;
}

// Project from uv to the sphere.
static void photo_map(const uv_map_t *map, const double v[2], double out[4])
{
    double p[4] = {v[0], v[1], 1.0, 1.0};
    vec3_normalize(p, p);
    vec4_copy(p, out);
}

static int photo_render(const obj_t *obj, const painter_t *painter)
{
    photo_t *photo = (photo_t*)obj;
    typeof(&photo->calibration) calibration = &photo->calibration;
    uv_map_t map = {};
    painter_t painter2 = *painter;

    fader_update(&photo->visible, 0.06);
    painter2.color[3] *= photo->visible.value;
    if (painter2.color[3] == 0.0) return 0;

    // We can only compute the projection matrix once we get the texture?
    if (!photo->img || !texture_load(photo->img, NULL)) return 0;

    if (photo->mat[3][3] == 0) {
        mat4_set_identity(photo->mat);
        mat4_rz(calibration->ra,  photo->mat, photo->mat);
        mat4_ry(90 * DD2R - calibration->dec, photo->mat, photo->mat);
        mat4_rz(-90 * DD2R, photo->mat, photo->mat);
        mat4_rz(calibration->orientation, photo->mat, photo->mat);
        mat4_iscale(photo->mat, calibration->pixscale * photo->img->w,
                                calibration->pixscale * photo->img->h, 1.0);
        mat4_itranslate(photo->mat, -0.5, -0.5, 0.0);
    }

    map.transf = &photo->mat;
    map.map = photo_map;

    if (!photo->render_shape) {
        painter_set_texture(&painter2, PAINTER_TEX_COLOR, photo->img, NULL);
        paint_quad(&painter2, FRAME_ICRF, &map, 4);
    } else {
        paint_quad_contour(&painter2, FRAME_ICRF, &map, 8, 15);
        painter2.color[3] *= 0.25;
        paint_quad(&painter2, FRAME_ICRF, &map, 8);
    }

    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t photo_klass = {
    .id         = "photo",
    .size       = sizeof(photo_t),
    .render     = photo_render,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(photo_t, visible.target)),
        PROPERTY(url, TYPE_STRING_PTR, .fn = photo_fn_url),
        PROPERTY(calibration, TYPE_JSON, .fn = photo_fn_calibration),
        PROPERTY(render_shape, TYPE_BOOL, MEMBER(photo_t, render_shape)),
        // Default properties.
        PROPERTY(radec),
        {}
    },
};
OBJ_REGISTER(photo_klass)
