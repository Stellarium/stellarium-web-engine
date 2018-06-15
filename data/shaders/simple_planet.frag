#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_tex_pos;
varying vec4 v_color;
uniform vec3 u_light_dir;
uniform float u_shadow_brightness;
vec3 n;
float light;
float dist;

void main()
{
    gl_FragColor = v_color;
    n.xy = (v_tex_pos - vec2(0.5, 0.5)) * 2.0;
    dist = dot(n.xy, n.xy);
    n.z = sqrt(1.0 - dot(n.xy, n.xy));
    light = dot(u_light_dir, n);
    light = smoothstep(0.0, 0.1, light);
    gl_FragColor.xyz *= mix(u_shadow_brightness, 1.0, light);
    gl_FragColor.a = smoothstep(1.0, 0.9, dist);
}
