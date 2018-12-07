/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform lowp vec4 u_color;
uniform lowp float u_smooth;

varying mediump vec2 v_tex_pos;
varying lowp    vec4 v_color;

#ifdef VERTEX_SHADER

attribute highp   vec4 a_pos;
attribute mediump vec2 a_tex_pos;
attribute lowp    vec4 a_color;
attribute lowp    vec2 a_shift;

void main()
{
    vec4 pos = a_pos;
    pos.xy += 2.0 * a_shift;
    gl_Position = pos;
    v_tex_pos = a_tex_pos;
    v_color = a_color * u_color;
}

#endif
#ifdef FRAGMENT_SHADER

lowp float dist;
lowp float k;

void main()
{
    dist = 2.0 * distance(v_tex_pos, vec2(0.5, 0.5));
    k = smoothstep(1.0 - u_smooth, 1.0, dist);
    k = sqrt(k);
    gl_FragColor.rgb = v_color.rgb;
    // Saturation effect at the center.
    gl_FragColor.rgb *= 1.0 + smoothstep(0.2, 0.0, k);
    gl_FragColor.a = v_color.a * (1.0 - k);
}

#endif
