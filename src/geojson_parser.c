/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "geojson_parser.h"

#include "utils/utils_json.h"
#include "utils/vec.h"
#include "utlist.h"
#include "erfa.h"

#include <stdio.h>
#include <string.h>

// Conveniance macro that sets the error message and calls goto error.
#define ERROR(msg, ...) do { \
    snprintf(error_msg, sizeof(error_msg), msg, ##__VA_ARGS__); \
    goto error; } while (0)

/*
 * Parse an RGB html color element (eg #AABBCC)
 */
static int parse_color(const json_value *data, float color[static 3])
{
    const char *str;
    int r, g, b;
    if (!data) return -1;
    if (data->type != json_string) return -1;
    str = data->u.string.ptr;
    if (!*str || str[0] != '#') return -1;
    sscanf(str, "#%02x%02x%02x", &r, &g, &b);
    color[0] = r / 255.;
    color[1] = g / 255.;
    color[2] = b / 255.;
    return 0;
}

/*
 * Push a new coordinate into a linestring instance.
 *
 * We pass a pointer to the currently allocated size so that we can grow
 * the data if needed.
 */
static void coordinates_push(geojson_linestring_t *line, const double p[2],
                             int *allocated)
{
    if (line->size <= *allocated) {
        *allocated += 32;
        line->coordinates = realloc(line->coordinates,
                                   *allocated * sizeof(*line->coordinates));
    }
    vec2_copy(p, line->coordinates[line->size++]);
}

/*
 * Function: tesselate_bezier
 * Push a cubic bezier curve into linestring coordiantes.
 *
 * The algo is taken from nanovg.
 *
 * Parameters:
 *   line       - The linestring where we push the points.
 *   p1         - Bezier point.
 *   p2         - Bezier point.
 *   p3         - Bezier point.
 *   p4         - Bezier point.
 *   level      - Current recursion level (set zero for the first call).
 *   allocated  - Currently allocated size of the line coordinate.
 */
static void tesselate_bezier(geojson_linestring_t *line,
                             const double p1[static 2],
                             const double p2[static 2],
                             const double p3[static 2],
                             const double p4[static 2],
                             int level, int *allocated)
{
    double p12[2], p23[2], p34[2], p123[2], p234[2], p1234[2];
    double dx, dy, d2, d3;
    double tess = 0.25; // Pass as argument?

    if (level > 10) return;
    p12[0] = (p1[0] + p2[0]) * 0.5;
    p12[1] = (p1[1] + p2[1]) * 0.5;
    p23[0] = (p2[0] + p3[0]) * 0.5;
    p23[1] = (p2[1] + p3[1]) * 0.5;
    p34[0] = (p3[0] + p4[0]) * 0.5;
    p34[1] = (p3[1] + p4[1]) * 0.5;
    p123[0] = (p12[0] + p23[0]) * 0.5;
    p123[1] = (p12[1] + p23[1]) * 0.5;

    dx = p4[0] - p1[0];
    dy = p4[1] - p1[1];
    d2 = fabs(((p2[0] - p4[0]) * dy - (p2[1] - p4[1]) * dx));
    d3 = fabs(((p3[0] - p4[0]) * dy - (p3[1] - p4[1]) * dx));

    if ((d2 + d3) * (d2 + d3) < tess * (dx * dx + dy * dy)) {
        coordinates_push(line, p4, allocated);
        return;
    }

    p234[0] = (p23[0] + p34[0]) * 0.5;
    p234[1] = (p23[1] + p34[1]) * 0.5;
    p1234[0] = (p123[0] + p234[0]) * 0.5;
    p1234[1] = (p123[1] + p234[1]) * 0.5;

    tesselate_bezier(line, p1, p12, p123, p1234, level+1, allocated);
    tesselate_bezier(line, p1234, p234, p34, p4, level+1, allocated);
}

/*
 * Function: parse_float_array
 * Conveniance function to parse a json array of the form [x, y, ...]
 *
 * Parameters:
 *   data   - A json array.
 *   start  - Staring index (zero for the whole array).
 *   size   - Number of float values to parse.
 *   out    - Pointer to a double array large enough to get all the data.
 */
static int parse_float_array(const json_value *data, int start, int size,
                             double *out)
{
    int i;
    const json_value *e;

    if (!data || data->type != json_array) return -1;
    if (data->u.array.length != size + start) return -1;
    for (i = 0; i < size; i++) {
        e = data->u.array.values[i + start];
        switch (e->type) {
            case json_double: out[i] = e->u.dbl; break;
            case json_integer: out[i] = e->u.integer; break;
            default: return -1;
        }
    }
    return 0;
}

/*
 * Function: parse_path_cmd
 * Parse a single path into a geometry instance
 */
