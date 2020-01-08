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
    json_value      *tour;
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
        if (str_startswith(cst->id, "CST ")) {
            module_remove(constellations, cst);
        }
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
        snprintf(anchors, sizeof(anchors), "%f %f %d %f %f %d %f %f %d",
            a->anchors[0].uv[0], a->anchors[0].uv[1], a->anchors[0].hip,
            a->anchors[1].uv[0], a->anchors[1].uv[1], a->anchors[1].hip,
            a->anchors[2].uv[0], a->anchors[2].uv[1], a->anchors[2].hip);
        json_object_push(v, "anchors", json_string_new(anchors));
        json_object_push(v, "uv_in_pixel", json_boolean_new(a->uv_in_pixel));
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
    const json_value *names = NULL, *features = NULL, *description = NULL,
                     *tour = NULL, *edges = NULL;
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
        "?names", JCON_VAL(names),
        "?features", JCON_VAL(features),
        "description", JCON_VAL(description),
        "?edges", JCON_VAL(edges),
        "?tour", JCON_VAL(tour),
    "}");
    if (r) {
        LOG_E("Cannot parse skyculture json (%s)", path);
        return -1;
    }

    cult->info.name = strdup(name);
    cult->description = json_to_string(description);
    if (names) cult->names = skyculture_parse_names_json(names);
    if (tour) cult->tour = json_copy(tour);

    if (!features) goto end;
    cult->constellations = calloc(features->u.object.length,
                                  sizeof(*cult->constellations));
    for (i = 0; i < features->u.object.length; i++) {
        r = skyculture_parse_feature_json(
                features->u.object.values[i].value,
                &cult->constellations[cult->nb_constellations]);
        if (r) continue;
        cult->nb_constellations++;
    }

    // For the moment we parse the art separatly, it should all be merged
    // int a 'feature'.
    arts = calloc(features->u.object.length + 1, sizeof(*arts));
    arts_nb = 0;
    for (i = 0; i < features->u.object.length; i++) {
        r = skyculture_parse_feature_art_json(
                features->u.object.values[i].value, &arts[arts_nb]);
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
            if (!cult->info.name) continue;
            if (strcmp(cult->info.name, cults->current->info.name) == 0) {
                active = true;
            } else {
                active = false;
            }
            res = gui_toggle(cult->info.name, &active);
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
 * Get the name of a star in the current skyculture.
 *
 * Parameters:
 *   skycultures    - A skyculture module.
 *   hip            - HIP number.
 *   buf            - A text buffer that get filled with the name.
 *
 * Return:
 *   NULL if no name was found.  A pointer to the passed buffer otherwise.
 */
const char *skycultures_get_name(obj_t *skycultures, int hip, char buf[128])
{
    skyculture_t *cult;
    skyculture_name_t *entry;
    if (!skycultures) return NULL;
    if (!hip) return NULL;
    assert(strcmp(skycultures->klass->id, "skycultures") == 0);
    cult = ((skycultures_t*)skycultures)->current;
    if (!cult) return NULL;
    HASH_FIND(hh, cult->names, &hip, sizeof(hip), entry);
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
