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
 * Enum of all the data files we need to parse.
 */
enum {
    SK_INFO           = 1 << 0,
    SK_EDGES          = 1 << 5,

    SK_CONSTELLATIONS_STEL          = 1 << 6,
    SK_CONSTELLATION_NAMES_STEL     = 1 << 7,
    SK_IMGS_STEL                    = 1 << 8,
    SK_STAR_NAMES_STEL              = 1 << 9,
    SK_DESCRIPTION_STEL             = 1 << 10,
};

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
    skyculture_name_t *names; // Hash table of oid -> names.
    constellation_infos_t *constellations;
    json_value      *imgs;
    int             parsed; // union of SK_ enum for each parsed file.
    char            *description;  // html description if any.
} skyculture_t;

/*
 * Type: skycultures_t
 * The module, that maintains the list of skycultures.
 */
typedef struct skycultures_t {
    obj_t   obj;
    skyculture_t *current; // The current skyculture.
    int     labels_display_style;
} skycultures_t;



static void skyculture_deactivate(skyculture_t *cult)
{
    obj_t *constellations, *cst, *tmp;
    // Remove all the constellation objects.
    constellations = core_get_module("constellations");
    assert(constellations);
    DL_FOREACH_SAFE(constellations->children, cst, tmp) {
        if (str_startswith(cst->id, "CST "))
            module_remove(constellations, cst);
    }
}

// Defined in constellations.c
int constellation_set_image(obj_t *obj, const json_value *args);

static void skyculture_activate(skyculture_t *cult)
{
    char id[32];
    int i;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations, *cons;

    // Create all the constellations object.
    constellations = core_get_module("constellations");
    assert(constellations);
    for (i = 0; i < cult->nb_constellations; i++) {
        cst = &cult->constellations[i];
        sprintf(id, "CST %s", cst->id);
        cons = obj_get(constellations, id, 0);
        if (cons) {
            obj_release(cons);
            continue;
        }
        args = json_object_new(0);
        json_object_push(args, "info_ptr", json_integer_new((int64_t)cst));
        obj_create("constellation", id, constellations, args);
        json_builder_free(args);
    }

    // Add the images.
    if (cult->imgs) {
        for (i = 0; i < cult->imgs->u.array.length; i++) {
            args = cult->imgs->u.array.values[i];
            sprintf(id, "CST %s", json_get_attr_s(args, "id"));
            cons = obj_get(constellations, id, 0);
            if (!cons) continue;
            constellation_set_image(cons, args);
            obj_release(cons);
        }
    }

    // Set the current attribute of the skycultures manager object.
    obj_set_attr(cult->obj.parent, "current", cult);
    module_changed(cult->obj.parent, "current_id");
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

static int skyculture_update(obj_t *obj, double dt);
static skyculture_t *add_from_uri(skycultures_t *cults, const char *uri,
                                  const char *id)
{
    skyculture_t *cult;
    cult = (void*)obj_create("skyculture", id, (obj_t*)cults, NULL);
    cult->uri = strdup(uri);
    skyculture_update((obj_t*)cult, 0);
    return cult;
}

/*
 * Convert an array of constellation_art_t values into a json in the same
 * format as recognised by the constellation module, that is an array of
 * dict similar to this one:
 *
 * {
 *   "anchors": "198 215 3881 337 136 3092 224 428 9640",
 *   "id": "And",
 *   "img": "And.webp",
 *   "type": "constellation"
 *   "uv_in_pixel": true,
 *   "base_path": "asset://skycultures/western/img"
 *  }
 *
 */
static json_value *make_imgs_json(
        const constellation_art_t *imgs, const char *uri)
{
    json_value *values, *v;
    constellation_art_t *a;
    char anchors[1024];
    values = json_array_new(0);
    for (a = imgs; *a->cst; a++) {
        v = json_object_new(0);
        json_object_push(v, "id", json_string_new(a->cst));
        json_object_push(v, "img", json_string_new(a->img));
        json_object_push(v, "type", json_string_new("constellation"));
        json_object_push(v, "base_path", json_string_new(uri));
        sprintf(anchors, "%f %f %d %f %f %d %f %f %d",
            a->anchors[0].uv[0], a->anchors[0].uv[1], a->anchors[0].hip,
            a->anchors[1].uv[0], a->anchors[1].uv[1], a->anchors[1].hip,
            a->anchors[2].uv[0], a->anchors[2].uv[1], a->anchors[2].hip);
        json_object_push(v, "anchors", json_string_new(anchors));
        json_object_push(v, "uv_in_pixel", json_boolean_new(a->uv_in_pixel));
        json_array_push(values, v);
    }
    return values;
}

/*
 * Function: get_file
 * Convenience function to get a data file.
 *
 * This only return true the first time the file is retreive.  After that
 * the file_id flag is set to cult->parsed and the file is never read
 * again.
 */
static bool get_file(skyculture_t *cult, int file_id, const char *name,
                     const char **data, int extra_flags)
{
    char path[1024];
    int code;
    if (cult->parsed & file_id) return false;
    snprintf(path, sizeof(path) - 1, "%s/%s", cult->uri, name);
    *data = asset_get_data2(path, ASSET_USED_ONCE | extra_flags, NULL, &code);
    if (!code) return false;
    cult->parsed |= file_id;
    return *data;
}

static int skyculture_update(obj_t *obj, double dt)
{
    skyculture_t *cult = (skyculture_t*)obj;
    const char *data;
    constellation_art_t *arts;

    if (get_file(cult, SK_INFO, "info.ini", &data, 0)) {
        ini_parse_string(data, info_ini_handler, cult);
    }

    if (get_file(cult, SK_DESCRIPTION_STEL, "description.en.utf8",
                 &data, ASSET_ACCEPT_404))
    {
        cult->description = strdup(data);
        module_changed((obj_t*)cult, "description");
    }

    if (get_file(cult, SK_CONSTELLATIONS_STEL, "constellationship.fab",
                 &data, 0))
    {
        cult->constellations = skyculture_parse_stellarium_constellations(
                data, &cult->nb_constellations);
    }

    if (cult->constellations && get_file(cult, SK_CONSTELLATION_NAMES_STEL,
                  "constellation_names.eng.fab", &data, 0))
    {
        skyculture_parse_stellarium_constellations_names(
                data, cult->constellations);
    }

    if (get_file(cult, SK_STAR_NAMES_STEL, "star_names.fab", &data,
                 ASSET_ACCEPT_404))
    {
        cult->names = skyculture_parse_stellarium_star_names(data);
    }

    if (cult->constellations &&
            get_file(cult, SK_EDGES, "edges.txt", &data, ASSET_ACCEPT_404)) {
        skyculture_parse_edges(data, cult->constellations);
    }

    if (cult->constellations && get_file(cult, SK_IMGS_STEL,
                "constellationsart.fab", &data, ASSET_ACCEPT_404))
    {
        arts = skyculture_parse_stellarium_constellations_art(data, NULL);
        if (arts) cult->imgs = make_imgs_json(arts, cult->uri);
        free(arts);
    }

    return 0;
}

static void skycultures_gui(obj_t *obj, int location)
{
    skyculture_t *cult;
    skycultures_t *cults = (void*)obj;
    bool active = false;
    bool res;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Skycultures")) {
        MODULE_ITER(obj, cult, "skyculture") {
            if (!cult->info.name) continue;
            if (strcmp(cult->info.name, cults->current->info.name) == 0) {
                active = true;
            } else {
                active = false;
            }
            res = gui_toggle(cult->info.name, &active);
            if (res) {
                obj_set_attr((obj_t*)cults, "current_id", cult->info.name);
            }
        }
        gui_tab_end();
    }
}

