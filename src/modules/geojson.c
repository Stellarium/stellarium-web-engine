/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include "earcut.h"
#include "geojson.h"

typedef struct feature feature_t;

typedef struct feature {
    feature_t   *next;
    double      bounding_cap[4];
    int         vertices_count;
    double      (*vertices)[3];
    int         triangles_count; // Number of triangles * 3.
    uint16_t    *triangles;
    int         lines_count; // Number of lines * 2.
    uint16_t    *lines;

    float       fill_color[4];
    float       stroke_color[4];
    float       stroke_width;
    char        *title;
    int         text_anchor;
    float       text_rotate;
    float       text_offset[2];
} feature_t;

typedef struct image {
    obj_t       obj;
    feature_t   *features;
    int         frame;
} image_t;


static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(-lonlat[0] * DD2R, lonlat[1] * DD2R, c);
}

static void c2lonlat(const double c[3], double lonlat[2])
{
    double lon, lat;
    eraC2s(c, &lon, &lat);
    lonlat[0] = -lon * DR2D;
    lonlat[1] = lat * DR2D;
}

// Should be in vec.h I guess, but we use eraSepp, so it's not conveniant.
static void create_rotation_between_vecs(
        double rot[3][3], const double a[3], const double b[3])
{
    double angle = eraSepp(a, b);
    double axis[3];
    double quat[4];
    if (angle < FLT_EPSILON) {
        mat3_set_identity(rot);
        return;
    }
    if (fabs(angle - M_PI) > FLT_EPSILON) {
        vec3_cross(a, b, axis);
    } else {
        vec3_get_ortho(a, axis);
    }
    quat_from_axis(quat, angle, axis[0], axis[1], axis[2]);
    quat_to_mat3(quat, rot);
}

// XXX: naive algo.
static void compute_bounding_cap(int size, const double (*verts)[3],
                                 double cap[4])
{
    int i;
    vec4_set(cap, 0, 0, 0, 1);
    for (i = 0; i < size; i++) {
        vec3_add(cap, verts[i], cap);
    }
    vec3_normalize(cap, cap);

    for (i = 0; i < size; i++) {
        cap[3] = min(cap[3], vec3_dot(cap, verts[i]));
    }
}

static int image_init(obj_t *obj, json_value *args)
{
    image_t *image = (void*)obj;
    image->frame = FRAME_ICRF;
    return 0;
}

static void add_geojson_feature(image_t *image,
                                const geojson_feature_t *geo_feature)
{
    feature_t *feature;
    earcut_t *earcut;
    const double (*coordinates)[2];
    int i, size, triangles_size;
    const uint16_t *triangles;
    double rot[3][3], p[3];
    double (*centered_lonlat)[2];

    feature = calloc(1, sizeof(*feature));

    vec3_copy(geo_feature->properties.fill, feature->fill_color);
    vec3_copy(geo_feature->properties.stroke, feature->stroke_color);
    feature->fill_color[3] = geo_feature->properties.fill_opacity;
    feature->stroke_color[3] = geo_feature->properties.stroke_opacity;
    feature->stroke_width = geo_feature->properties.stroke_width;
    if (geo_feature->properties.title)
        feature->title = strdup(geo_feature->properties.title);
    feature->text_anchor = geo_feature->properties.text_anchor;
    feature->text_rotate = geo_feature->properties.text_rotate;
    vec2_copy(geo_feature->properties.text_offset, feature->text_offset);

    switch (geo_feature->geometry.type) {
    case GEOJSON_LINESTRING:
        coordinates = geo_feature->geometry.linestring.coordinates;
        size = geo_feature->geometry.linestring.size;
        break;
    case GEOJSON_POLYGON:
        coordinates = geo_feature->geometry.polygon.rings[0].coordinates;
        size = geo_feature->geometry.polygon.rings[0].size;
        break;
    case GEOJSON_POINT:
        coordinates = &geo_feature->geometry.point.coordinates;
        size = 1;
        break;
    default:
        assert(false);
    }

    feature->vertices_count = size;
    feature->vertices = malloc(size * sizeof(*feature->vertices));
    for (i = 0; i < size; i++) {
        lonlat2c(coordinates[i], feature->vertices[i]);
    }
    compute_bounding_cap(size, feature->vertices, feature->bounding_cap);

    // Generates contour index.
    feature->lines_count = size * 2;
    feature->lines = malloc(size * 2 * 2);
    for (i = 0; i < size - 1; i++) {
        feature->lines[i * 2 + 0] = i;
        feature->lines[i * 2 + 1] = i + 1;
    }


    if (geo_feature->geometry.type == GEOJSON_POLYGON) {
        // Triangulate the shape.
        // First we rotate the points so that they are centered around the
        // origin.
        create_rotation_between_vecs(rot, feature->bounding_cap, VEC(1, 0, 0));
        centered_lonlat = calloc(size, sizeof(*centered_lonlat));
        for (i = 0; i < size; i++) {
            mat3_mul_vec3(rot, feature->vertices[i], p);
            c2lonlat(p, centered_lonlat[i]);
        }

        earcut = earcut_new();
        earcut_set_poly(earcut, size, centered_lonlat);
        triangles = earcut_triangulate(earcut, &triangles_size);
        feature->triangles_count = triangles_size;
        feature->triangles = malloc(triangles_size * 2);
        memcpy(feature->triangles, triangles, triangles_size * 2);
        earcut_delete(earcut);
        free(centered_lonlat);
    }

    LL_APPEND(image->features, feature);
}

