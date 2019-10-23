/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform highp   vec4        u_color;
uniform mediump sampler2D   u_tex;

varying highp   vec2        v_tex_pos;
varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

attribute highp     vec4    a_pos;
attribute mediump   vec2    a_tex_pos;
attribute lowp      vec3    a_color;

void main()
{
    gl_Position = a_pos;
    v_tex_pos = a_tex_pos;
    v_color = vec4(a_color, 1.0) * u_color;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
#ifndef TEXTURE_LUMINANCE
    gl_FragColor = texture2D(u_tex, v_tex_pos) * v_color;
#else
    // Luminance mode: the texture only applies to the alpha channel.
    gl_FragColor = v_color;
    gl_FragColor.a *= texture2D(u_tex, v_tex_pos).r;
#endif
}

#endif
