#ifdef GL_ES
precision mediump float;
#endif

uniform float u_smooth;

varying vec2 v_tex_pos;
varying vec4 v_color;

float dist;
float k;

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
