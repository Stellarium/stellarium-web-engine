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

#ifdef DASH
// Dash effect defined as a total length (dash and space, in uv unit),
// and the ratio of the dash dot to the length.
uniform   lowp      float   u_dash_length;
uniform   lowp      float   u_dash_ratio;
#endif

#ifdef FADE
uniform     lowp    float   u_fade_dist_min;
uniform     lowp    float   u_fade_dist_max;
varying     lowp    float   v_alpha;
#endif

varying   mediump   vec2    v_uv;

#ifdef VERTEX_SHADER

#includes "projections.glsl"

attribute highp     vec3    a_pos;
attribute highp     vec2    a_wpos;
attribute highp     vec2    a_tex_pos;

void main()
{
    gl_Position = proj(a_pos);
    gl_Position.xy = (a_wpos / u_win_size - 0.5) * vec2(2.0, -2.0);
    gl_Position.xy *= gl_Position.w;
    v_uv = a_tex_pos;

#ifdef FADE
    v_alpha = smoothstep(u_fade_dist_max, u_fade_dist_min, -a_pos.z);
#endif
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    mediump float dist = abs(v_uv.y); // Distance to line in pixel.
    // Use smooth step on 2.4px to emulate an anti-aliased line
    mediump float base = smoothstep(u_line_width / 2.0 + 1.2, u_line_width / 2.0 - 1.2, dist);
    // Generate a glow with 5px radius
    mediump float glow = (1.0 - dist / 5.0) * u_line_glow;
    // Only use the most visible of both to avoid changing brightness
    gl_FragColor = vec4(u_color.rgb, u_color.a * max(glow, base));

#ifdef DASH
    lowp float len = u_dash_length;
    lowp float r = u_dash_ratio;
    gl_FragColor.a *= smoothstep(len / 2.0 * r + 0.01,
                                 len / 2.0 * r - 0.01,
                                 abs(mod(v_uv.x, len) - len / 2.0));
#endif

#ifdef FADE
    gl_FragColor.a *= v_alpha;
#endif
}

#endif
