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
typedef struct image image_t;

typedef struct {
    int size;
    double (*points)[3];
} linestring_t;

struct feature {
    obj_t       obj;
    feature_t   *next, *prev;
    mesh_t      *meshes;
    linestring_t linestring; // Only support a single linestring for the moment.
    int         frame;
    float       fill_color[4];
    float       stroke_color[4];
    float       stroke_width;
    bool        stroke_glow;
    char        *title;
    int         text_anchor;
    int         text_size;
    float       text_rotate;
    float       text_offset[2];
    bool        hidden;
    bool        blink;
};

typedef void (*filter_fn_t)(const image_t *img, int idx,
                            float fill_color[4], float stroke_color[4],
                            bool *blink, bool *hidden);

/*
 * Struct: image_t
 * Represents a geojson document
 *
 * Attributes:
 *   filter - Function called for each feature.  Can set the fill and stroke
 *            color.  If it returns zero, then the feature is hidden.
 */
struct image {
    obj_t       obj;
    feature_t   *features;
    int         frame;
    filter_fn_t filter;
    int         filter_idx;
    double      z;      // For sorting inside a layer.
};


typedef struct survey {
    obj_t       obj;
    char        *path;
    hips_t      *hips;
    image_t     *allsky;
    bool        allsky_loaded;
    double      min_fov;
    double      max_fov;

    void        (*filter)(const image_t *img, int idx,
                          float fill_color[4], float stroke_color[4],
                          bool *blink, bool *hidden);
    int         filter_idx;
    double      z;      // For sorting inside a layer.
} survey_t;


static int image_init(obj_t *obj, json_value *args)
{
    image_t *image = (void*)obj;
    image->frame = FRAME_ICRF;
    return 0;
}

static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(lonlat[0] * ERFA_DD2R, lonlat[1] * ERFA_DD2R, c);
}

/* Parse a geojson linestring into the feature linestring in cartesian
 * coordinates.  Note: maybe should directly be done by the geojson parser.  */
static void linestring2c(const geojson_linestring_t *ls, feature_t *feature)
{
    int i;
    feature->linestring.size = ls->size;
    feature->linestring.points =
        calloc(ls->size, sizeof(*feature->linestring.points));
    for (i = 0; i < ls->size; i++)
        lonlat2c(ls->coordinates[i], feature->linestring.points[i]);
}

static void feature_add_geo(feature_t *feature, const geojson_geometry_t *geo,
                            bool save_linestring)
{
    const double (*coordinates)[2];
    int *rings_size;
    const double (**rings_verts)[2];
    int i, size;
    mesh_t *mesh = NULL;
    geojson_geometry_t poly;

    switch (geo->type) {
    case GEOJSON_LINESTRING:
        coordinates = geo->linestring.coordinates;
        size = geo->linestring.size;
        mesh = calloc(1, sizeof(*mesh));
        mesh_add_line_lonlat(mesh, size, coordinates, false);
        DL_APPEND(feature->meshes, mesh);
        if (save_linestring && !feature->linestring.size && size) {
            linestring2c(&geo->linestring, feature);
        }
        break;

    case GEOJSON_POLYGON:
        mesh = calloc(1, sizeof(*mesh));
        rings_size = calloc(geo->polygon.size, sizeof(*rings_size));
        rings_verts = calloc(geo->polygon.size, sizeof(*rings_verts));
        for (i = 0; i < geo->polygon.size; i++) {
            rings_size[i] = geo->polygon.rings[i].size - 1;
            rings_verts[i] = geo->polygon.rings[i].coordinates;
        }
        mesh_add_poly_lonlat(mesh, geo->polygon.size, rings_size, rings_verts);
        free(rings_size);
        free(rings_verts);

        DL_APPEND(feature->meshes, mesh);
        break;

    case GEOJSON_POINT:
        coordinates = &geo->point.coordinates;
        mesh = calloc(1, sizeof(*mesh));
        mesh_add_point_lonlat(mesh, coordinates[0]);
        DL_APPEND(feature->meshes, mesh);
        break;

    case GEOJSON_MULTIPOLYGON:
        for (i = 0; i < geo->multipolygon.size; i++) {
            poly.type = GEOJSON_POLYGON;
            poly.polygon = geo->multipolygon.polygons[i];
            feature_add_geo(feature, &poly, false);
        }
        break;

    default:
        assert(false);
        return;
    }
    if (mesh) mesh_update_bounding_cap(mesh);
}

