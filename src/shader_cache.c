/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "shader_cache.h"

#define MAX_NB_SHADERS 16

typedef struct {
    char key[256];
    gl_shader_t *shader;
} shader_t;

static shader_t g_shaders[MAX_NB_SHADERS] = {};

gl_shader_t *shader_get(const char *name, const shader_define_t *defines,
                        const char **attr_names,
                        void (*on_created)(gl_shader_t *s))
{
    int i;
    shader_t *s = NULL;
    const char *code;
    char key[256];
    char path[128];
    char pre[256] = {};
    const shader_define_t *define;


    // Create the key of the form:
    // <name>_define1_define2
    strcpy(key, name);
    for (define = defines; define && define->name; define++) {
        if (define->set) {
            strcat(key, "_");
            strcat(key, define->name);
        }
    }

    for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
        s = &g_shaders[i];
        if (!*s->key) break;
        if (strcmp(s->key, key) == 0)
            return s->shader;
    }
    assert(i < ARRAY_SIZE(g_shaders));
    strcpy(s->key, key);

    snprintf(path, sizeof(path), "asset://shaders/%s.glsl", name);
    code = asset_get_data2(path, ASSET_USED_ONCE, NULL, NULL);
    assert(code);

    for (define = defines; define && define->name; define++) {
        if (define->set) {
            snprintf(pre + strlen(pre), sizeof(pre) - strlen(pre),
                     "#define %s\n", define->name);
        }
    }
    s->shader = gl_shader_create(code, code, pre, attr_names);
    if (on_created) on_created(s->shader);
    return s->shader;
}
