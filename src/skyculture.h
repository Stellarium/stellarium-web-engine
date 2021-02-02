/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: Skyculture
 *
 * Some basic functions to parse skyculture data files.
 * Still experimental, probably going to change.
 *
 * The actual skyculture struct is in src/modules/skycultures.c
 */

#ifndef SKYCULTURE_H
#define SKYCULTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "json.h"
#include "uthash.h"

/*
 * Weight used for displaying constellation lines.
 */
enum {
    LINE_WEIGHT_NORMAL    = 0,
    LINE_WEIGHT_THIN      = 1,
    LINE_WEIGHT_BOLD      = 2
};

typedef struct constellation_line
{
    int hip[2];
    uint8_t line_weight;
} constellation_line_t;

/*
 * Type: constellation_infos_t
 * Information about a given constellation.
 */
typedef struct constellation_infos
{
    char id[128];
    constellation_line_t lines[64];
    int  nb_lines;
    double edges[64][2][2]; // Ra/dec B1875 boundaries polygon.
    int nb_edges;
    char *description;
    char iau[8]; // IAU abbreviation. Zero padded.
    char img[128];  // Name of the image file.
    struct {
        double  uv[2]; // Texture UV position.
        int     hip;   // Star HIP.
    } anchors[3];
    const char *base_path;
} constellation_infos_t;

/*
 * Type: skyculture_name
 * Structure to hold hash table of object name and oid.
 *
 * Used as result of skyculture names file parsing.
 */
typedef struct skyculture_name
{
    UT_hash_handle  hh;
    // The ID to use when calling sky_cultures_get_name:
    //   - for bright stars use "HIP XXXX"
    //   - for constellations use "CON culture_name XXX"
    //   - for planets use "NAME Planet"
    //   - for DSO use the first identifier of the names list
    char           *main_id;
    char           *name_english;
    char           *name_native;
    char           *name_pronounce;
    char           *name_description;
    // Pointer to a secondary name, or NULL
    struct skyculture_name* alternative;
} skyculture_name_t;

/*
 * Type: cultural_name
 * Structure to hold information about a cultural name.
 */
typedef struct cultural_name
{
    // The english cultural name e.g. 'Great Bear'
    char           *name_english;
    // The translated version of this cultural name. It is translated according
    // to the current user language as defined in sys_get_lang()
    char           *name_translated;
    // The native name using native spelling, e.g. for western constellation
    // it's the latin name like 'Ursa Major'. For Chinese constellations it's
    // the chinese name like '座旗'
    // other
    char           *name_native;
    // Contains the pronounciation of the native name if it's not ascii, e.g.
    // if native is '座旗', name_pronounce will be the pidgin 'Zuòqí'
    char           *name_pronounce;
    // True if the user prefers to see the native names instead of the
    // translated ones. For example english speaker prefer to see the latin
    // native constellation names like 'Ursa Major' instead of 'Great Bear'
    bool           user_prefer_native;
} cultural_name_t;

/*
 * Function: skyculture_parse_edge
 * Parse constellation edges.
 *
 * Parameters:
 *   edges  - json array of the edges (see western skyculture json).
 *   infos  - Constellation info to update with the edge data.
 *   size   - Size of the infos array.
 *
 * Return:
 *   The number of lines parsed, or -1 in case of error.
 */
int skyculture_parse_edges(const json_value *edges,
                           constellation_infos_t *infos, int size);


// Experimental parsing functions for the new json format.

int skyculture_parse_feature_json(skyculture_name_t **names_hash,
                                  const json_value *v,
                                  constellation_infos_t *feature);

skyculture_name_t *skyculture_parse_names_json(const json_value *v);

#endif // SKYCULTURE_H