static int skycultures_update(obj_t *obj, double dt)
{
    obj_t *skyculture;
    MODULE_ITER(obj, skyculture, "skyculture") {
        skyculture_update(skyculture, dt);
    }
    return 0;
}

static int skycultures_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    const char *key;
    skycultures_t *cults = (void*)obj;
    skyculture_t *cult;

    if (!type || strcmp(type, "skyculture") != 0) return 1;
    key = strrchr(url, '/') + 1;
    // Skip if we already have it.
    if (module_get_child(&cults->obj, key)) return 0;
    cult = add_from_uri(cults, url, key);
    if (!cult) LOG_W("Cannot add skyculture (%s)", url);
    // If it's the default skyculture (western) activate it immediatly.
    if (str_endswith(url, "western"))
        obj_set_attr((obj_t*)cults, "current_id", cult->info.name);
    return 0;
}

/*
 * Function: skyculture_get_name
 * Get the name of a star in the current skyculture.
 *
 * Parameters:
 *   skycultures    - A skyculture module.
 *   oid            - Object id of a star.
 *   buf            - A text buffer that get filled with the name.
 *
 * Return:
 *   NULL if no name was found.  A pointer to the passed buffer otherwise.
 */
const char *skycultures_get_name(obj_t *skycultures, uint64_t oid,
                                 char buf[128])
{
    skyculture_t *cult;
    skyculture_name_t *entry;
    if (!skycultures) return NULL;
    assert(strcmp(skycultures->klass->id, "skycultures") == 0);
    cult = ((skycultures_t*)skycultures)->current;
    if (!cult) return NULL;
    HASH_FIND(hh, cult->names, &oid, sizeof(oid), entry);
    if (!entry) return NULL;
    strcpy(buf, entry->name);
    return buf;
}

// Set/Get the current skyculture by id.
static json_value *skycultures_current_id_fn(
        obj_t *obj, const attribute_t *attr, const json_value *args)
{
    char id[128];
    skycultures_t *cults = (skycultures_t*)obj;
    skyculture_t *cult;
    if (args && args->u.array.length) {
        args_get(args, NULL, 1, TYPE_STRING, id);
        if (cults->current) {
            MODULE_ITER(cults, cult, "skyculture") {
                if (strcmp(cult->info.name, cults->current->info.name) == 0) {
                    skyculture_deactivate(cult);
                    break;
                }
            }
        }
        MODULE_ITER(cults, cult, "skyculture") {
            if (strcmp(cult->info.name, id) == 0) {
                skyculture_activate(cult);
                break;
            }
        }
    }
    return args_value_new(TYPE_STRING, cults->current->info.name);
}

/*
 * Meta class declarations.
 */

static obj_klass_t skyculture_klass = {
    .id     = "skyculture",
    .size   = sizeof(skyculture_t),
    .flags  = 0,
    .update = skyculture_update,
    .attributes = (attribute_t[]) {
        PROPERTY(name, TYPE_STRING_PTR, MEMBER(skyculture_t, info.name)),
        PROPERTY(description, TYPE_STRING_PTR,
                 MEMBER(skyculture_t, description)),
        PROPERTY(url, TYPE_STRING_PTR, MEMBER(skyculture_t, uri)),
        {}
    },
};
OBJ_REGISTER(skyculture_klass)


static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .gui            = skycultures_gui,
    .update         = skycultures_update,
    .add_data_source    = skycultures_add_data_source,
    .create_order   = 30, // After constellations.
    .attributes = (attribute_t[]) {
        PROPERTY(current, TYPE_OBJ, MEMBER(skycultures_t, current)),
        PROPERTY(current_id, TYPE_STRING, .fn = skycultures_current_id_fn),
        {}
    },
};
OBJ_REGISTER(skycultures_klass)