static void add_geojson_feature(image_t *image,
                                const geojson_feature_t *geo_feature)
{
    feature_t *feature;

    feature = (void*)obj_create("geojson-feature", NULL);
    feature->frame = image->frame;

    vec3_copy(geo_feature->properties.fill, feature->fill_color);
    vec3_copy(geo_feature->properties.stroke, feature->stroke_color);
    feature->fill_color[3] = geo_feature->properties.fill_opacity;
    feature->stroke_color[3] = geo_feature->properties.stroke_opacity;
    feature->stroke_width = geo_feature->properties.stroke_width;
    feature->stroke_glow = geo_feature->properties.stroke_glow;
    if (geo_feature->properties.title)
        feature->title = strdup(geo_feature->properties.title);
    feature->text_anchor = geo_feature->properties.text_anchor;
    feature->text_size = geo_feature->properties.text_size;
    feature->text_rotate = geo_feature->properties.text_rotate;
    vec2_copy(geo_feature->properties.text_offset, feature->text_offset);

    feature_add_geo(feature, &geo_feature->geometry, feature->stroke_glow);
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
    free(feature->linestring.points);
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

// Use to optimize the js code so that we don't use the slow _setValue.
EMSCRIPTEN_KEEPALIVE
void geojson_set_bool_ptr_(bool *ptr, bool value)
{
    *ptr = value;
}

// Use to optimize the js code so that we don't use the slow _setValue.
EMSCRIPTEN_KEEPALIVE
void geojson_set_color_ptr_(float *ptr, float r, float g, float b, float a)
{
    ptr[0] = r;
    ptr[1] = g;
    ptr[2] = b;
    ptr[3] = a;
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

static void apply_filter(image_t *image)
{
    feature_t *feature;
    int i = 0;
    if (!image->filter) return;
    for (feature = image->features; feature; feature = feature->next, i++) {
        image->filter(image, i, feature->fill_color, feature->stroke_color,
                      &feature->blink, &feature->hidden);
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
    int frame = image->frame, mode;
    const mesh_t *mesh;
    double c[4];

    /*
     * For the moment, we render all the filled shapes first, then
     * all the lines, and then all the titles.  This allows the renderer
     * to merge the rendering calls together.
     * We should probably instead allow the renderer to reorder the calls.
     */
    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden || feature->fill_color[3] == 0) continue;
        vec4_copy(feature->fill_color, c);
        vec4_emul(c, painter_->color, painter.color);
        if (feature->blink)
            painter.color[3] *= blink();
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            mode = mesh->points_count ? MODE_POINTS : MODE_TRIANGLES;
            paint_mesh(&painter, frame, mode, mesh);
        }
    }

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden || feature->stroke_color[3] == 0) continue;
        vec4_copy(feature->stroke_color, c);
        vec4_emul(c, painter_->color, painter.color);
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (mesh->points_count) continue;
            painter.lines.width = feature->stroke_width;
            if (feature->linestring.size) {
                paint_linestring(&painter, frame, feature->linestring.size,
                                 feature->linestring.points);
            } else {
                paint_mesh(&painter, frame, MODE_LINES, mesh);
            }
        }
    }

    for (feature = image->features; feature; feature = feature->next) {
        if (feature->hidden) continue;
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (feature->title) {
                vec4_copy(feature->stroke_color, c);
                vec4_emul(c, painter_->color, painter.color);
                painter_project(&painter, frame, mesh->bounding_cap,
                                true, false, pos);
                vec2_copy(feature->text_offset, ofs);
                vec2_rotate(feature->text_rotate, ofs, ofs);
                vec2_add(pos, ofs, pos);
                paint_text(&painter, feature->title, pos, NULL,
                           feature->text_anchor,
                           0, feature->text_size > 0 ? feature->text_size :
                                                       FONT_SIZE_BASE,
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

static int query_rendered_features_(
        const image_t *image, const double pos[3], int max_ret,
        void **tiles, int *index)
{
    int i = 0, nb = 0;
    const feature_t *feature;
    const mesh_t *mesh;

    for (feature = image->features; feature; feature = feature->next, i++) {
        if (nb >= max_ret) break;
        if (feature->hidden) continue;
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (mesh_contains_vec3(mesh, pos)) {
                index[nb] = i;
                if (tiles) tiles[nb] = image;
                nb++;
                break;
            }
        }
    }
    return nb;
}

static bool mesh_intersects_box(const mesh_t *mesh_, const painter_t *painter,
                                const double box[2][2])
{
    mesh_t *mesh;
    int i;
    double p[4];
    bool ret;

    // Project the mesh vertices into screen coordinates.
    mesh = mesh_copy(mesh_);
    for (i = 0; i < mesh->vertices_count; i++) {
        vec3_normalize(mesh->vertices[i], p);
        convert_frame(painter->obs, FRAME_ICRF, FRAME_VIEW, true, p, p);
        project_to_win(painter->proj, p, p);
        vec2_copy(p, mesh->vertices[i]);
    }
    ret = mesh_intersects_2d_box(mesh, box);
    mesh_delete(mesh);
    return ret;
}

static int query_rendered_features_box_(
        const painter_t *painter, const image_t *image,
        const double box[2][2], int max_ret,
        void **tiles, int *index)
{
    int i = 0, nb = 0;
    const feature_t *feature;
    const mesh_t *mesh;

    for (feature = image->features; feature; feature = feature->next, i++) {
        if (nb >= max_ret) break;
        if (feature->hidden) continue;
        for (mesh = feature->meshes; mesh; mesh = mesh->next) {
            if (mesh_intersects_box(mesh, painter, box)) {
                index[nb] = i;
                if (tiles) tiles[nb] = image;
                nb++;
                break;
            }
        }
    }
    return nb;
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

    return query_rendered_features_(image, pos, max_ret, NULL, index);
}

static void (*g_survey_on_new_tile)(void *tile, const char *data);

EMSCRIPTEN_KEEPALIVE
void geojson_set_on_new_tile_callback(
        void (*fn)(void *tile, const char *data))
{
    g_survey_on_new_tile = fn;
}

static void image_update_filter(image_t *image,
                                filter_fn_t filter, int filter_idx)
{
    if (image->filter_idx == filter_idx) return;
    image->filter = filter;
    image->filter_idx = filter_idx;
    apply_filter(image);
}

/*
 * Iter all the visible tiles at the appropriate order.
 */
static bool survey_iter_visible_tiles(
        const survey_t *survey,
        const painter_t *painter,
        hips_iterator_t *iter,
        int *order, int *pix, int *code,
        image_t **tile)
{
    int render_order;
    hips_t *hips = survey->hips;

    render_order = hips_get_render_order(hips, painter);
    render_order = clamp(render_order, hips->order_min, hips->order);

    while (true) {
        if (!hips_iter_next(iter, order, pix)) return false;
        if (painter_is_healpix_clipped(painter, hips->frame, *order, *pix))
            continue;
        if (*order < render_order) {
            hips_iter_push_children(iter, *order, *pix);
            continue;
        }
        *tile = hips_get_tile(hips, *order, *pix, HIPS_NO_DELAY, code);
        if (*tile)
            image_update_filter(*tile, survey->filter, survey->filter_idx);
        return true;
    }
}


/*
 * Experimental function to get the list of rendered features index.
 * Return the number of features returned.
 *
 * Assume the current core observer and projection!
 *
 * Parameters:
 *   box    - bounding box as two points.  If they are the same the search
 *            is done on a single point.
 */
EMSCRIPTEN_KEEPALIVE
int geojson_survey_query_rendered_features(
        const obj_t *obj, double box[2][2], int max_ret,
        void **tiles, int *index)
{
    const survey_t *survey = (void*)obj;
    int i, nb = 0;
    int order, pix, code;
    painter_t painter;
    projection_t proj;
    double pos[3];
    hips_t *hips = survey->hips;
    image_t *tile;
    hips_iterator_t iter;

    assert(!isnan(box[0][0] + box[0][1] + box[1][0] + box[0][1]));

    core_get_proj(&proj);
    painter = (painter_t) {
        .obs = core->observer,
        .proj = &proj,
        .fb_size = {core->win_size[0] * core->win_pixels_scale,
                    core->win_size[1] * core->win_pixels_scale},
    };
    painter_update_clip_info(&painter);

    // Case where we query a single point.
    if (box[0][0] == box[1][0] && box[0][1] == box[1][1]) {
        painter_unproject(&painter, hips->frame, box[0], pos);

        if (survey->allsky) {
            nb = geojson_query_rendered_features(
                    (obj_t*)survey->allsky, box[0], max_ret, index);
            for (i = 0; i < nb; i++) tiles[i] = survey->allsky;
        }

        if (!hips_is_ready(hips)) return nb;
        hips_iter_init(&iter);
        while (survey_iter_visible_tiles(survey, &painter, &iter, &order, &pix,
                                         &code, &tile)) {
            if (!tile) continue;
            if (nb >= max_ret) break;
            nb += query_rendered_features_(tile, pos, max_ret - nb,
                                           tiles + nb, index + nb);
            if (nb >= max_ret) break;
        }
        return nb;
    }

    if (!hips_is_ready(hips)) return 0;
    hips_iter_init(&iter);
    while (survey_iter_visible_tiles(survey, &painter, &iter, &order, &pix,
                                     &code, &tile)) {
        if (!tile) continue;
        if (nb >= max_ret) break;
        nb += query_rendered_features_box_(&painter, tile, box, max_ret - nb,
                                           tiles + nb, index + nb);
        if (nb >= max_ret) break;
    }
    return nb;
}

static const void *survey_create_tile(
        void *user, int order, int pix, void *data, int size,
        int *cost, int *transparency)
{
    int mask;
    json_value *jdata, *jhips;
    json_settings settings = {.value_extra = json_builder_extra};
    bool empty = false;
    image_t *tile;

    jdata = json_parse_ex(&settings, data, size, NULL);
    if (!jdata) return NULL;

    jhips = json_get_attr(jdata, "hips", json_object);
    if (jhips) {
        mask = json_get_attr_i(jhips, "children_mask", 15);
        *transparency = (~mask) & 15;
        if (!json_get_attr(jdata, "type", json_string))
            empty = true;
    }

    tile = (void*)obj_create("geojson", NULL);
    if (!empty) {
        obj_call_json((obj_t*)tile, "data", jdata);
        if (g_survey_on_new_tile)
            g_survey_on_new_tile(tile, data);
    }
    json_builder_free(jdata);

    return tile;
}

static int survey_delete_tile(void *data)
{
    return 0;
}

static int survey_init(obj_t *obj, json_value *args)
{
    survey_t *survey = (void*)obj;
    const char *path;
    int r;
    static int counter = 0;

    hips_settings_t settings = {
        .create_tile = survey_create_tile,
        .delete_tile = survey_delete_tile,
        .ext = "geojson",
    };

    if (!args) return -1;
    r = jcon_parse(args, "{",
            "path", JCON_STR(path),
            "?min_fov", JCON_DOUBLE(survey->min_fov, 0),
            "?max_fov", JCON_DOUBLE(survey->max_fov, 0),
        "}");
    if (r) {
        LOG_E("Cannot parse geojson survey");
        return -1;
    }

    survey->path = strdup(path);
    survey->min_fov *= DD2R;
    survey->max_fov *= DD2R;
    survey->hips = hips_create(path, 0, &settings);

    // Manually change the hash so that even surveys with same url are
    // considered independent, since we dynamically change the tiles
    // attributes with the filters.
    survey->hips->hash += counter++;

    hips_set_frame(survey->hips, FRAME_ICRF);
    return 0;
}

static void survey_load_allsky(survey_t *survey)
{
    char path[1024];
    void *data;
    int size, code;
    json_value *geojson;

    if (survey->allsky_loaded) return;
    // Attempt to load the allsky geojson document if available.
    snprintf(path, sizeof(path), "%s/Allsky.geojson", survey->path);
    data = asset_get_data2(path, ASSET_ACCEPT_404 | ASSET_USED_ONCE,
                           &size, &code);
    if (!code) return;
    survey->allsky_loaded = true;
    if (!data) return;

    geojson = json_parse(data, size);
    if (!geojson) {
        LOG_E("Cannot parse %s", path);
        return;
    }
    survey->allsky = (void*)obj_create("geojson", NULL);
    data_fn((obj_t*)survey->allsky, NULL, geojson);
    if (g_survey_on_new_tile)
        g_survey_on_new_tile(survey->allsky, data);
    json_value_free(geojson);
}

static int survey_render(const obj_t *obj, const painter_t *painter)
{
    const survey_t *survey = (void*)obj;
    int nb_tot = 0, nb_loaded = 0;
    int order, pix, code;
    hips_t *hips = survey->hips;
    hips_iterator_t iter;
    image_t *tile;

    if (survey->min_fov && core->fov < survey->min_fov) return 0;
    if (survey->max_fov && core->fov >= survey->max_fov) return 0;

    survey_load_allsky((survey_t*)survey);
    if (survey->allsky) {
        image_update_filter(survey->allsky, survey->filter, survey->filter_idx);
        obj_render((obj_t*)survey->allsky, painter);
    }

    if (!hips_is_ready(hips)) return 0;
    hips_iter_init(&iter);
    while (survey_iter_visible_tiles(survey, painter, &iter, &order, &pix,
                                     &code, &tile)) {
        nb_tot++;
        if (code) nb_loaded++;
        if (!tile) continue;
        image_render((obj_t*)tile, painter);
    }

    progressbar_report(survey->hips->url, survey->hips->label,
                       nb_loaded, nb_tot, -1);
    return 0;
}

static json_value *survey_filter_fn(obj_t *obj, const attribute_t *attr,
                                    const json_value *args)
{
    static int filter_idx = 1;
    survey_t *survey = (void*)obj;
    if (!args) return NULL;
    if (args->type != json_integer) {
        LOG_E("Wrong type for filter attribute");
        return NULL;
    }
    survey->filter = (void*)(intptr_t)(args->u.integer);
    survey->filter_idx = filter_idx++;
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
    .attributes = (attribute_t[]) {
        PROPERTY(data, TYPE_JSON, .fn = data_fn),
        PROPERTY(frame, TYPE_ENUM, MEMBER(image_t, frame)),
        PROPERTY(filter, TYPE_FUNC, .fn = filter_fn),
        PROPERTY(z, TYPE_FLOAT, MEMBER(image_t, z)),
        {}
    },
};
OBJ_REGISTER(image_klass)

static obj_klass_t survey_klass = {
    .id             = "geojson-survey",
    .size           = sizeof(survey_t),
    .init           = survey_init,
    .render         = survey_render,
    .attributes = (attribute_t[]) {
        PROPERTY(filter, TYPE_FUNC, .fn = survey_filter_fn),
        PROPERTY(z, TYPE_FLOAT, MEMBER(survey_t, z)),
        {}
    },
};
OBJ_REGISTER(survey_klass);
