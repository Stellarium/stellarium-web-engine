/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifdef GL_ES
precision mediump float;
#endif

uniform lowp    vec4        u_color;

varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

#includes "projections.glsl"

attribute highp   vec4       a_pos;
attribute highp   vec3       a_sky_pos;

void main()
{
    gl_Position = proj(a_pos.xyz);
    const lowp float height = 0.2;
    const lowp float alpha = 0.15;
    lowp float d = smoothstep(height, 0.0, abs(a_sky_pos.z));
    v_color = u_color;
    v_color.a *= alpha * d;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