static int parse_path_cmd(const json_value *data, geojson_geometry_t *geo,
                          int *allocated)
{
    json_value *e;
    int nb_args;
    double args[6];
    double (*last)[2];
    char cmd;
    geojson_linestring_t *line = &geo->linestring;

    if (!data || data->type != json_array) goto error;
    if (data->u.array.length == 0) goto error;
    e = data->u.array.values[0];
    if (e->type != json_string || e->u.string.length == 0) goto error;
    cmd = e->u.string.ptr[0];

    switch (cmd) {
    case 'M': nb_args = 2; break;
    case 'L': nb_args = 2; break;
    case 'C': nb_args = 6; break;
    default: goto error;
    }
    if (parse_float_array(data, 1, nb_args, args)) goto error;

    switch (cmd) {
    case 'M': // Moveto.
        coordinates_push(line, args, allocated);
    case 'L': // Lineto (same for the moment!);
        coordinates_push(line, args, allocated);
        break;
    case 'C':
        last = &line->coordinates[line->size - 1];
        tesselate_bezier(line, *last, args, args + 2, args + 4, 0,
                         allocated);
        break;
    default:
        goto error;
    }

    return 0;
error:
    LOG_W("Error parsing geojson path");
    return -1;

}

static int parse_linestring_coordinates(const json_value *coordinates,
                                        geojson_linestring_t *linestring)
{
    const json_value *point;
    int i;
    assert(coordinates->type == json_array);
    linestring->size = coordinates->u.array.length;
    linestring->coordinates = calloc(linestring->size,
                                     sizeof(*linestring->coordinates));

    for (i = 0; i < linestring->size; i++) {
        point = coordinates->u.array.values[i];
        if (point->type != json_array) return -1;
        parse_float_array(point, 0, 2, linestring->coordinates[i]);
    }
    return 0;
}

static int parse_linestring(const json_value *data,
                            geojson_linestring_t *linestring)
{
    const json_value *coordinates;
    coordinates = json_get_attr(data, "coordinates", json_array);
    if (!coordinates) return -1;
    return parse_linestring_coordinates(coordinates, linestring);
}

static int parse_polygon(const json_value *data, geojson_polygon_t *poly)
{
    const json_value *coordinates, *ring;
    int i;

    if (!data) return -1;
    if (data->type == json_array)
        coordinates = data;
    else
        coordinates = json_get_attr(data, "coordinates", json_array);
    if (!coordinates) return -1;
    poly->size = coordinates->u.array.length;
    poly->rings = calloc(poly->size, sizeof(*poly->rings));
    for (i = 0; i < poly->size; i++) {
        ring = coordinates->u.array.values[i];
        if (parse_linestring_coordinates(ring, &poly->rings[i])) return -1;
    }
    return 0;
}

static int parse_multipolygon(const json_value *data,
                              geojson_multipolygon_t *multipoly)
{
    const json_value *coordinates;
    int i;

    coordinates = json_get_attr(data, "coordinates", json_array);
    if (!coordinates) return -1;
    multipoly->size = coordinates->u.array.length;
    multipoly->polygons = calloc(multipoly->size,
                                 sizeof(*multipoly->polygons));
    for (i = 0; i < multipoly->size; i++) {
        if (parse_polygon(coordinates->u.array.values[i],
                          &multipoly->polygons[i])) return -1;
    }
    return 0;
}

static int parse_point(const json_value *data, geojson_point_t *point)
{
    const json_value *coordinates;
    coordinates = json_get_attr(data, "coordinates", json_array);
    if (parse_float_array(coordinates, 0, 2, point->coordinates))
        return -1;
    return 0;
}

/*
 * Function: parse_path
 * Parse a path into a geometry instance.
 */
static int parse_path(const json_value *data, geojson_geometry_t *geo)
{
    const json_value *path;
    int i, allocated = 0;

    geo->type = GEOJSON_LINESTRING;
    path = json_get_attr(data, "path", json_array);
    if (!path) return -1;
    for (i = 0; i < path->u.array.length; i++) {
        if (parse_path_cmd(path->u.array.values[i], geo, &allocated))
            return -1;
    }
    return 0;
}

static void lonlat2c(const double lonlat[2], double c[3])
{
    eraS2c(lonlat[0] * ERFA_DD2R, lonlat[1] * ERFA_DD2R, c);
}

static void c2lonlat(const double c[3], double lonlat[2])
{
    double lon, lat;
    eraC2s(c, &lon, &lat);
    lonlat[0] = lon * ERFA_DR2D;
    lonlat[1] = lat * ERFA_DR2D;
}

/*
 * Function: tesselate_circle
 * Convert a circle into a list of lon/lat points.
 */
