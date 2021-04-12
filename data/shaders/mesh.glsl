/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

varying   lowp    vec4 v_color;

#ifdef VERTEX_SHADER

#includes "projections.glsl"

attribute highp   vec3 a_pos;
attribute lowp    vec4 a_color;

void main()
{
    gl_Position = proj(a_pos);
    v_color = a_color;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
