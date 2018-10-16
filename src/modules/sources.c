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

typedef struct source source_t;
struct source {
    char            *url;
    source_t        *next, *prev;
};

typedef struct sources {
    obj_t           obj;
    source_t        *sources;
} sources_t;

static int add_data_source(obj_t *obj, const char *url, const char *type)
{
    source_t *source;
    sources_t *sources = (sources_t*)obj;
    if (type) return 1;
    source = calloc(1, sizeof(*source));
    source->url = strdup(url);
    DL_APPEND(sources->sources, source);
    return 0;
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
        obj_add_data_source(NULL, url, type);
    }

    json_value_free(json);
    return 0;
}

static int sources_update(obj_t *obj, const observer_t *obs, double dt)
{
    sources_t *sources = (sources_t*)obj;
    source_t *source, *tmp;
    char index_url[1024];
    const char *data;
    int code;

    DL_FOREACH_SAFE(sources->sources, source, tmp) {
        sprintf(index_url, "%s/index.json", source->url);
        data = asset_get_data(index_url, NULL, &code);
        if (!code) continue;
        if (data) {
            parse_index(source->url, data);
            asset_release(index_url);
        } else {
            LOG_W("Cannot get %s", index_url);
        }
        DL_DELETE(sources->sources, source);
        free(source->url);
        free(source);
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
