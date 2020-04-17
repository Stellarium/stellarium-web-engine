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
    SK_JSON                         = 1 << 0,
};

/*
 * Type: skyculture_t
 * Represent an individual skyculture.
 */
typedef struct skyculture {
    obj_t           obj;
    char            *uri;
    int             nb_constellations;
    skyculture_name_t *names; // Hash table of identifier -> common names.
    constellation_infos_t *constellations;
    json_value      *imgs;
    int             parsed; // union of SK_ enum for each parsed file.
    json_value      *tour;

    // The following strings are all english text, with a matching translation
    // in the associated sky culture translations files.
    char            *name;         // name
    char            *introduction; // introduction (html)
    char            *description;  // description if any (html)
    char            *references;   // references if any (html)
    char            *authors;      // authors (html)
    char            *licence;      // licence (html)
} skyculture_t;

/*
 * Enum: NAME_FORMAT_STYLE
 * Flags that specify how to format a sky culture common name.
 *
 * Values:
 *   NAME_AUTO          - Return the localized version of the english common
 *                        name, fallback to pronounciation if not available,
 *                        and native if not available.
 *   NAME_NATIVE        - Return the native name if available, e.g. the chinese
 *                        character version of the name, fallback to
 *                        pronounciation if not available.
 *   NAME_NATIVE_AND_PRONOUNCE - Return native name + pronounciation if
 *                        available.
 */
enum {
    NAME_AUTO                  = 0,
    NAME_NATIVE                = 1 << 0,
    NAME_NATIVE_AND_PRONOUNCE  = 1 << 1
};

/*
 * Type: skycultures_t
 * The module, that maintains the list of skycultures.
 */
typedef struct skycultures_t {
    obj_t   obj;
    skyculture_t *current; // The current skyculture.
    int     name_format_style;
} skycultures_t;

// Static instance.
static skycultures_t *g_skycultures = NULL;

static void skyculture_deactivate(skyculture_t *cult)
{
    obj_t *constellations, *cst, *tmp;
    // Remove all the constellation objects.
    constellations = core_get_module("constellations");
    assert(constellations);
    DL_FOREACH_SAFE(constellations->children, cst, tmp) {
        if (str_startswith(cst->id, "CST ")) {
            module_remove(constellations, cst);
        }
    }
}

// Defined in constellations.c
int constellation_set_image(obj_t *obj, const json_value *args);

static void skyculture_activate(skyculture_t *cult)
{
    char id[256];
    int i;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations, *cons;

    // Create all the constellations object.
    constellations = core_get_module("constellations");
    assert(constellations);
    for (i = 0; i < cult->nb_constellations; i++) {
        cst = &cult->constellations[i];
        snprintf(id, sizeof(id), "CST %s", cst->id);
        cons = module_get_child(constellations, id);
        if (cons) {
            module_remove(constellations, cons);
            obj_release(cons);
            continue;
        }
        args = json_object_new(0);
        json_object_push(args, "info_ptr", json_integer_new((int64_t)cst));
        module_add_new(constellations, "constellation", id, args);
        json_builder_free(args);
    }

    // Add the images.
    if (cult->imgs) {
        for (i = 0; i < cult->imgs->u.array.length; i++) {
            args = cult->imgs->u.array.values[i];
            snprintf(id, sizeof(id), "CST %s", json_get_attr_s(args, "id"));
            cons = module_get_child(constellations, id);
            if (!cons) continue;
            constellation_set_image(cons, args);
            obj_release(cons);
        }
    }

    // Set the current attribute of the skycultures manager object.
    obj_set_attr(cult->obj.parent, "current", cult);
    module_changed(cult->obj.parent, "current_id");
}

static int skyculture_update(obj_t *obj, double dt);
static skyculture_t *add_from_uri(skycultures_t *cults, const char *uri,
                                  const char *id)
{
    skyculture_t *cult;
    cult = (void*)module_add_new(&cults->obj, "skyculture", id, NULL);
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
        snprintf(anchors, sizeof(anchors), "%f %f %d %f %f %d %f %f %d",
            a->anchors[0].uv[0], a->anchors[0].uv[1], a->anchors[0].hip,
            a->anchors[1].uv[0], a->anchors[1].uv[1], a->anchors[1].hip,
            a->anchors[2].uv[0], a->anchors[2].uv[1], a->anchors[2].hip);
        json_object_push(v, "anchors", json_string_new(anchors));
        json_array_push(values, v);
    }
    return values;
}