static int tesselate_circle(const double center[2], double r, int size,
                            double (*out)[2])
{
    double start[3], axis[3], up[3], a, quat[4], p[3];
    int i;

    lonlat2c(center, axis);
    vec3_get_ortho(axis, up);
    quat_from_axis(quat, r, up[0], up[1], up[2]);
    quat_mul_vec3(quat, axis, start);

    for (i = 0; i < size; i++) {
        a = i * 2 * M_PI / (size - 1);
        quat_from_axis(quat, a, axis[0], axis[1], axis[2]);
        quat_mul_vec3(quat, start, p);
        c2lonlat(p, out[i]);
    }

    return 0;
}

/*
 * Function: parse_circle
 * Parse a circle feature into a geometry instance.
 */
static int parse_circle(const json_value *data, geojson_geometry_t *geo)
{
    double r, center[2];
    int size = 64; // Make it an argument?
    geojson_linestring_t *ring;

    geo->type = GEOJSON_POLYGON;
    geo->polygon.size = 1;
    geo->polygon.rings = calloc(1, sizeof(*geo->polygon.rings));

    ring = &geo->polygon.rings[0];

    ring->size = size;
    ring->coordinates = calloc(size, sizeof(*ring->coordinates));

    r = json_get_attr_f(data, "radius", -1);
    if (r == -1) return -1;
    if (parse_float_array(
                json_get_attr(data, "center", json_array), 0, 2, center)) {
        LOG_W("Cannot parse circle center");
        return -1;
    }

    tesselate_circle(center, r * ERFA_DD2R, size, ring->coordinates);
    return 0;
}

static int parse_anchor(const char *str)
{
    if (!str) return GEOJSON_ANCHOR_CENTER | GEOJSON_ANCHOR_MIDDLE;
    if (strcmp(str, "left") == 0)
        return GEOJSON_ANCHOR_LEFT | GEOJSON_ANCHOR_MIDDLE;
    if (strcmp(str, "center") == 0)
        return GEOJSON_ANCHOR_CENTER | GEOJSON_ANCHOR_MIDDLE;
    if (strcmp(str, "right") == 0)
        return GEOJSON_ANCHOR_RIGHT | GEOJSON_ANCHOR_MIDDLE;
    if (strcmp(str, "top") == 0)
        return GEOJSON_ANCHOR_CENTER | GEOJSON_ANCHOR_TOP;
    if (strcmp(str, "bottom") == 0)
        return GEOJSON_ANCHOR_CENTER | GEOJSON_ANCHOR_BOTTOM;
    if (strcmp(str, "top-left") == 0)
        return GEOJSON_ANCHOR_LEFT | GEOJSON_ANCHOR_TOP;
    if (strcmp(str, "top-right") == 0)
        return GEOJSON_ANCHOR_RIGHT | GEOJSON_ANCHOR_TOP;
    if (strcmp(str, "bottom-left") == 0)
        return GEOJSON_ANCHOR_LEFT | GEOJSON_ANCHOR_BOTTOM;
    if (strcmp(str, "bottom-right") == 0)
        return GEOJSON_ANCHOR_RIGHT | GEOJSON_ANCHOR_BOTTOM;

    LOG_W("Wrong anchor value: %s", str);
    return 0;
}

static int parse_properties(const json_value *data,
                            geojson_feature_properties_t *props)
{
    const char *title;
    const json_value *v;
    double text_offset[2];
    char error_msg[128] = "";

    if (!data) return 0;
    parse_color(json_get_attr(data, "stroke", 0), props->stroke);
    parse_color(json_get_attr(data, "fill", 0), props->fill);
    props->stroke_width = json_get_attr_f(data, "stroke-width", 1);
    props->stroke_opacity = json_get_attr_f(data, "stroke-opacity", 1);
    props->stroke_glow = json_get_attr_b(data, "stroke-glow", false);
    props->fill_opacity = json_get_attr_f(data, "fill-opacity", 0.5);
    if ((title = json_get_attr_s(data, "title")))
        props->title = strdup(title);
    props->text_anchor = parse_anchor(json_get_attr_s(data, "text-anchor"));
    props->text_size = json_get_attr_i(data, "text-size", -1);
    props->text_rotate = -json_get_attr_f(data, "text-rotate", 0) * ERFA_DD2R;
    if ((v = json_get_attr(data, "text-offset", 0))) {
        if (parse_float_array(v, 0, 2, text_offset))
            ERROR("Can't parse text-offset");
        vec2_copy(text_offset, props->text_offset);
    }
    return 0;
error:
    LOG_W("Error parsing geojson properties: %s", error_msg);
    return -1;
}

/*
 * Function: parse_feature
 * Parse a single geojson feature.
 */
