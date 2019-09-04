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

varying lowp    vec4 v_color;
varying mediump float v_halo_dist;

#ifdef VERTEX_SHADER

attribute highp   vec2  a_pos;
attribute lowp    vec4  a_color;
attribute mediump float a_size;

void main()
{
    gl_Position = vec4(a_pos, 0, 1.0);
    v_halo_dist = 1.0 / ((1.0 + u_smooth) * 4.0);
    gl_PointSize = a_size * 2.0 / v_halo_dist;
    v_color = a_color * u_color;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    mediump float dist;
    mediump float k;
    dist = 2.0 * distance(gl_PointCoord, vec2(0.5, 0.5));

    // Center bright point.
    k = smoothstep(v_halo_dist * 1.25, v_halo_dist * 0.75, dist);

    // Halo
    k += smoothstep(1.0, 0.0, dist) * 0.08;
    gl_FragColor.rgb = v_color.rgb;
    gl_FragColor.a = v_color.a * clamp(k, 0.0, 1.0);
}

#endif
