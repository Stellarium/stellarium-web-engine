attribute vec4 a_pos;
attribute vec2 a_tex_pos;
attribute vec4 a_color;
attribute vec2 a_shift;

varying vec2 v_tex_pos;
varying vec4 v_color;

uniform vec4 u_color;

void main()
{
    vec4 pos = a_pos;
    pos.xy += 2.0 * a_shift;
    gl_Position = pos;
    v_tex_pos = a_tex_pos;
    v_color = a_color * u_color;
}
