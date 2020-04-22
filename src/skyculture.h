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
 * Type: constellation_infos_t
 * Information about a given constellation.
 */
typedef struct constellation_infos
{
    char id[128];
    int  lines[64][2]; // star HIP number.
    int  nb_lines;
    double edges[64][2][2]; // Ra/dec B1875 boundaries polygon.
    int nb_edges;
    char *description;
    char iau[8]; // IAU abbreviation. Zero padded.
} constellation_infos_t;

/*
 * Type: constellation_art_t
 * Information about a constellation image.
 */
typedef struct constellation_art
{
    char cst[128];  // Id of the constellation.
    char img[128];  // Name of the image file.
    struct {
        double  uv[2]; // Texture UV position.
        int     hip;   // Star HIP.
    } anchors[3];
} constellation_art_t;

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
} skyculture_name_t;


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

int skyculture_parse_feature_art_json(const json_value *v,
                                      constellation_art_t *art);

skyculture_name_t *skyculture_parse_names_json(const json_value *v);

#endif // SKYCULTURE_H
