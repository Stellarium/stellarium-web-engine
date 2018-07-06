/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

/*
 * Module that handles the skycultures list.
 */


/*
 * Type: skyculture_t
 * Represent an individual skyculture.
 */
typedef struct skyculture {
    obj_t           obj;
    char            *uri;
    int             nb_constellations;
    char            *(*names)[2]; // [ID, Name].  NULL terminated.
    constellation_infos_t *constellations;
} skyculture_t;

static obj_klass_t skyculture_klass = {
    .id     = "skyculture",
    .size   = sizeof(skyculture_t),
    .flags  = 0,
};

OBJ_REGISTER(skyculture_klass)

// Module class.
typedef struct skycultures_t {
    obj_t   obj;
} skycultures_t;

static int skycultures_init(obj_t *obj, json_value *args);

static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = skycultures_init,
    .create_order   = 30, // After constellations.
};

OBJ_REGISTER(skycultures_klass)


void skyculture_activate(skyculture_t *cult)
{
    char id[32];
    int i;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations;

    // Add all the names.
    for (i = 0; cult->names[i][0]; i++) {
        identifiers_add(cult->names[i][0], "NAME", cult->names[i][1],
                        NULL, NULL);
    }

    // Create all the constellations object.
    constellations = obj_get(NULL, "constellations", 0);
    assert(constellations);
    for (i = 0; i < cult->nb_constellations; i++) {
        cst = &cult->constellations[i];
        args = json_object_new(0);
        json_object_push(args, "info_ptr", json_integer_new((int64_t)cst));
        sprintf(id, "CST %s", cst->id);
        obj_create("constellation", id, constellations, args);
        json_value_free(args);
    }
}

static int skycultures_init(obj_t *obj, json_value *args)
{
    // For the moment we manually create and activate the western culture.
    skyculture_t *cult;
    char path[1024];
    const char *constellations, *edges, *names;
    int nb;
    const char *uri = "asset://skycultures/western";

    cult = (void*)obj_create("skyculture", "CULTURE western", obj, NULL);
    cult->uri = strdup(uri);

    sprintf(path, "%s/%s", cult->uri, "names.txt");
    names = asset_get_data(path , NULL, NULL);
    assert(names);
    nb = skyculture_parse_names(names, NULL);
    cult->names = calloc(nb + 1, sizeof(*cult->names));
    skyculture_parse_names(names, cult->names);

    sprintf(path, "%s/%s", uri, "constellations.txt");
    constellations = asset_get_data(path, NULL, NULL);
    sprintf(path, "%s/%s", uri, "edges.txt");
    edges = asset_get_data(path, NULL, NULL);
    assert(constellations);

    cult->constellations = skyculture_parse_constellations(
            constellations, edges, &cult->nb_constellations);

    skyculture_activate(cult);

    return 0;
}

