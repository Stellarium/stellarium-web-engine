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
    char            *(*names)[2]; // [ID, Name].  NULL terminated.
    constellation_infos_t *constellations;
    json_value      *imgs;
    bool            active;
    int             parsed; // union of SK_ enum for each parsed file.
} skyculture_t;

static void skyculture_on_active_changed(
        obj_t *obj, const attribute_t *attr);
static int skyculture_update(obj_t *obj, const observer_t *obs, double dt);

static obj_klass_t skyculture_klass = {
    .id     = "skyculture",
    .size   = sizeof(skyculture_t),
    .flags  = 0,
    .update = skyculture_update,
    .attributes = (attribute_t[]) {
        PROPERTY("name", "s", MEMBER(skyculture_t, info.name)),
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
static int skycultures_update(obj_t *obj, const observer_t *obs, double dt);

static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = skycultures_init,
    .gui            = skycultures_gui,
    .update         = skycultures_update,
    .create_order   = 30, // After constellations.
};

OBJ_REGISTER(skycultures_klass)


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
    int i;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations, *cons;
    skyculture_t *other;

    // Deactivate the current activated skyculture if any.
    OBJ_ITER(cult->obj.parent, other, &skyculture_klass) {
        if (other != cult && other->active) skyculture_deactivate(other);
    }

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
        sprintf(id, "CST %s", cst->id);
        cons = obj_get(constellations, id, 0);
        if (cons) continue;
        args = json_object_new(0);
        json_object_push(args, "info_ptr", json_integer_new((int64_t)cst));
        obj_create("constellation", id, constellations, args);
        json_value_free(args);
    }

    // Add the images.
    if (cult->imgs) {
        for (i = 0; i < cult->imgs->u.array.length; i++) {
            args = cult->imgs->u.array.values[i];
            sprintf(id, "CST %s", json_get_attr_s(args, "id"));
            cons = obj_get(constellations, id, 0);
            if (!cons) continue;
            obj_call_json(cons, "set_image", args);
        }
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
    cult = (void*)obj_create("skyculture", NULL, (obj_t*)cults, NULL);
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
        OBJ_ITER(cult->obj.parent, other, &skyculture_klass) {
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

static int skyculture_update(obj_t *obj, const observer_t *obs, double dt)
{
    skyculture_t *cult = (skyculture_t*)obj;
    const char *info, *constellations, *edges, *names, *imgs;
    char path[1024];
    int nb, code;

    if (!(cult->parsed & SK_INFO)) {
        sprintf(path, "%s/%s", cult->uri, "info.ini");
        info = asset_get_data(path, NULL, NULL);
        assert(info); // For the moment assume local!
        ini_parse_string(info, info_ini_handler, cult);
        free(cult->obj.id);
        asprintf(&cult->obj.id, "CULT %s", cult->info.name);
        cult->parsed |= SK_INFO;
    }

    if (!(cult->parsed & SK_NAMES)) {
        sprintf(path, "%s/%s", cult->uri, "names.txt");
        names = asset_get_data(path , NULL, NULL);
        assert(names); // For the moment assume local!
        nb = skyculture_parse_names(names, NULL);
        cult->names = calloc(nb + 1, sizeof(*cult->names));
        skyculture_parse_names(names, cult->names);
        cult->parsed |= SK_NAMES;
    }

    if (!(cult->parsed & SK_CONSTELLATIONS)) {
        sprintf(path, "%s/%s", cult->uri, "constellations.txt");
        constellations = asset_get_data(path, NULL, NULL);
        sprintf(path, "%s/%s", cult->uri, "edges.txt");
        edges = asset_get_data(path, NULL, NULL);
        assert(constellations); // For the moment assume local!
        cult->parsed |= SK_CONSTELLATIONS;
        cult->constellations = skyculture_parse_constellations(
                constellations, edges, &cult->nb_constellations);
        cult->parsed |= SK_CONSTELLATIONS;
    }

    if (!(cult->parsed & SK_IMGS)) {
        sprintf(path, "%s/%s", cult->uri, "imgs/index.json");
        imgs = asset_get_data(path, NULL, &code);
        if (code) cult->parsed |= SK_IMGS;
        if (!imgs) return 0;
        cult->imgs = parse_imgs(imgs, cult->uri);
        if (cult->active) skyculture_activate(cult);
    }

    return 0;
}

static int skycultures_init(obj_t *obj, json_value *args)
{
    const char *path;
    char name[256], url[1024];
    skycultures_t *cults = (void*)obj;
    skyculture_t *cult;
    asset_set_alias("https://data.stellarium.org/skycultures",
                    "asset://skycultures");

    // Add the other ones.
    ASSET_ITER("asset://skycultures/", path) {
        if (!str_endswith(path, "/info.ini")) continue;
        strcpy(name, path + strlen("asset://skycultures/"));
        *strchr(name, '/') = '\0';
        sprintf(url, "https://data.stellarium.org/skycultures/%s", name);
        cult = add_from_uri(cults, url);
        // Immedatly load the western skyculture.
        if (strcmp(name, "western") == 0)
            obj_set_attr((obj_t*)cult, "active", "b", true);
    }
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

static int skycultures_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *skyculture;
    OBJ_ITER(obj, skyculture, &skyculture_klass) {
        obj_update(skyculture, obs, dt);
    }
    return 0;
}
