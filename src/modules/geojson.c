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

typedef struct shape shape_t;

typedef struct shape {
    shape_t     *next;
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
} shape_t;

typedef struct image {
    obj_t       obj;
    shape_t     *shapes;
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

static void add_geojson_feature(image_t *image,
                                const geojson_feature_t *feature)
{
    shape_t *shape;
    earcut_t *earcut;
    const double (*coordinates)[2];
    int i, size, triangles_size;
    const uint16_t *triangles;
    double rot[3][3], p[3];
    double (*centered_lonlat)[2];

    shape = calloc(1, sizeof(*shape));

    vec3_copy(feature->properties.fill, shape->fill_color);
    vec3_copy(feature->properties.stroke, shape->stroke_color);
    shape->fill_color[3] = feature->properties.fill_opacity;
    shape->stroke_color[3] = feature->properties.stroke_opacity;
    shape->stroke_width = feature->properties.stroke_width;
    if (feature->properties.title)
        shape->title = strdup(feature->properties.title);
    shape->text_anchor = feature->properties.text_anchor;

    switch (feature->geometry.type) {
    case GEOJSON_LINESTRING:
        coordinates = feature->geometry.linestring.coordinates;
        size = feature->geometry.linestring.size;
        break;
    case GEOJSON_POLYGON:
        coordinates = feature->geometry.polygon.rings[0].coordinates;
        size = feature->geometry.polygon.rings[0].size;
        break;
    case GEOJSON_POINT:
        coordinates = &feature->geometry.point.coordinates;
        size = 1;
        break;
    default:
        assert(false);
    }

    shape->vertices_count = size;
    shape->vertices = malloc(size * sizeof(*shape->vertices));
    for (i = 0; i < size; i++) {
        lonlat2c(coordinates[i], shape->vertices[i]);
    }
    compute_bounding_cap(size, shape->vertices, shape->bounding_cap);

    // Generates contour index.
    shape->lines_count = size * 2;
    shape->lines = malloc(size * 2 * 2);
    for (i = 0; i < size - 1; i++) {
        shape->lines[i * 2 + 0] = i;
        shape->lines[i * 2 + 1] = i + 1;
    }


    if (feature->geometry.type == GEOJSON_POLYGON) {
        // Triangulate the shape.
        // First we rotate the points so that they are centered around the
        // origin.
        create_rotation_between_vecs(rot, shape->bounding_cap, VEC(1, 0, 0));
        centered_lonlat = calloc(size, sizeof(*centered_lonlat));
        for (i = 0; i < size; i++) {
            mat3_mul_vec3(rot, shape->vertices[i], p);
            c2lonlat(p, centered_lonlat[i]);
        }

        earcut = earcut_new();
        earcut_set_poly(earcut, size, centered_lonlat);
        triangles = earcut_triangulate(earcut, &triangles_size);
        shape->triangles_count = triangles_size;
        shape->triangles = malloc(triangles_size * 2);
        memcpy(shape->triangles, triangles, triangles_size * 2);
        earcut_delete(earcut);
        free(centered_lonlat);
    }

    LL_APPEND(image->shapes, shape);
}

static void remove_all_features(image_t *image)
{
    shape_t *shape;
    while(image->shapes) {
        shape = image->shapes;
        LL_DELETE(image->shapes, shape);
        free(shape->vertices);
        free(shape->triangles);
        free(shape->lines);
        free(shape);
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
    const shape_t *shape;

    for (shape = image->shapes; shape; shape = shape->next) {
        if (shape->fill_color[3]) {
            vec4_copy(shape->fill_color, painter.color);
            paint_mesh(&painter, FRAME_ICRF, MODE_TRIANGLES,
                       shape->vertices_count, shape->vertices,
                       shape->triangles_count, shape->triangles,
                       shape->bounding_cap);
        }

        if (shape->stroke_color[3]) {
            vec4_copy(shape->stroke_color, painter.color);
            painter.lines_width = shape->stroke_width;
            paint_mesh(&painter, FRAME_ICRF, MODE_LINES,
                       shape->vertices_count, shape->vertices,
                       shape->lines_count, shape->lines,
                       shape->bounding_cap);
        }

        if (shape->title) {
            labels_add_3d(shape->title, FRAME_ICRF, shape->vertices[0],
                          true, 0, FONT_SIZE_BASE, VEC(1, 1, 1, 1), 0,
                          shape->text_anchor, 0, 0, 0);
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
    .render = image_render,
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = data_fn),
        {}
    },
};
OBJ_REGISTER(image_klass)
