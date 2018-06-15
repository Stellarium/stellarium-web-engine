attribute vec4 a_pos;
attribute vec2 a_tex_pos;
attribute vec3 a_color;

uniform vec4 u_color;

varying vec2 v_tex_pos;
varying vec4 v_color;

void main()
{
    gl_Position = a_pos;
    v_tex_pos = a_tex_pos;
    v_color = vec4(a_color, 1.0) * u_color;
}
