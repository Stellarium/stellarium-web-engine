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
    mesh_t      *next, *prev;
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
    feature_t   *next, *prev;
    mesh_t      *meshes;
    int         frame;
    float       fill_color[4];
    float       stroke_color[4];
    float       stroke_width;
    char        *title;
    int         text_anchor;
    float       text_rotate;
    float       text_offset[2];
    bool        hidden;
};

/*
 * Struct: image_t
 * Represents a geojson document
 *
 * Attributes:
 *   filter - Function called for each feature.  Can set the fill and stroke
 *            color.  If it returns zero, then the feature is hidden.
 */
typedef struct image {
    obj_t       obj;
    feature_t   *features;
    int         frame;
    int         (*filter)(int idx, float fill_color[4], float stroke_color[4]);
} image_t;


// Spherical triangle / point intersection.
static bool triangle_contains_vec3(const double verts[][3],
                                   const uint16_t indices[],
                                   const double pos[3])
{
    int i;
    double u[3];
    for (i = 0; i < 3; i++) {
        vec3_cross(verts[indices[i]], verts[indices[(i + 1) % 3]], u);
        if (vec3_dot(u, pos) > 0) return false;
    }
    return true;
}

/*
 * Function: mesh_contains_vec3
 * Test if a 3d direction vector intersects a 3d mesh.
 */
static bool mesh_contains_vec3(const mesh_t *mesh, const double pos[3])
{
    int i;
    if (!cap_contains_vec3(mesh->bounding_cap, pos))
        return false;
    for (i = 0; i < mesh->triangles_count; i += 3) {
        if (triangle_contains_vec3(mesh->vertices, mesh->triangles + i, pos))
            return true;
    }
    return false;
}

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
        DL_APPEND(feature->meshes, mesh);
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
    DL_APPEND(feature->meshes, mesh);
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
    DL_APPEND(image->features, feature);
}

static void feature_del(obj_t *obj)
{
    feature_t *feature = (void*)obj;
    mesh_t *mesh;

    while (feature->meshes) {
        mesh = feature->meshes;
        DL_DELETE(feature->meshes, mesh);
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

EMSCRIPTEN_KEEPALIVE
void geojson_remove_all_features(image_t *image)
{
    feature_t *feature;

    while (image->features) {
        feature = image->features;
        DL_DELETE(image->features, feature);
        obj_release(&feature->obj);
    }
}

// XXX: deprecated: we use filter_all instead now!
static void apply_filter(image_t *image)
{
    feature_t *feature;
    int i = 0, r;
    if (!image->filter) return;
    for (feature = image->features; feature; feature = feature->next, i++) {
        r = image->filter(i, feature->fill_color, feature->stroke_color);
        feature->hidden = (r == 0);
    }
}

static json_value *data_fn(obj_t *obj, const attribute_t *attr,
                           const json_value *args)
{
    image_t *image = (void*)obj;
    geojson_t *geojson;
    int i;

    if (!args) return NULL;
    geojson_remove_all_features(image);
    geojson = geojson_parse(args);
    if (!geojson) {
        LOG_E("Cannot parse geojson");
        return NULL;
    }
    for (i = 0; i < geojson->nb_features; i++) {
        add_geojson_feature(image, &geojson->features[i]);
    }
    geojson_delete(geojson);
    apply_filter(image);
    return NULL;
}

static json_value *filter_fn(obj_t *obj, const attribute_t *attr,
                             const json_value *args)
{
    image_t *image = (void*)obj;
    if (!args) return NULL;
    if (args->type != json_integer) {
        LOG_E("Wrong type for filter attribute");
        return NULL;
    }
    image->filter = (void*)(intptr_t)(args->u.integer);
    apply_filter(image);
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
void geojson_filter_all(image_t *image,
                        int (*f)(int idx, float fill[4], float stroke[4]))
{
    feature_t *feature;
    int i = 0, r;
    for (feature = image->features; feature; feature = feature->next, i++) {
        r = f(i, feature->fill_color, feature->stroke_color);
        feature->hidden = (r == 0);
    }
}

static int image_render(const obj_t *obj, const painter_t *painter_)
{
    const image_t *image = (void*)obj;
    painter_t painter = *painter_;
    const feature_t *feature;
    double pos[2], ofs[2];
    int frame = image->frame;
    const mesh_t *mesh;

    /*
     * For the moment, we render all the filled shapes first, then
     * all the lines, and then all the titles.  This allows the renderer
     * to merge the rendering calls together.
     * We should probably instead allow the renderer to reorder the calls.
     */
    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden || feature->fill_color[3] == 0) continue;
        vec4_copy(feature->fill_color, painter.color);
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            paint_mesh(&painter, frame, MODE_TRIANGLES,
                       mesh->vertices_count, mesh->vertices,
                       mesh->triangles_count, mesh->triangles,
                       mesh->bounding_cap);
        }
    }

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden || feature->stroke_color[3] == 0) continue;
        vec4_copy(feature->stroke_color, painter.color);
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            painter.lines_width = feature->stroke_width;
            paint_mesh(&painter, frame, MODE_LINES,
                       mesh->vertices_count, mesh->vertices,
                       mesh->lines_count, mesh->lines,
                       mesh->bounding_cap);
        }
    }

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden) continue;
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
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
    geojson_remove_all_features(image);
}

// Special function for fast geojson parsing directly from js!
// Still experimental.
EMSCRIPTEN_KEEPALIVE
void geojson_add_poly_feature(image_t *image, int size, const double *data)
{
    geojson_linestring_t ring;
    geojson_feature_t feature = {
        .properties = {
            .fill = {1, 1, 1},
            .fill_opacity = 0.5,
            .stroke = {1, 1, 1},
            .stroke_opacity = 1,
            .stroke_width = 1,
        },
        .geometry = {
            .type = GEOJSON_POLYGON,
            .polygon = {
                .size = 1,
                .rings = &ring,
            },
        },
    };
    ring.size = size;
    ring.coordinates = (void*)data;
    add_geojson_feature(image, &feature);
}

/*
 * Experimental function to get the list of rendered features index.
 * Return the number of features returned.
 *
 * Assume the current core observer and projection!
 */
EMSCRIPTEN_KEEPALIVE
int geojson_query_rendered_features(
        const obj_t *obj, double win_pos[2], int max_ret, int *index)
{
    const image_t *image = (void*)obj;
    int i = 0, nb = 0;
    const feature_t *feature;
    const mesh_t *mesh;
    painter_t painter;
    int frame = image->frame;
    projection_t proj;
    double pos[3];

    core_get_proj(&proj);
    painter = (painter_t) {
        .obs = core->observer,
        .proj = &proj,
    };
    painter_update_clip_info(&painter);
    painter_unproject(&painter, frame, win_pos, pos);

    for (feature = image->features; feature; feature = feature->next, i++) {
        if (nb >= max_ret) break;
        if (feature->hidden) continue;
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (mesh_contains_vec3(mesh, pos))
                index[nb++] = i;
        }
    }
    return nb;
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
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = data_fn),
        PROPERTY(frame, TYPE_ENUM, MEMBER(image_t, frame)),
        PROPERTY(filter, TYPE_FUNC, .fn = filter_fn),
        {}
    },
};
OBJ_REGISTER(image_klass)
