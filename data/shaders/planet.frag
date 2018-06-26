#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265

uniform sampler2D u_tex;
uniform sampler2D u_normal_tex;
uniform vec3 u_light_emit;
uniform mat4 u_mv;
uniform int u_has_normal_tex;

uniform highp vec4 u_sun; // Sun pos (xyz) and radius (w).

varying vec3 v_vpos;
varying vec2 v_tex_pos;
varying vec4 v_color;
varying vec3 v_normal;
varying vec3 v_tangent;
varying vec3 v_bitangent;

float oren_nayar_diffuse(
        vec3 lightDirection,
        vec3 viewDirection,
        vec3 surfaceNormal,
        float roughness,
        float albedo) {

    float r2 = roughness * roughness;
    float LdotV = dot(lightDirection, viewDirection);
    float NdotL = dot(lightDirection, surfaceNormal);
    float NdotV = dot(surfaceNormal, viewDirection);
    float NaL = acos(NdotL);
    float NaV = acos(NdotV);
    float alpha = max(NaV, NaL);
    float beta = min(NaV, NaL);
    float gamma = dot(viewDirection - surfaceNormal * NdotV,
                      lightDirection - surfaceNormal * NdotL);
    float A = 1.0 - 0.5 * (r2 / (r2 + 0.33));
    float B = 0.45 * r2 / (r2 + 0.09);
    float C = sin(alpha) * tan(beta);
    float scale = 1.6; // Empirical value!
    return max(0.0, NdotL) * (A + B * max(0.0, gamma) * C) * scale;
}

void main()
{
    vec3 light_dir = normalize(u_sun.xyz - v_vpos);
    // Compute N in view space
    vec3 n = v_normal;
    if (u_has_normal_tex != 0) {
        n = texture2D(u_normal_tex, v_tex_pos).rgb - vec3(0.5, 0.5, 0.0);
        n.z *= 0.2; // XXX: amplify the normal map!
        // XXX: inverse the Y coordinates, don't know why!
        n = normalize(+n.x * v_tangent - n.y * v_bitangent + n.z * v_normal);
    }
    n = normalize((u_mv * vec4(n, 0.0)).xyz);
    gl_FragColor = texture2D(u_tex, v_tex_pos) * v_color;
#ifndef NO_OREN_NAYAR
    float power = oren_nayar_diffuse(light_dir,
                                     normalize(-v_vpos),
                                     n,
                                     0.9, 0.12);
    gl_FragColor.rgb *= power;
#else
    vec3 light = vec3(0.0, 0.0, 0.0);
    light += max(0.0, dot(n, light_dir));
    light += u_light_emit;
    gl_FragColor.rgb *= light;
#endif
}
