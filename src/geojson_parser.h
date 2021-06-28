/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef GEOJSON_H
#define GEOJSON_H

#include "json.h"

#include <stdbool.h>

/*
 * Simple geojson parser.
 *
 * The <geojson_parse> function takes a json_value and return a <geojson_t>
 * instance that matches the structure of the data, or NULL in case of
 * error.
 *
 * All the features can have a 'properties' attribute with the style,
 * same as used in http://geojson.io, eg:
 *
 *    "properties": {
 *      "stroke": "#c73737",
 *      "stroke-width": 2,
 *      "stroke-opacity": 1,
 *      "fill": "#555555",
 *      "fill-opacity": 0.5
 *    }
 *
 * We add support for two new geojson features: Circle and Path, that can
 * be used to render curved lines.  Both get automatically converted into
 * polygon or linestring.
 *
 * Circles are defined with a center and radius:
 *
 *   "geometry": {
 *     "type": "Circle",
 *     "center": [20, 0],
 *     "radius": 10
 *   }
 *
 * Paths are defined using a subset of SVG path (only M and C commands for the
 * moment):
 *
 *   "geometry": {
 *      "type": "Path","
 *      "path": ["
 *        ["M", 0, 0],"
 *        ["C", 10.0, 0.0, 10.0, 10.0, 0.0, 10.0]"
 *      ]
 *    }
 *
 * Point features accept a few extra properties:
 *
 *   title        - A string.
 *   text-anchor  - One of "left", "center", "right", "top", "bottom",
 *                  "top-left", "top-right", "bottom-left", "bottom-right".
 *   text-offset  - [x, y] offset in pixels.
 *   text-rotate  - rotation angle in degrees.
 */

enum {
    GEOJSON_POLYGON,
    GEOJSON_MULTIPOLYGON,
    GEOJSON_LINESTRING,
    GEOJSON_POINT,
};

/*
 * Enum: GEOJSON_ANCHOR
 * Used in the text_anchor property.
 *
 * Same as nanovg.
 */
enum {
    GEOJSON_ANCHOR_LEFT     = 1 << 0,
    GEOJSON_ANCHOR_CENTER   = 1 << 1,
    GEOJSON_ANCHOR_RIGHT    = 1 << 2,
    GEOJSON_ANCHOR_TOP      = 1 << 3,
    GEOJSON_ANCHOR_MIDDLE   = 1 << 4,
    GEOJSON_ANCHOR_BOTTOM   = 1 << 5,
};

typedef struct geojson_feature_properties
{
    float stroke[3];
    float stroke_width;
    float stroke_opacity;
    bool stroke_glow; // to remove?
    float fill[3];
    float fill_opacity;
    char *title;
    int text_anchor;
    float text_rotate;
    int text_size;
    float text_offset[2];
} geojson_feature_properties_t;

typedef struct
{
    int size;
    double (*coordinates)[2];
} geojson_linestring_t;

typedef struct
{
    double coordinates[2];
} geojson_point_t;

typedef struct
{
    int size;
    geojson_linestring_t *rings;
} geojson_polygon_t;

typedef struct
{
    int size;
    geojson_polygon_t *polygons;
} geojson_multipolygon_t;

typedef struct geojson_geometry
{
    int type;
    union {
        geojson_linestring_t linestring;
        geojson_polygon_t polygon;
        geojson_multipolygon_t multipolygon;
        geojson_point_t point;
    };
} geojson_geometry_t;

typedef struct geojson_feature geojson_feature_t;
struct geojson_feature
{
    geojson_feature_properties_t properties;
    geojson_geometry_t geometry;
};

typedef struct geojson {
    int                 nb_features;
    geojson_feature_t   *features;
} geojson_t;


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
geojson_t *geojson_parse(const json_value *data);

/*
 * Function: geojson_delete
 * Delete a geojson_t instance created with <geojson_parse>.
 */
void geojson_delete(geojson_t *geojson);

#endif // GEOJSON_H
