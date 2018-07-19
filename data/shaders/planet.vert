attribute vec4 a_pos;
attribute vec4 a_mpos;
attribute vec2 a_tex_pos;
attribute vec3 a_color;
attribute vec3 a_normal;
attribute vec3 a_tangent;

uniform vec4 u_color;

varying vec3 v_mpos;
varying vec2 v_tex_pos;
varying vec4 v_color;
varying vec3 v_normal;
varying vec3 v_tangent;
varying vec3 v_bitangent;

void main()
{
    gl_Position = a_pos;
    v_mpos = a_mpos.xyz;
    v_tex_pos = a_tex_pos;
    v_color = vec4(a_color, 1.0) * u_color;

    v_normal = normalize(a_normal);
    v_tangent = normalize(a_tangent);
    v_bitangent = normalize(cross(v_normal, v_tangent));

}
