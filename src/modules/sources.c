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
 * File: sources.c
 * A module that handle sources list loading.
 *
 * We need this module because if we add a data source dir, that contains
 * a list of other directories, we need to first parse the index.json file
 * to get the type of all the sub sources.
 *
 * This could be done in the core, but it's easier to move this into a separate
 * module.
 */

enum {
    SOURCE_DIR = 0,
    SOURCE_HIPSLIST,
    SOURCE_HIPS,
};

typedef struct source source_t;
struct source {
    char            *url;
    int             type;
    double          release_date;
    source_t        *next, *prev;
};

typedef struct sources {
    obj_t           obj;
    source_t        *sources;
} sources_t;

static int process_source(sources_t *sources, source_t *source);

static int add_data_source(obj_t *obj, const char *url, const char *type,
                           json_value *args)
{
    char *tmp;
    source_t *source = NULL;
    sources_t *sources = (sources_t*)obj;

    if (!type) {
        source = calloc(1, sizeof(*source));
        source->url = strdup(url);
    } else if (strcmp(type, "hipslist") == 0) {
        source = calloc(1, sizeof(*source));
        source->url = strdup(url);
        source->type = SOURCE_HIPSLIST;
    } else if (strcmp(type, "hips") == 0 && !args) {
        source = calloc(1, sizeof(*source));
        source->url = strdup(url);
        source->type = SOURCE_HIPS;
    }
    if (!source) return 1;

    // Parse url of the form: <URL>?v=date for cache invalidation.
    if ((tmp = strstr(source->url, "?v="))) {
        *tmp = '\0';
        source->release_date = hips_parse_date(tmp + 3);
    }

    DL_APPEND(sources->sources, source);
    // Immediatly process only if offline source.
    if (strncmp(url, "http", 4) != 0)
        process_source(sources, source);
    return 0;
}

static const char *get_data(const source_t *source, const char *file,
                            int extra_flags, int *code)
{
    char url[1024];
    const char *data;

    if (    source->release_date &&
            (strncmp(source->url, "http://", 7) == 0 ||
             strncmp(source->url, "https://", 8) == 0)) {
        sprintf(url, "%s/%s?v=%d",
                source->url, file, (int)source->release_date);
    } else {
        sprintf(url, "%s/%s", source->url, file);
    }

    data = asset_get_data2(url, ASSET_USED_ONCE | extra_flags, NULL, code);
    return data;
}

static int parse_index(const char *base_url, const char *data)
{
    json_value *json;
    const char *key, *type;
    int i;
    char url[1024];

    json = json_parse(data, strlen(data));
    if (!json || json->type != json_object) {
        LOG_E("Cannot parse json file");
        return -1;
    }
    for (i = 0; i < json->u.object.length; i++) {
        if (json->u.object.values[i].value->type != json_object) continue;
        key = json->u.object.values[i].name;
        type = json_get_attr_s(json->u.object.values[i].value, "type");
        sprintf(url, "%s/%s", base_url, key);
        module_add_data_source(NULL, url, type, NULL);
    }

    json_value_free(json);
    return 0;
}

static int on_hips(void *user, const char *url, double release_date)
{
    sources_t *sources = (sources_t*)user;
    source_t *source;
    source = calloc(1, sizeof(*source));
    source->url = strdup(url);
    source->type = SOURCE_HIPS;
    source->release_date = release_date;
    DL_APPEND(sources->sources, source);
    return 0;
}

static int hips_property_handler(void* user, const char* section,
                                 const char* name, const char* value)
{
    json_value *args = user;
    json_object_push(args, name, json_string_new(value));
    return 0;
}

static int on_sub_dir(void *user, const char *path, int is_dir)
{
    if (!is_dir) return 0;
    module_add_data_source(NULL, path, NULL, NULL);
    return 0;
}

static int process_dir(source_t *source)
{
    const char *data;
    int code;

    // First check if there is an index.json file, if so we use it.
    data = get_data(source, "index.json", ASSET_ACCEPT_404, &code);
    if (!code) return 0;
    if (data) {
        parse_index(source->url, data);
        return 1;
    }

    // Check for a skyculture dir.
    data = get_data(source, "constellationship.fab",
                    ASSET_ACCEPT_404 | ASSET_USED_ONCE, &code);
    if (!code) return 0;
    if (data) {
        module_add_data_source(NULL, source->url, "skyculture", NULL);
        return 1;
    }

    // Check for a HiPS survey.
    data = get_data(source, "properties",
                    ASSET_ACCEPT_404 | ASSET_USED_ONCE, &code);
    if (!code) return 0;
    if (data) {
        module_add_data_source(NULL, source->url, "hips", NULL);
        return 1;
    }

    // Finally try to iter for subdirectories.
    if (strncmp(source->url, "http", 4) != 0) {
        sys_list_dir(source->url, NULL, on_sub_dir);
    }
    return 1;
}

static int process_source(sources_t *sources, source_t *source)
{
    const char *data;
    json_value *args;
    int code;

    switch (source->type) {
    case SOURCE_DIR:
        if (!process_dir(source)) return 0;
        break;
    case SOURCE_HIPSLIST:
        data = get_data(source, "hipslist", 0, &code);
        if (!data && code) break; // Error.
        if (!data) return 0;
        hips_parse_hipslist(data, sources, on_hips);
        break;
    case SOURCE_HIPS:
        data = get_data(source, "properties", 0, &code);
        if (!data && code) break; // Error.
        if (!data) return 0;
        args = json_object_new(0);
        ini_parse_string(data, hips_property_handler, args);
        module_add_data_source(NULL, source->url, "hips", args);
        json_builder_free(args);
        break;
    default:
        assert(false);
    }
    DL_DELETE(sources->sources, source);
    free(source->url);
    free(source);
    return 1;
}

static int sources_update(obj_t *obj, double dt)
{
    sources_t *sources = (sources_t*)obj;
    source_t *source, *tmp;

    DL_FOREACH_SAFE(sources->sources, source, tmp) {
        process_source(sources, source);
    }
    return 0;
}

static obj_klass_t sources_klass = {
    .id                 = "sources",
    .size               = sizeof(sources_t),
    .flags              = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .update             = sources_update,
    .add_data_source    = add_data_source,
    .create_order       = -1,
    .attributes = (attribute_t[]) {
        {}
    },
};
OBJ_REGISTER(sources_klass)
