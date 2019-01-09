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
    SK_NAMES          = 1 << 1,
    SK_CONSTELLATIONS = 1 << 2,
    SK_IMGS           = 1 << 3,
    SK_DESCRIPTION    = 1 << 4,
    SK_EDGES          = 1 << 5,

    SK_CONSTELLATIONS_STEL          = 1 << 6,
    SK_CONSTELLATION_NAMES_STEL     = 1 << 7,
    SK_IMGS_STEL                    = 1 << 8,
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
    skyculture_name_t *names; // NULL terminated.
    constellation_infos_t *constellations;
    json_value      *imgs;
    bool            active;
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
} skycultures_t;



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

static void skyculture_activate(skyculture_t *cult)
{
    char id[32];
    int i, nb_skipped = 0;
    obj_t *star;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations, *cons;
    skyculture_t *other;

    // Deactivate the current activated skyculture if any.
    OBJ_ITER(cult->obj.parent, other, "skyculture") {
        if (other != cult && other->active) skyculture_deactivate(other);
    }

    // Add all the names.
    for (i = 0; cult->names && cult->names[i].oid; i++) {
        star = obj_get_by_oid(NULL, cult->names[i].oid, 0);
        if (!star) {
            nb_skipped++;
            continue;
        }
        identifiers_add("NAME", cult->names[i].name, cult->names[i].oid, 0,
                        star->type, star->vmag, NULL, NULL);
        obj_release(star);
    }

    if (nb_skipped)
        LOG_D("Cannot add name for %d stars in skyculture %s",
              nb_skipped, cult->info.name);

    // Create all the constellations object.
    constellations = obj_get(NULL, "constellations", 0);
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
            obj_call_json(cons, "set_image", args);
            obj_release(cons);
        }
    }

    // Set the current attribute of the skycultures manager object.
    obj_set_attr(cult->obj.parent, "current", "p", cult);
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

static int skyculture_update(obj_t *obj, const observer_t *obs, double dt);
static skyculture_t *add_from_uri(skycultures_t *cults, const char *uri,
                                  const char *id)
{
    skyculture_t *cult;
    cult = (void*)obj_create("skyculture", id, (obj_t*)cults, NULL);
    cult->uri = strdup(uri);
    skyculture_update((obj_t*)cult, NULL, 0);
    return cult;
}

static void skyculture_on_active_changed(
        obj_t *obj, const attribute_t *attr)
{
    skyculture_t *cult = (void*)obj, *other;

    // Deactivate the others.
    if (cult->active) {
        OBJ_ITER(cult->obj.parent, other, "skyculture") {
            if (other == cult) continue;
            obj_set_attr((obj_t*)other, "active", "b", false);
        }
    }

    if (cult->active) skyculture_activate(cult);
    else skyculture_deactivate(cult);
}

static json_value *parse_imgs(const char *data, const char *uri)
{
    int i;
    char base_path[1024];
    char error[json_error_max] = "";
    json_value *value, *v;
    json_settings settings = {};
    settings.value_extra = json_builder_extra;
    value = json_parse_ex(&settings, data, strlen(data), error);
    if (error[0]) {
        LOG_E("Failed to parse json: %s", error);
        return NULL;
    }

    // Add the base_path attribute needed by the constellation add_img
    // function.
    // XXX: It would probably be better to add it in the activate function
    //      instead.
    sprintf(base_path, "%s/imgs", uri);
    for (i = 0; i < value->u.array.length; i++) {
        v = value->u.array.values[i];
        json_object_push(v, "base_path", json_string_new(base_path));
    }
    return value;
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

static int skyculture_update(obj_t *obj, const observer_t *obs, double dt)
{
    skyculture_t *cult = (skyculture_t*)obj;
    const char *data;
    constellation_art_t *arts;

    if (get_file(cult, SK_INFO, "info.ini", &data, 0)) {
        ini_parse_string(data, info_ini_handler, cult);
    }

    if (get_file(cult, SK_NAMES, "names.txt", &data, 0)) {
        cult->names = skyculture_parse_names(data, NULL);
    }

    if (get_file(cult, SK_CONSTELLATIONS, "constellations.txt", &data, 0)) {
        cult->constellations = skyculture_parse_constellations(
                data, &cult->nb_constellations);
    }

    if (cult->constellations &&
            get_file(cult, SK_EDGES, "edges.txt", &data, ASSET_ACCEPT_404)) {
        skyculture_parse_edges(data, cult->constellations);
    }

    if (get_file(cult, SK_DESCRIPTION, "description.en.html", &data, 0)) {
        cult->description = strdup(data);
        obj_changed((obj_t*)cult, "description");
        return 0; // Don't load imgs just after the descriptions.
    }

    if (get_file(cult, SK_IMGS, "imgs/index.json", &data, ASSET_ACCEPT_404)) {
        cult->imgs = parse_imgs(data, cult->uri);
        if (cult->active) skyculture_activate(cult);
    }


    // Fallback support for stellarium format.
    // XXX: This should be the default, and we should remove the
    // original format.
    if ((cult->parsed & SK_CONSTELLATIONS) && !cult->constellations &&
         get_file(cult, SK_CONSTELLATIONS_STEL, "constellationship.fab",
                  &data, 0))
    {
        cult->constellations = skyculture_parse_stellarium_constellations(
                data, &cult->nb_constellations);
    }

    if ((cult->parsed & SK_CONSTELLATIONS_STEL) && cult->constellations &&
         get_file(cult, SK_CONSTELLATION_NAMES_STEL,
                  "constellation_names.eng.fab", &data, 0))
    {
        skyculture_parse_stellarium_constellations_names(
                data, cult->constellations);
    }

    if ((cult->parsed & SK_CONSTELLATIONS_STEL) && cult->constellations &&
            get_file(cult, SK_IMGS_STEL, "constellationsart.fab",
                     &data, ASSET_ACCEPT_404))
    {
        arts = skyculture_parse_stellarium_constellations_art(data, NULL);
        if (arts) cult->imgs = make_imgs_json(arts, cult->uri);
        free(arts);
        if (cult->active) skyculture_activate(cult);
    }

    return 0;
}

static void skycultures_gui(obj_t *obj, int location)
{
    skyculture_t *cult;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Skycultures")) {
        OBJ_ITER(obj, cult, "skyculture") {
            if (!cult->info.name) continue;
            gui_item(&(gui_item_t){
                    .label = cult->info.name,
                    .obj = (obj_t*)cult,
                    .attr = "active"});
        }
        gui_tab_end();
    }
}

static int skycultures_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *skyculture;
    OBJ_ITER(obj, skyculture, "skyculture") {
        obj_update(skyculture, obs, dt);
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
    if (obj_get((obj_t*)cults, key, 0)) return 0;
    cult = add_from_uri(cults, url, key);
    if (!cult) LOG_W("Cannot add skyculture (%s)", url);
    // If it's the first one activate it immediately.
    if (cult == (void*)cults->obj.children)
        obj_set_attr((obj_t*)cult, "active", "b", true);
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
        PROPERTY("name", "s", MEMBER(skyculture_t, info.name)),
        PROPERTY("active", "b", MEMBER(skyculture_t, active),
                 .on_changed = skyculture_on_active_changed),
        PROPERTY("description", "s", MEMBER(skyculture_t, description)),
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
        PROPERTY("current", "p", MEMBER(skycultures_t, current),
                 .hint = "obj"),
        {}
    },
};
OBJ_REGISTER(skycultures_klass)
