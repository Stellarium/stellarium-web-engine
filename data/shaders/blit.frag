#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D u_tex;
uniform float u_stripes;

varying vec2 v_tex_pos;
varying vec4 v_color;

void main()
{
    gl_FragColor = texture2D(u_tex, v_tex_pos) * v_color;

    if (u_stripes != 0.0) {
        float k = v_tex_pos.x;
        k = step(0.5, fract(k * u_stripes));
        gl_FragColor.a *= k;
    }
}
