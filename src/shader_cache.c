/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "shader_cache.h"

#define MAX_NB_SHADERS 32

typedef struct {
    char key[256];
    gl_shader_t *shader;
} shader_t;

static shader_t g_shaders[MAX_NB_SHADERS] = {};

static char *process_includes(const char *code)
{
    UT_string ret;
    const char *pos, *end, *include;
    char path[128];

    if (!strstr(code, "#include")) return code;
    utstring_init(&ret);
    while ((pos = strstr(code, "#include"))) {
        utstring_printf(&ret, "%.*s", (int)(pos - code), code);
        code = strstr(code, "\"") + 1;
        end = strstr(code, "\"");
        snprintf(path, sizeof(path), "asset://shaders/%.*s",
                 (int)(end - code), code);
        include = asset_get_data(path, NULL, NULL);
        assert(include);
        utstring_printf(&ret, "%s", include);
        code = end + 1;
    }
    utstring_printf(&ret, "%s", code);
    return utstring_body(&ret);
}

gl_shader_t *shader_get(const char *name, const shader_define_t *defines,
                        const char **attr_names,
                        void (*on_created)(gl_shader_t *s))
{
    int i;
    shader_t *s = NULL;
    const char *code;
    char *code2;
    char key[256];
    char buf[64];
    char path[128];
    UT_string pre;
    const shader_define_t *define;


    // Create the key of the form:
    // <name> define1:val1,define2:val2
    strcpy(key, name);
    for (define = defines; define && define->name; define++) {
        if (!define->val) continue;
        // Prevent passing a pointer by accident.
        assert(define->val >= 0 && define->val <= 100);
        snprintf(buf, sizeof(buf), "_%s:%d", define->name, define->val);
        strcat(key, buf);
    }

    for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
        s = &g_shaders[i];
        if (!*s->key) break;
        if (strcmp(s->key, key) == 0)
            return s->shader;
    }

    if (i >= ARRAY_SIZE(g_shaders)) {
        LOG_E("Too many shaders!");
        for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
            LOG_W("%s", g_shaders[i].key);
        }
        assert(false);
    }
    strcpy(s->key, key);

    snprintf(path, sizeof(path), "asset://shaders/%s.glsl", name);
    code = asset_get_data2(path, ASSET_USED_ONCE, NULL, NULL);
    assert(code);
    code2 = process_includes(code);

    utstring_init(&pre);
    for (define = defines; define && define->name; define++) {
        if (!define->val) continue;
        utstring_printf(&pre, "#define %s %d\n", define->name, define->val);
    }
    s->shader = gl_shader_create(code2, code2, utstring_body(&pre), attr_names);
    utstring_done(&pre);
    if (code2 != code) free(code2);
    if (on_created) on_created(s->shader);
    return s->shader;
}
