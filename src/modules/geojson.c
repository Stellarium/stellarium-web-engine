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
#include "geojson_parser.h"

typedef struct mesh mesh_t;
typedef struct feature feature_t;

struct mesh {
    mesh_t      *next;
    double      bounding_cap[4];
    int         vertices_count;
    double      (*vertices)[3];
    int         triangles_count; // Number of triangles * 3.
    uint16_t    *triangles;
    int         lines_count; // Number of lines * 2.
    uint16_t    *lines;
};

struct feature {
    obj_t       obj;
    feature_t   *next;
    mesh_t      *meshes;
    int         frame;
    float       fill_color[4];
    float       stroke_color[4];
    float       stroke_width;
    char        *title;
    int         text_anchor;
    float       text_rotate;
    float       text_offset[2];
};

typedef struct image {
    obj_t       obj;

    json_value  *geojson;
    json_value  *filter;
    bool        dirty;

    feature_t   *features;
    int         frame;
} image_t;


static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(lonlat[0] * DD2R, lonlat[1] * DD2R, c);
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

static int mesh_add_vertices(mesh_t *mesh, int count, double (*verts)[2])
{
    int i, ofs;
    ofs = mesh->vertices_count;
    mesh->vertices = realloc(mesh->vertices,
            (mesh->vertices_count + count) * sizeof(*mesh->vertices));
    for (i = 0; i < count; i++) {
        assert(!isnan(verts[i][0]));
        lonlat2c(verts[i], mesh->vertices[mesh->vertices_count + i]);
        assert(!isnan(mesh->vertices[mesh->vertices_count + i][0]));
    }
    mesh->vertices_count += count;
    compute_bounding_cap(mesh->vertices_count, mesh->vertices,
                         mesh->bounding_cap);
    return ofs;
}

static void mesh_add_line(mesh_t *mesh, int ofs, int size)
{
    int i;
    mesh->lines = realloc(mesh->lines, (mesh->lines_count + (size - 1) * 2) *
                          sizeof(*mesh->lines));
    for (i = 0; i < size - 1; i++) {
        mesh->lines[mesh->lines_count + i * 2 + 0] = ofs + i;
        mesh->lines[mesh->lines_count + i * 2 + 1] = ofs + i + 1;
    }
    mesh->lines_count += (size - 1) * 2;
}

static void mesh_add_poly(mesh_t *mesh, int nb_rings,
                          const int ofs, const int *size)
{
    int r, i, j = 0, triangles_size;
    double rot[3][3], p[3];
    double (*centered_lonlat)[2];
    const uint16_t *triangles;
    earcut_t *earcut;

    earcut = earcut_new();
    // Triangulate the shape.
    // First we rotate the points so that they are centered around the
    // origin.
    create_rotation_between_vecs(rot, mesh->bounding_cap, VEC(1, 0, 0));

    for (r = 0; r < nb_rings; r++) {
        centered_lonlat = calloc(size[r], sizeof(*centered_lonlat));
        for (i = 0; i < size[r]; i++) {
            mat3_mul_vec3(rot, mesh->vertices[j++], p);
            c2lonlat(p, centered_lonlat[i]);
        }
        earcut_add_poly(earcut, size[r], centered_lonlat);
        free(centered_lonlat);
    }

    triangles = earcut_triangulate(earcut, &triangles_size);
    mesh->triangles = realloc(mesh->triangles,
            (mesh->triangles_count + triangles_size) *
            sizeof(*mesh->triangles));
    for (i = 0; i < triangles_size; i++) {
        mesh->triangles[mesh->triangles_count + i] = ofs + triangles[i];
    }
    mesh->triangles_count += triangles_size;
    earcut_delete(earcut);
}

static void feature_add_geo(feature_t *feature, const geojson_geometry_t *geo)
{
    const double (*coordinates)[2];
    int i, size, ofs;
    int rings_ofs = 0, rings_size[8];
    mesh_t *mesh;
    geojson_geometry_t poly;

    switch (geo->type) {
    case GEOJSON_LINESTRING:
        coordinates = geo->linestring.coordinates;
        size = geo->linestring.size;
        break;
    case GEOJSON_POLYGON:
        mesh = calloc(1, sizeof(*mesh));
        for (i = 0; i < geo->polygon.size; i++) {
            size = geo->polygon.rings[i].size;
            ofs = mesh_add_vertices(mesh, size,
                                    geo->polygon.rings[i].coordinates);
            if (i == 0) rings_ofs = ofs;
            mesh_add_line(mesh, ofs, size);
            rings_size[i] = size;
        }
        mesh_add_poly(mesh, geo->polygon.size, rings_ofs, rings_size);
        LL_APPEND(feature->meshes, mesh);
        return;
    case GEOJSON_POINT:
        coordinates = &geo->point.coordinates;
        size = 1;
        break;
    case GEOJSON_MULTIPOLYGON:
        for (i = 0; i < geo->multipolygon.size; i++) {
            poly.type = GEOJSON_POLYGON;
            poly.polygon = geo->multipolygon.polygons[i];
            feature_add_geo(feature, &poly);
        }
        return;
    default:
        assert(false);
        return;
    }
    mesh = calloc(1, sizeof(*mesh));
    ofs = mesh_add_vertices(mesh, size, coordinates);
    mesh_add_line(mesh, ofs, size);
    LL_APPEND(feature->meshes, mesh);
}

