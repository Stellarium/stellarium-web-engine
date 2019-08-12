/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform   lowp    vec4 u_color;
uniform   lowp    vec2 u_fbo_size;

#ifdef VERTEX_SHADER

attribute highp   vec2 a_pos;

void main()
{
    gl_Position = vec4(((a_pos / u_fbo_size) * 2.0 - 1.0) * vec2(1.0, -1.0),
                       0.0, 1.0);
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = u_color;
}

#endif
