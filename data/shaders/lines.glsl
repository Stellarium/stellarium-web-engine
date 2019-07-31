/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform   lowp      vec2    u_win_size;
uniform   lowp      float   u_line_width;
uniform   lowp      float   u_line_glow;
uniform   lowp      vec4    u_color;

varying   lowp      vec4    v_color;
varying   lowp      vec4    v_glow_color;
varying   mediump   vec2    v_uv;

#ifdef VERTEX_SHADER

attribute highp     vec2    a_pos;
attribute highp     vec2    a_tex_pos;

void main()
{
    gl_Position = vec4((a_pos / u_win_size - 0.5) * vec2(2.0, -2.0), 0.0, 1.0);
    v_color = u_color;
    v_glow_color = vec4(mix(u_color.rgb, vec3(1.0), 0.5),
                        u_color.a * u_line_glow);
    v_uv = a_tex_pos;
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    mediump vec4 base;
    mediump vec4 glow;
    mediump float dist = abs(v_uv.y); // Distance to line in pixel.
    base = v_color;
    base.a *= step(dist, u_line_width / 2.0);
    glow = mix(v_glow_color, vec4(0.0), dist / 5.0);
    gl_FragColor = vec4(base.rgb + glow.rgb * glow.a, max(base.a, glow.a));

}

#endif
