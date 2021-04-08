/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include "utils/gl.h"

typedef struct {
    const char  *name;
    int         val;
} shader_define_t;

/*
 * Function: shader_get
 * Retreive a cached shader
 *
 * Probably need to change this api soon.
 *
 * Properties:
 *   name       - Name of one of the shaders in the resources.
 *   defines    - Array of <shader_define_t>, terminated by an empty one.
 *                Can be NULL.
 *   on_created - If set, called the first time the shader has been created.
 */
gl_shader_t *shader_get(const char *name, const shader_define_t *defines,
                        const char **attr_names,
                        void (*on_created)(gl_shader_t *s));