static int parse_feature(const json_value *data, geojson_feature_t *feature)
{
    const json_value *geometry, *properties;
    const char *type;
    char *json;
    char error_msg[128] = "";
    geojson_geometry_t *geo;

    geo = &feature->geometry;
    geometry = json_get_attr(data, "geometry", json_object);
    if (!geometry) ERROR("Missing 'geometry' attribute");

    type = json_get_attr_s(geometry, "type");
    if (!type) ERROR("Missing 'type' attribute");

    if (strcmp(type, "Polygon") == 0) {
        geo->type = GEOJSON_POLYGON;
        if (parse_polygon(geometry, &geo->polygon))
            ERROR("Cannot parse polygon");
    } else if (strcmp(type, "MultiPolygon") == 0) {
        geo->type = GEOJSON_MULTIPOLYGON;
        if (parse_multipolygon(geometry, &geo->multipolygon))
            ERROR("Cannot parse MultiPolygon");
    } else if (strcmp(type, "LineString") == 0) {
        geo->type = GEOJSON_LINESTRING;
        if (parse_linestring(geometry, &geo->linestring))
            ERROR("Cannot parse LineString");
    } else if (strcmp(type, "Point") == 0) {
        geo->type = GEOJSON_POINT;
        if (parse_point(geometry, &geo->point))
            ERROR("Cannot parse Point");
    } else if (strcmp(type, "Path") == 0) {
        if (parse_path(geometry, geo))
            ERROR("Cannot parse Path");
    } else if (strcmp(type, "Circle") == 0) {
        if (parse_circle(geometry, geo))
            ERROR("Cannot parse Circle");
    } else {
        ERROR("Unknown geojson type: %s", type);
    }

    vec3_set(feature->properties.fill, 1, 1, 1);
    vec3_set(feature->properties.stroke, 1, 1, 1);
    feature->properties.stroke_width = 1;
    feature->properties.stroke_opacity = 1;
    feature->properties.fill_opacity = 0.5;
    properties = json_get_attr(data, "properties", json_object);
    if (parse_properties(properties, &feature->properties)) goto error;

    return 0;
error:
    LOG_W("Error parsing geojson feature: %s", error_msg);
    json = malloc(json_measure(data));
    json_serialize(json, data);
    LOG_W("json:\n%s", json);
    free(json);
    return -1;
}

/*
 * Function: geojson_parse
 * Parse a geojson into a structure.
 *
 * Parameters:
 *   data   - The json data to parse.
 *
 * Return:
 *   A new geojson_t instance, or NULL in case of error.
 */
geojson_t *geojson_parse(const json_value *data)
{
    char error_msg[128] = "";
    const char *type;
    type = json_get_attr_s(data, "type");
    const json_value *features;
    int i;
    geojson_t *geojson = calloc(1, sizeof(*geojson));

    if (!type) ERROR("Cannot find 'type' attribute");
    if (strcmp(type, "FeatureCollection") == 0) {
        features = json_get_attr(data, "features", json_array);
        if (!features) ERROR("Missing 'features' attribute");
        geojson->nb_features = features->u.array.length;
        geojson->features = calloc(geojson->nb_features,
                                   sizeof(*geojson->features));
        for (i = 0; i < features->u.array.length; i++) {
            if (parse_feature(features->u.array.values[i],
                              &geojson->features[i]))
                ERROR("Cannot parse feature");
        }
    } else if (strcmp(type, "Feature") == 0) {
        geojson->nb_features = 1;
        geojson->features = calloc(1, sizeof(*geojson->features));
        if (parse_feature(data, &geojson->features[0]))
            ERROR("Cannot parse feature");
    } else {
        ERROR("type %s not supported", type);
    }

    return geojson;

error:
    LOG_W("Error parsing geojson: %s", error_msg);
    geojson_delete(geojson);
    return NULL;
}

/*
 * Function: geojson_delete
 * Delete a geojson_t instance created with <geojson_parse>.
 */
void geojson_delete(geojson_t *geojson)
{
    int i, j, k;
    geojson_feature_t *feature;
    geojson_geometry_t *geo;

    if (!geojson) return;

    for (i = 0; i < geojson->nb_features; i++) {
        feature = &geojson->features[i];
        free(feature->properties.title);
        geo = &feature->geometry;
        switch (geo->type) {
        case GEOJSON_LINESTRING:
            free(geo->linestring.coordinates);
            break;
        case GEOJSON_POLYGON:
            for (j = 0; j < geo->polygon.size; j++)
                free(geo->polygon.rings[j].coordinates);
            free(geo->polygon.rings);
            break;
        case GEOJSON_MULTIPOLYGON:
            for (j = 0; j < geo->multipolygon.size; j++) {
                for (k = 0; k < geo->multipolygon.polygons[j].size; k++) {
                    free(geo->multipolygon.polygons[j].rings[k].coordinates);
                }
                free(geo->multipolygon.polygons[j].rings);
            }
            free(geo->multipolygon.polygons);
            break;
        default:
            break;
        }
    }
    free(geojson->features);
    free(geojson);
}