static void add_geojson_feature(image_t *image,
                                const geojson_feature_t *geo_feature)
{
    static uint32_t g_id = 1;
    feature_t *feature;

    feature = (void*)obj_create("geojson-feature", NULL, NULL, NULL);
    feature->frame = image->frame;
    feature->obj.oid = oid_create("GEOF", g_id++);

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

    feature_add_geo(feature, &geo_feature->geometry);
    LL_APPEND(image->features, feature);
}

static void feature_del(obj_t *obj)
{
    feature_t *feature = (void*)obj;
    mesh_t *mesh;

    while (feature->meshes) {
        mesh = feature->meshes;
        LL_DELETE(feature->meshes, mesh);
        free(mesh->vertices);
        free(mesh->triangles);
        free(mesh->lines);
        free(mesh);
    }
    free(feature->title);
}

static int feature_get_info(const obj_t *obj, const observer_t *obs,
                            int info, void *out)
{
    feature_t *feature = (void*)obj;

    switch (info) {
    case INFO_PVO:
        convert_frame(obs, feature->frame, FRAME_ICRF, true,
                      feature->meshes[0].bounding_cap, out);
        return 0;
    default:
        return 1;
    }
}

static void remove_all_features(image_t *image)
{
    feature_t *feature;

    while(image->features) {
        feature = image->features;
        LL_DELETE(image->features, feature);
        obj_release(&feature->obj);
    }
}

static json_value *data_fn(obj_t *obj, const attribute_t *attr,
                           const json_value *args)
{
    image_t *image = (void*)obj;
    if (!args) return json_copy(image->geojson);
    if (image->geojson) json_builder_free(image->geojson);
    image->geojson = json_copy(args);
    image->dirty = true;
    return NULL;
}

static json_value *filter_fn(obj_t *obj, const attribute_t *attr,
                             const json_value *args)
{
    image_t *image = (void*)obj;
    if (!args) return json_copy(image->filter);
    if (image->filter) json_builder_free(image->filter);
    image->filter = json_copy(args);
    image->dirty = true;
    return NULL;
}

static int image_update(image_t *image)
{
    geojson_t *geojson;
    int i;

    if (!image->dirty) return 0;
    image->dirty = false;
    remove_all_features(image);
    geojson = geojson_parse(image->geojson, image->filter);
    assert(geojson);
    for (i = 0; i < geojson->nb_features; i++) {
        add_geojson_feature(image, &geojson->features[i]);
    }
    geojson_delete(geojson);
    return 0;
}

static int image_render(const obj_t *obj, const painter_t *painter_)
{
    const image_t *image = (void*)obj;
    painter_t painter = *painter_;
    const feature_t *feature;
    double pos[2], ofs[2];
    int frame = image->frame;
    const mesh_t *mesh;

    if (image->dirty)
        image_update((image_t*) image);

    for (feature = image->features; feature; feature = feature->next) {
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (feature->fill_color[3]) {
                vec4_copy(feature->fill_color, painter.color);
                paint_mesh(&painter, frame, MODE_TRIANGLES,
                           mesh->vertices_count, mesh->vertices,
                           mesh->triangles_count, mesh->triangles,
                           mesh->bounding_cap);
            }

            if (feature->stroke_color[3]) {
                vec4_copy(feature->stroke_color, painter.color);
                painter.lines_width = feature->stroke_width;
                paint_mesh(&painter, frame, MODE_LINES,
                           mesh->vertices_count, mesh->vertices,
                           mesh->lines_count, mesh->lines,
                           mesh->bounding_cap);
            }
            if (feature->title) {
                painter_project(&painter, frame, mesh->bounding_cap,
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
    }
    return 0;
}

static void image_del(obj_t *obj)
{
    image_t *image = (void*)obj;
    remove_all_features(image);
    if (image->filter) json_builder_free(image->filter);
    if (image->geojson) json_builder_free(image->geojson);
}

static obj_t *image_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    image_t *image = (void*)obj;
    feature_t *feature;

    if (!oid_is_catalog(oid, "GEOF")) return NULL;
    for (feature = image->features; feature; feature = feature->next) {
        if (feature->obj.oid == oid) {
            obj_retain(&feature->obj);
            return &feature->obj;
        }
    }
    return NULL;
}


/*
 * Meta class declarations.
 */

static obj_klass_t geojson_feature_klass = {
    .id = "geojson-feature",
    .del = feature_del,
    .get_info = feature_get_info,
    .size = sizeof(feature_t),
};
OBJ_REGISTER(geojson_feature_klass)

static obj_klass_t image_klass = {
    .id = "geojson",
    .size = sizeof(image_t),
    .init = image_init,
    .render = image_render,
    .del = image_del,
    .get_by_oid = image_get_by_oid,
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = data_fn),
        PROPERTY(filter, TYPE_JSON, .fn = filter_fn),
        PROPERTY(frame, TYPE_ENUM, MEMBER(image_t, frame)),
        {}
    },
};
OBJ_REGISTER(image_klass)
