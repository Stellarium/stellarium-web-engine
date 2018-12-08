/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "gl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef LOG_E
#   define LOG_E
#endif

static const char* gl_get_error_text(int code) {
    switch (code) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "undefined error";
    }
}

int gl_check_errors(const char *file, int line)
{
    int errors = 0;
    while (true)
    {
        GLenum x = glGetError();
        if (x == GL_NO_ERROR)
            return errors;
        LOG_E("%s:%d: OpenGL error: %d (%s)\n",
              file, line, x, gl_get_error_text(x));
        errors++;
    }
}

static int compile_shader(int shader, const char *code,
                          const char *include1,
                          const char *include2)
{
    int status, len;
    char *log;
#ifndef GLES2
    const char *pre = "#define highp\n#define mediump\n#define lowp\n";
#else
    const char *pre = "";
#endif
    const char *sources[] = {pre, include1, include2, code};
    glShaderSource(shader, 4, (const char**)&sources, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        log = malloc(len + 1);
        LOG_E("Compile shader error:");
        glGetShaderInfoLog(shader, len, &len, log);
        LOG_E("%s", log);
        free(log);
        assert(false);
    }
    return 0;
}

int gl_create_program(const char *vertex_shader_code,
                      const char *fragment_shader_code, const char *include,
                      const char **attr_names)
{
    int i;
    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    include = include ? : "";
    assert(vertex_shader);
    if (compile_shader(vertex_shader, vertex_shader_code,
                       "#define VERTEX_SHADER\n", include))
        return 0;
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    if (compile_shader(fragment_shader, fragment_shader_code,
                       "#define FRAGMENT_SHADER\n", include))
        return 0;
    int prog = glCreateProgram();
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);

    // Set all the attributes names if specified.
    if (attr_names) {
        for (i = 0; attr_names[i]; i++) {
            glBindAttribLocation(prog, i, attr_names[i]);
        }
    }

    glLinkProgram(prog);
    int status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOG_E("Link Error");
        int len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char log[len];
        glGetProgramInfoLog(prog, len, &len, log);
        LOG_E("%s", log);
        return 0;
    }
    return prog;
}

void gl_buf_alloc(gl_buf_t *buf, const gl_buf_info_t *info, int capacity)
{
    memset(buf, 0, sizeof(*buf));
    buf->info = info;
    buf->data = malloc(capacity * info->size);
    buf->capacity = capacity;
}

void gl_buf_release(gl_buf_t *buf)
{
    free(buf->data);
}

void gl_buf_next(gl_buf_t *buf)
{
    assert(buf->nb < buf->capacity);
    buf->nb++;
}

static int gl_size_for_type(int type)
{
    switch (type) {
    case GL_FLOAT: return sizeof(GLfloat);
    case GL_INT: return sizeof(GLint);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    case GL_BYTE: return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    default: assert(false); return -1;
    }
}

static void gl_buf_set(gl_buf_t *buf, int i, int attr, void *v, int size)
{
    void *dst;
    const __typeof__(buf->info->attrs[attr]) *a;
    if (i == -1) i = buf->nb;
    a = &buf->info->attrs[attr];
    dst = buf->data + i * buf->info->size + a->ofs;
    memcpy(dst, v, size);
}

void gl_buf_2f(gl_buf_t *buf, int i, int attr, float v0, float v1)
{
    float v[2] = {v0, v1};
    gl_buf_set(buf, i, attr, v, 8);
}

void gl_buf_3f(gl_buf_t *buf, int i, int attr, float v0, float v1, float v2)
{
    float v[3] = {v0, v1, v2};
    gl_buf_set(buf, i, attr, v, 12);
}

void gl_buf_4f(gl_buf_t *buf, int i, int attr,
               float v0, float v1, float v2, float v3)
{
    float v[4] = {v0, v1, v2, v3};
    gl_buf_set(buf, i, attr, v, 16);
}

void gl_buf_1i(gl_buf_t *buf, int i, int attr, int v0)
{
    if (buf->info->attrs[attr].type == GL_UNSIGNED_SHORT)
        gl_buf_set(buf, i, attr, (uint16_t[]){v0}, 2);
    else
        assert(false);
}

void gl_buf_4i(gl_buf_t *buf, int i, int attr,
               int v0, int v1, int v2, int v3)
{
    if (buf->info->attrs[attr].type == GL_UNSIGNED_BYTE)
        gl_buf_set(buf, i, attr, (uint8_t[]){v0, v1, v2, v3}, 4);
    else if (buf->info->attrs[attr].type == GL_BYTE)
        gl_buf_set(buf, i, attr, (int8_t[]){v0, v1, v2, v3}, 4);
    else
        assert(false);
}

void gl_buf_enable(const gl_buf_t *buf)
{
    int i, tot = 0;
    const gl_buf_info_t *info = buf->info;
    const __typeof__(*buf->info->attrs) *a;
    for (i = 0; ; i++) {
        a = &info->attrs[i];
        if (!a->size) continue;
        GL(glEnableVertexAttribArray(i));
        GL(glVertexAttribPointer(i, a->size, a->type, a->normalized,
                                 info->size, (void*)(long)a->ofs));
        tot += a->size * gl_size_for_type(a->type);
        if (tot == info->size) break;
    }
}

void gl_buf_disable(const gl_buf_t *buf)
{
    int i, tot = 0;
    const gl_buf_info_t *info = buf->info;
    const __typeof__(*buf->info->attrs) *a;
    for (i = 0; ; i++) {
        a = &info->attrs[i];
        if (!a->size) continue;
        GL(glDisableVertexAttribArray(i));
        tot += a->size * gl_size_for_type(a->type);
        if (tot == info->size) break;
    }
}
