/* Stellarium Web Engine - Copyright (c) 2021 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform mediump vec4        u_color;
uniform mediump sampler2D   u_tex;
uniform lowp    vec2        u_win_size;

varying highp   vec2        v_tex_pos;

#ifdef VERTEX_SHADER

#ifdef HAS_VIEW_POS
    attribute highp vec3 a_pos;
    #includes "projections.glsl"
#endif

attribute highp     vec2    a_wpos;
attribute mediump   vec2    a_tex_pos;

void main()
{
    #ifdef HAS_VIEW_POS
        gl_Position = proj(a_pos);
    #else
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    #endif
    gl_Position.xy = (a_wpos / u_win_size - 0.5) * vec2(2.0, -2.0);
    gl_Position.xy *= gl_Position.w;
    v_tex_pos = a_tex_pos;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
#ifndef TEXTURE_LUMINANCE
    gl_FragColor = texture2D(u_tex, v_tex_pos) * u_color;
#else
    // Luminance mode: the texture only applies to the alpha channel.
    gl_FragColor = u_color;
    gl_FragColor.a *= texture2D(u_tex, v_tex_pos).r;
#endif
}

#endif
