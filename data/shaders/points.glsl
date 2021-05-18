/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform lowp vec4 u_color;

// Size of the core point, not including the halo relative to the total
// size of the rendered point.
// 0 -> only halo (not supported), 1 -> no halo
uniform lowp float u_core_size;

varying lowp    vec4 v_color;

#ifdef VERTEX_SHADER

attribute lowp    vec4  a_color;
attribute mediump float a_size;

#ifdef IS_3D
    #include "projections.glsl"
    attribute highp   vec3  a_pos;
#else
    attribute highp   vec2  a_pos;
#endif

void main()
{
    #ifdef IS_3D
        gl_Position = proj(a_pos);
    #else
        gl_Position = vec4(a_pos, 1.0, 1.0);
    #endif

    gl_PointSize = a_size * 2.0 / u_core_size;
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
    k = smoothstep(u_core_size * 1.25, u_core_size * 0.75, dist);

    // Halo
    k += smoothstep(1.0, 0.0, dist) * 0.08;
    gl_FragColor.rgb = v_color.rgb;
    gl_FragColor.a = v_color.a * clamp(k, 0.0, 1.0);
}

#endif
