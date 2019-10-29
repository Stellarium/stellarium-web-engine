/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include "geojson_parser.h"

#include "utils/mesh.h"

typedef struct feature feature_t;

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
    bool        blink;
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


static int image_init(obj_t *obj, json_value *args)
{
    image_t *image = (void*)obj;
    image->frame = FRAME_ICRF;
    return 0;
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
            ofs = mesh_add_vertices_lonlat(
                    mesh, size, geo->polygon.rings[i].coordinates);
            if (i == 0) rings_ofs = ofs;
            mesh_add_line(mesh, ofs, size);
            rings_size[i] = size;
        }
        mesh_add_poly(mesh, geo->polygon.size, rings_ofs, rings_size);

        // For testing.  We want to avoid meshes with too long edges
        // for the distortion.
        mesh_subdivide(mesh, M_PI / 8);

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
    ofs = mesh_add_vertices_lonlat(mesh, size, coordinates);
    mesh_add_line(mesh, ofs, size);

    DL_APPEND(feature->meshes, mesh);
}

static void add_geojson_feature(image_t *image,
                                const geojson_feature_t *geo_feature)
{
    static uint32_t g_id = 1;
    feature_t *feature;

    feature = (void*)obj_create("geojson-feature", NULL, NULL);
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
        mesh_delete(mesh);
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
        feature->blink = r & 0x2;
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
        feature->blink = r & 0x2;
    }
}

// Compute the blink alpha coef.  Probably need to be changed.
static float blink(void)
{
    const float period = 1;
    float t; // Range from 0 to 1.
    t = (sin(sys_get_unix_time() * 2 * M_PI / period) + 1) / 2;
    return mix(0.5f, 1.0f, t);
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
        if (feature->blink)
            painter.color[3] *= blink();
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            paint_mesh(&painter, frame, MODE_TRIANGLES, mesh);
        }
    }

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden || feature->stroke_color[3] == 0) continue;
        vec4_copy(feature->stroke_color, painter.color);
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            painter.lines_width = feature->stroke_width;
            paint_mesh(&painter, frame, MODE_LINES, mesh);
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
