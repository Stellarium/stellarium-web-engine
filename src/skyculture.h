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

/*
 * Type: constellation_infos_t
 * Information about a given constellation.
 */
typedef struct constellation_infos
{
    char id[8];
    char name[128];
    int  lines[64][2]; // star HIP number.
    int  nb_lines;
    double edges[64][2][2]; // Ra/dec B1875 boundaries polygon.
    int nb_edges;
} constellation_infos_t;


/*
 * Function: skyculture_parse_names
 * Parse a skyculture star names file.
 */
int skyculture_parse_names(const char *data, char *(*names)[2]);

/*
 * Function: skyculture_parse_constellations
 * Parse a skyculture constellation file.
 */
constellation_infos_t *skyculture_parse_constellations(
        const char *consts, int *nb);

/*
 * Function: skyculture_parse_edge
 * Parse a constellation edge file.
 *
 * Parameters:
 *   data   - Text data in the edge file format.
 *   infos  - Constellation info to update with the edge data.
 *
 * Return:
 *   The number of edges parsed, or -1 in case of error.
 */
int skyculture_parse_edges(const char *data, constellation_infos_t *infos);

/*
 * Function: skyculture_parse_stellarium_constellations
 * Parse a skyculture constellation file in stellarium format.
 */
constellation_infos_t *skyculture_parse_stellarium_constellations(
        const char *consts, int *nb);

/*
 * Function: skyculture_parse_stellarium_constellations_names
 * Parse a 'constellation_names.fab' file.
 *
 * Parameters:
 *   data   - Text data in the fab file format.
 *   infos  - Constellation info to update with the names.
 *
 * Return:
 *   The number of names parsed, or -1 in case of error.
 */
int skyculture_parse_stellarium_constellations_names(
        const char *data, constellation_infos_t *infos);

#endif // SKYCULTURE_H
