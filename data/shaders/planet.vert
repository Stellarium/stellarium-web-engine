attribute vec4 a_pos;
attribute vec4 a_vpos;
attribute vec2 a_tex_pos;
attribute vec3 a_color;
attribute vec3 a_normal;
attribute vec3 a_tangent;

uniform vec4 u_color;

varying vec3 v_vpos;
varying vec2 v_tex_pos;
varying vec4 v_color;
varying vec3 v_normal;
varying vec3 v_tangent;
varying vec3 v_bitangent;

void main()
{
    gl_Position = a_pos;
    v_vpos = a_vpos.xyz;
    v_tex_pos = a_tex_pos;
    v_color = vec4(a_color, 1.0) * u_color;

    // XXX: for the moment we use spherical coordinates tangent,
    // like in stellarium.
    v_normal = a_normal;
    v_tangent = normalize(cross(vec3(0.0, 0.0, 1.0), v_normal));
    // v_tangent = a_tangent;
    v_bitangent = normalize(cross(v_normal, v_tangent));

}