static void remove_all_features(image_t *image)
{
    feature_t *feature;
    while(image->features) {
        feature = image->features;
        LL_DELETE(image->features, feature);
        free(feature->vertices);
        free(feature->triangles);
        free(feature->lines);
        free(feature);
        free(feature->title);
    }
}

static json_value *data_fn(obj_t *obj, const attribute_t *attr,
                           const json_value *args)
{
    int i;
    image_t *image = (void*)obj;
    geojson_t *geojson;

    remove_all_features(image);
    args = args->u.array.values[0];
    geojson = geojson_parse(args);
    assert(geojson);
    for (i = 0; i < geojson->nb_features; i++) {
        add_geojson_feature(image, &geojson->features[i]);
    }
    geojson_delete(geojson);
    return NULL;
}

static int image_render(const obj_t *obj, const painter_t *painter_)
{
    const image_t *image = (void*)obj;
    painter_t painter = *painter_;
    const feature_t *feature;
    double pos[2], ofs[2];
    int frame = image->frame;

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->fill_color[3]) {
            vec4_copy(feature->fill_color, painter.color);
            paint_mesh(&painter, frame, MODE_TRIANGLES,
                       feature->vertices_count, feature->vertices,
                       feature->triangles_count, feature->triangles,
                       feature->bounding_cap);
        }

        if (feature->stroke_color[3]) {
            vec4_copy(feature->stroke_color, painter.color);
            painter.lines_width = feature->stroke_width;
            paint_mesh(&painter, frame, MODE_LINES,
                       feature->vertices_count, feature->vertices,
                       feature->lines_count, feature->lines,
                       feature->bounding_cap);
        }

        if (feature->title) {
            painter_project(&painter, frame, feature->bounding_cap,
                            true, false, pos);
            vec2_copy(feature->text_offset, ofs);
            vec2_rotate(feature->text_rotate, ofs, ofs);
            vec2_add(pos, ofs, pos);
            paint_text(&painter, feature->title, pos, feature->text_anchor,
                       0, FONT_SIZE_BASE,
                       VEC(VEC4_SPLIT(feature->stroke_color)),
                       feature->text_rotate);
        }
    }
    return 0;
}


/*
 * Meta class declarations.
 */

static obj_klass_t image_klass = {
    .id = "geojson",
    .size = sizeof(image_t),
    .init = image_init,
    .render = image_render,
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = data_fn),
        PROPERTY(frame, TYPE_ENUM, MEMBER(image_t, frame)),
        {}
    },
};
OBJ_REGISTER(image_klass)
