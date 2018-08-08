/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "ini.h"

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
    struct  {
        char        *name;
        char        *author;
    } info;
    int             nb_constellations;
    char            *(*names)[2]; // [ID, Name].  NULL terminated.
    constellation_infos_t *constellations;
    bool            active;
} skyculture_t;

static void skyculture_on_active_changed(
        obj_t *obj, const attribute_t *attr);

static obj_klass_t skyculture_klass = {
    .id     = "skyculture",
    .size   = sizeof(skyculture_t),
    .flags  = 0,
    .attributes = (attribute_t[]) {
        PROPERTY("active", "b", MEMBER(skyculture_t, active),
                 .on_changed = skyculture_on_active_changed),
        {}
    },
};

OBJ_REGISTER(skyculture_klass)

// Module class.
typedef struct skycultures_t {
    obj_t   obj;
} skycultures_t;

static int skycultures_init(obj_t *obj, json_value *args);
static void skycultures_gui(obj_t *obj, int location);

static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = skycultures_init,
    .gui            = skycultures_gui,
    .create_order   = 30, // After constellations.
};

OBJ_REGISTER(skycultures_klass)


static void skyculture_activate(skyculture_t *cult)
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

static void skyculture_deactivate(skyculture_t *cult)
{
    obj_t *constellations, *cst, *tmp;
    // Remove all the constellation objects.
    constellations = obj_get(NULL, "constellations", 0);
    assert(constellations);
    DL_FOREACH_SAFE(constellations->children, cst, tmp) {
        if (str_startswith(cst->id, "CST "))
            obj_remove(constellations, cst);
    }
}

static int info_ini_handler(void* user, const char* section,
                            const char* attr, const char* value)
{
    skyculture_t *cult = user;
    if (strcmp(section, "info") == 0) {
        if (strcmp(attr, "name") == 0)
            cult->info.name = strdup(value);
        if (strcmp(attr, "author") == 0)
            cult->info.author = strdup(value);
    }
    return 0;
}

static skyculture_t *add_from_uri(skycultures_t *cults, const char *uri)
{
    skyculture_t *cult;
    char path[1024];
    const char *info, *constellations, *edges, *names;
    int nb;

    cult = (void*)obj_create("skyculture", NULL, (obj_t*)cults, NULL);
    cult->uri = strdup(uri);

    sprintf(path, "%s/%s", cult->uri, "info.ini");
    info = asset_get_data(path, NULL, NULL);
    assert(info);
    ini_parse_string(info, info_ini_handler, cult);
    free(cult->obj.id);
    asprintf(&cult->obj.id, "CULT %s", cult->info.name);

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
    return cult;
}

static void skyculture_on_active_changed(
        obj_t *obj, const attribute_t *attr)
{
    skyculture_t *cult = (void*)obj, *other;

    // Deactivate the others.
    if (cult->active) {
        OBJ_ITER(cult->obj.parent, other, &skyculture_klass) {
            if (other == cult) continue;
            obj_set_attr((obj_t*)other, "active", "b", false);
        }
    }

    if (cult->active) skyculture_activate(cult);
    else skyculture_deactivate(cult);
}

static int skycultures_init(obj_t *obj, json_value *args)
{
    skycultures_t *cults = (void*)obj;
    skyculture_t *western;
    asset_set_alias("https://data.stellarium.org/skycultures",
                    "asset://skycultures");
    // Immediately load western sky culture.
    western = add_from_uri(cults,
            "https://data.stellarium.org/skycultures/western");
    assert(western);
    obj_set_attr((obj_t*)western, "active", "b", true);
    return 0;
}

static void skycultures_gui(obj_t *obj, int location)
{
    skyculture_t *cult;
    if (!DEFINED(SWE_GUI)) return;
    if (gui_tab("Skycultures")) {
        OBJ_ITER(obj, cult, &skyculture_klass) {
            gui_item(&(gui_item_t){
                    .label = cult->info.name,
                    .obj = (obj_t*)cult,
                    .attr = "active"});
        }
        gui_tab_end();
    }
}
