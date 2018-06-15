#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D u_tex;

varying vec2 v_tex_pos;
varying vec4 v_color;

void main()
{
    gl_FragColor = v_color;
    gl_FragColor.a *= texture2D(u_tex, v_tex_pos).r;
}
