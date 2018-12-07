/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// Set the DEBUG macro if needed
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#  define GLES2
#  include <OPenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
#else
#  ifdef GLES2
#    include <GLES2/gl2.h>
#    include <GLES2/gl2ext.h>
#  else
#    include <GL/gl.h>
#  endif
#endif

#if DEBUG
#  define GL(line) do {                                 \
       line;                                            \
       if (gl_check_errors(__FILE__, __LINE__))         \
           exit(-1);                                    \
   } while(0)
#else
#  define GL(line) line
#endif

int gl_check_errors(const char *file, int line);
int gl_create_program(const char *vertex_shader_code,
                      const char *fragment_shader_code, const char *include,
                      const char **attr_names);