static int skyculture_update(obj_t *obj, double dt)
{
    const char *json;
    skyculture_t *cult = (skyculture_t*)obj;
    skycultures_t *cults = (skycultures_t*)obj->parent;
    char path[1024], *name;
    int code, r, i, arts_nb;
    json_value *doc;
    const json_value *names = NULL, *features = NULL,
                     *tour = NULL, *edges = NULL;
    const json_value *description = NULL, *introduction = NULL,
                     *references = NULL, *authors = NULL, *licence = NULL;
    constellation_art_t *arts;
    bool active = (cult == cults->current);

    if (cult->parsed & SK_JSON)
        return 0;

    snprintf(path, sizeof(path), "%s/%s", cult->uri, "index.json");
    json = asset_get_data2(path, ASSET_USED_ONCE, NULL, &code);
    if (!code) return 0;
    cult->parsed |= SK_JSON;

    if (!json) {
        LOG_E("Failed to download skyculture json file");
        return -1;
    }

    doc = json_parse(json, strlen(json));
    if (!doc) {
        LOG_E("Cannot parse skyculture json (%s)", path);
        return -1;
    }

    r = jcon_parse(doc, "{",
        "name", JCON_STR(name),
        "?common_names", JCON_VAL(names),
        "?constellations", JCON_VAL(features),
        "introduction", JCON_VAL(introduction),
        "?description", JCON_VAL(description),
        "?references", JCON_VAL(references),
        "?authors", JCON_VAL(authors),
        "?licence", JCON_VAL(licence),
        "?edges", JCON_VAL(edges),
        "?tour", JCON_VAL(tour),
    "}");
    if (r) {
        LOG_E("Cannot parse skyculture json (%s)", path);
        json_value_free(doc);
        return -1;
    }

    cult->name = strdup(name);
    cult->introduction = json_to_string(introduction);
    if (description)
        cult->description = json_to_string(description);
    if (references)
        cult->references = json_to_string(references);
    cult->authors = json_to_string(authors);
    cult->licence = json_to_string(licence);
    if (names) cult->names = skyculture_parse_names_json(names);
    if (tour) cult->tour = json_copy(tour);

    if (!features) goto end;
    cult->constellations = calloc(features->u.array.length,
                                  sizeof(*cult->constellations));
    for (i = 0; i < features->u.array.length; i++) {
        r = skyculture_parse_feature_json(
                features->u.array.values[i],
                &cult->constellations[cult->nb_constellations]);
        if (r) continue;
        cult->nb_constellations++;
    }

    // For the moment we parse the art separatly, it should all be merged
    // int a 'feature'.
    arts = calloc(features->u.array.length + 1, sizeof(*arts));
    arts_nb = 0;
    for (i = 0; i < features->u.array.length; i++) {
        r = skyculture_parse_feature_art_json(
                features->u.array.values[i], &arts[arts_nb]);
        if (r) continue;
        arts_nb++;
    }
    memset(&arts[arts_nb], 0, sizeof(arts[arts_nb]));
    if (arts_nb) cult->imgs = make_imgs_json(arts, cult->uri);
    free(arts);

    if (edges) {
        skyculture_parse_edges(edges, cult->constellations,
                               cult->nb_constellations);
    }

    // Once all has beed parsed, we can activate the skyculture.
    if (active) skyculture_activate(cult);

end:
    json_value_free(doc);
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
            if (!cult->name) continue;
            if (strcmp(cult->name, cults->current->name) == 0) {
                active = true;
            } else {
                active = false;
            }
            res = gui_toggle(cult->name, &active);
            if (res) {
                obj_set_attr((obj_t*)cults, "current_id", cult->obj.id);
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

static int skycultures_init(obj_t *obj, json_value *args)
{
    skycultures_t *scs = (skycultures_t*)obj;
    assert(!g_skycultures);
    g_skycultures = scs;
    return 0;
}

static int skycultures_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    skycultures_t *cults = (void*)obj;
    skyculture_t *cult;

    // Skip if we already have it.
    if (module_get_child(&cults->obj, key)) return -1;
    cult = add_from_uri(cults, url, key);
    if (!cult) LOG_W("Cannot add skyculture (%s)", url);
    // If it's the default skyculture (western) activate it immediatly.
    if (str_endswith(url, "western")) {
        obj_set_attr((obj_t*)cults, "current_id", "western");
        skyculture_update((obj_t*)cult, 0);
    }
    return 0;
}

/*
 * Function: skyculture_get_name
 * Get the common name of a sky object in the current skyculture.
 *
 * Parameters:
 *   main_id        - "HIP XXX" for bright stars, anything else for other types.
 *   out            - A text buffer that get filled with the name.
 *   out_size       - size of the out buffer.
 *
 * Return:
 *   NULL if no name was found.  A pointer to the passed buffer otherwise.
 */
const char *skycultures_get_name(const char* main_id, char *out, int out_size)
{
    skyculture_t *cult;
    skyculture_name_t *entry;
    assert(main_id);
    cult = g_skycultures->current;
    const char* tr_name;

    if (!cult) return NULL;
    HASH_FIND_STR(cult->names, main_id, entry);
    if (!entry) return NULL;
    switch (g_skycultures->name_format_style) {
    case NAME_AUTO:
        if (*entry->name_english) {
            tr_name = sys_translate("skyculture", entry->name_english);
            snprintf(out, out_size, "%s", tr_name);
            return out;
        }
        if (*entry->name_pronounce) {
            snprintf(out, out_size, "%s", entry->name_pronounce);
            return out;
        }
        if (*entry->name_native) {
            snprintf(out, out_size, "%s", entry->name_native);
            return out;
        }
        return NULL;
    case NAME_NATIVE_AND_PRONOUNCE:
        if (*entry->name_native && *entry->name_pronounce) {
            snprintf(out, out_size, "%s (%s)", entry->name_native,
                     entry->name_pronounce);
            return out;
        }
        // If not both are present fallback to NAME_NATIVE
    case NAME_NATIVE:
        if (*entry->name_native) {
            snprintf(out, out_size, "%s", entry->name_native);
            return out;
        }
        if (*entry->name_pronounce) {
            snprintf(out, out_size, "%s", entry->name_pronounce);
            return out;
        }
    }
    return NULL;
}

// Set/Get the current skyculture by id.
static json_value *skycultures_current_id_fn(
        obj_t *obj, const attribute_t *attr, const json_value *args)
{
    char id[128];
    skycultures_t *cults = (skycultures_t*)obj;
    skyculture_t *cult;
    if (args && args->u.array.length) {
        args_get(args, TYPE_STRING, id);
        if (cults->current) {
            MODULE_ITER(cults, cult, "skyculture") {
                if (strcmp(cult->obj.id, cults->current->obj.id) == 0) {
                    skyculture_deactivate(cult);
                    break;
                }
            }
        }
        MODULE_ITER(cults, cult, "skyculture") {
            if (strcmp(cult->obj.id, id) == 0) {
                skyculture_activate(cult);
                break;
            }
        }
    }
    if (!cults->current) return json_null_new();
    return args_value_new(TYPE_STRING, cults->current->obj.id);
}

static int skycultures_list(const obj_t *obj, observer_t *obs,
                            double max_mag, uint64_t hint, const char *source,
                            void *user, int (*f)(void *user, obj_t *obj))
{
    const skycultures_t *cults = (void*)obj;
    const skyculture_t *cult = cults->current;
    obj_t *star;
    int code;
    skyculture_name_t *entry, *tmp;
    int hip;
    if (!cult) return 0;

    HASH_ITER(hh, cult->names, entry, tmp) {
        hip = 0;
        // Special case for HIP stars (most common case)
        if (strncmp(entry->main_id, "HIP ", 4) == 0) {
            hip = atoi(entry->main_id + 4);
            if (hip) {
                star = obj_get_by_hip(hip, &code);
                if (code == 0) return MODULE_AGAIN;
            }
        }
        if (hip == 0) {
            star = obj_get(NULL, entry->main_id, 0);
        }
        if (!star) continue;
        f(user, star);
        obj_release(star);
    }

    return 0;
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
        PROPERTY(name, TYPE_STRING_PTR, MEMBER(skyculture_t, name)),
        PROPERTY(introduction, TYPE_STRING_PTR,
                 MEMBER(skyculture_t, introduction)),
        PROPERTY(description, TYPE_STRING_PTR,
                 MEMBER(skyculture_t, description)),
        PROPERTY(references, TYPE_STRING_PTR, MEMBER(skyculture_t, references)),
        PROPERTY(authors, TYPE_STRING_PTR, MEMBER(skyculture_t, authors)),
        PROPERTY(licence, TYPE_STRING_PTR, MEMBER(skyculture_t, licence)),
        PROPERTY(url, TYPE_STRING_PTR, MEMBER(skyculture_t, uri)),
        PROPERTY(tour, TYPE_JSON, MEMBER(skyculture_t, tour)),
        {}
    },
};
OBJ_REGISTER(skyculture_klass)


static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .gui            = skycultures_gui,
    .init           = skycultures_init,
    .update         = skycultures_update,
    .add_data_source    = skycultures_add_data_source,
    .create_order   = 30, // After constellations.
    .list           = skycultures_list,
    .attributes = (attribute_t[]) {
        PROPERTY(current, TYPE_OBJ, MEMBER(skycultures_t, current)),
        PROPERTY(current_id, TYPE_STRING, .fn = skycultures_current_id_fn),
        PROPERTY(name_format_style, TYPE_INT,
                 MEMBER(skycultures_t, name_format_style)),
        {}
    },
};
OBJ_REGISTER(skycultures_klass)
