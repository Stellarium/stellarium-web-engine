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
#include <stdarg.h>
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
    return errors;
}

static int compile_shader(int shader, const char *code,
                          const char *include1,
                          const char *include2)
{
    int status, len;
    char *log;
#ifndef GLES2
    // We need GLSL version 1.2 to have gl_PointCoord support in desktop OpenGL
    // It's already included in GLES 2.0
    const char *pre =
            "#version 120\n#define highp\n#define mediump\n#define lowp\n";
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

/*
 * Function: gl_shader_create
 * Helper function that compiles an opengl shader.
 *
 * Parameters:
 *   vert       - The vertex shader code.
 *   frag       - The fragment shader code.
 *   include    - Extra includes added to both shaders.
 *   attr_names - NULL terminated list of attribute names that will be binded.
 *
 * Return:
 *   A new gl_shader_t instance.
 */
gl_shader_t *gl_shader_create(const char *vert, const char *frag,
                              const char *include, const char **attr_names)
{
    int i, status, len, count;
    int vertex_shader, fragment_shader;
    char log[1024];
    gl_shader_t *shader;
    GLint prog;
    gl_uniform_t *uni;

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    include = include ? : "";
    assert(vertex_shader);
    if (compile_shader(vertex_shader, vert,
                       "#define VERTEX_SHADER\n", include))
        return NULL;
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    if (compile_shader(fragment_shader, frag,
                       "#define FRAGMENT_SHADER\n", include))
        return NULL;
    prog = glCreateProgram();
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);

    // Set all the attributes names if specified.
    if (attr_names) {
        for (i = 0; attr_names[i]; i++) {
            glBindAttribLocation(prog, i, attr_names[i]);
        }
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOG_E("Link Error");
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOG_E("%s", log);
        return NULL;
    }

    shader = calloc(1, sizeof(*shader));
    shader->prog = prog;

    GL(glGetProgramiv(shader->prog, GL_ACTIVE_UNIFORMS, &count));
    for (i = 0; i < count; i++) {
        uni = &shader->uniforms[i];
        GL(glGetActiveUniform(shader->prog, i, sizeof(uni->name),
                              NULL, &uni->size, &uni->type, uni->name));
        // Special case for array uniforms: remove the '[0]'
        if (uni->size > 1) {
            assert(uni->type == GL_FLOAT);
            if (strchr(uni->name, '['))
                *strchr(uni->name, '[') = '\0';
        }
        GL(uni->loc = glGetUniformLocation(shader->prog, uni->name));
    }

    return shader;
}

void gl_shader_delete(gl_shader_t *shader)
{
    int i;
    GLuint shaders[2];
    GLint count = 0;

    if (!shader) return;
    GL(glGetAttachedShaders(shader->prog, 2, &count, shaders));
    for (i = 0; i < count; i++)
        GL(glDeleteShader(shaders[i]));
    GL(glDeleteProgram(shader->prog));
    free(shader);
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

void *gl_buf_at(gl_buf_t *buf, int idx, int attr)
{
    return buf->data + idx * buf->info->size + buf->info->attrs[attr].ofs;
}

static void gl_buf_set(gl_buf_t *buf, int i, int attr, void *v, int size)
{
    void *dst;
    const __typeof__(buf->info->attrs[attr]) *a;
    if (i == -1) i = buf->nb;
    assert(i < buf->capacity);
    a = &buf->info->attrs[attr];
    dst = buf->data + i * buf->info->size + a->ofs;
    memcpy(dst, v, size);
}

void gl_buf_1f(gl_buf_t *buf, int i, int attr, float v0)
{
    gl_buf_set(buf, i, attr, &v0, 4);
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
    if (!buf->info->attrs[attr].type) return;
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
                                 info->size, (void*)(uintptr_t)a->ofs));
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

bool gl_has_uniform(gl_shader_t *shader, const char *name)
{
    gl_uniform_t *uni;
    for (uni = &shader->uniforms[0]; uni->size; uni++) {
        if (strcmp(uni->name, name) == 0) return true;
    }
    return false;
}

void gl_update_uniform(gl_shader_t *shader, const char *name, ...)
{
    gl_uniform_t *uni;
    va_list args;

    for (uni = &shader->uniforms[0]; uni->size; uni++) {
        if (strcmp(uni->name, name) == 0) break;
    }
    if (!uni->size) return; // No such uniform.

    va_start(args, name);
    switch (uni->type) {
    case GL_INT:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
        GL(glUniform1i(uni->loc, va_arg(args, int)));
        break;
    case GL_FLOAT:
        if (uni->size == 1)
            GL(glUniform1f(uni->loc, va_arg(args, double)));
        else
            GL(glUniform1fv(uni->loc, uni->size, va_arg(args, float*)));
        break;
    case GL_FLOAT_VEC2:
        GL(glUniform2fv(uni->loc, 1, va_arg(args, const float*)));
        break;
    case GL_FLOAT_VEC3:
        GL(glUniform3fv(uni->loc, 1, va_arg(args, const float*)));
        break;
    case GL_FLOAT_VEC4:
        GL(glUniform4fv(uni->loc, 1, va_arg(args, const float*)));
        break;
    case GL_FLOAT_MAT3:
        GL(glUniformMatrix3fv(uni->loc, 1, 0, va_arg(args, const float*)));
        break;
    case GL_FLOAT_MAT4:
        GL(glUniformMatrix4fv(uni->loc, 1, 0, va_arg(args, const float*)));
        break;
    default:
        assert(false);
    }
    va_end(args);
}
