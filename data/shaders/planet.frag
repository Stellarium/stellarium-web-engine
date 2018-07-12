#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265

uniform sampler2D u_tex;
uniform sampler2D u_normal_tex;
uniform vec3 u_light_emit;
uniform mat4 u_mv;  // Model view matrix.
uniform int u_has_normal_tex;
uniform int u_material; // 0: Oren Nayar, 1: generic
uniform int u_is_moon; // Set to 1 for the Moon only.
uniform sampler2D u_shadow_color_tex; // Used for the Moon.

uniform highp vec4 u_sun; // Sun pos (xyz) and radius (w).
// Up to four spheres for illumination ray tracing.
uniform int u_shadow_spheres_nb;
uniform highp mat4 u_shadow_spheres;

varying vec3 v_vpos;   // Pos in view coordinates.
varying vec2 v_tex_pos;
varying vec4 v_color;
varying vec3 v_normal; // Normal in model coordinates.
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

/*
 * Compute the illumination if we only consider a single sphere in the scene.
 * Parameters:
 *   p       - The surface point where we compute the illumination.
 *   sphere  - A sphere: xyz -> pos, w -> radius.
 *   sun_pos - Position of the sun.
 *   sun_r   - Precomputed sun angular radius from the given point.
 */
float illumination_sphere(vec3 p, vec4 sphere, vec3 sun_pos, float sun_r)
{
    // Sphere angular radius as viewed from the point.
    float sph_r = asin(sphere.w / length(sphere.xyz - p));
    // Angle <sun, pos, sphere>
    float d = acos(min(1.0, dot(normalize(sun_pos - p),
                                normalize(sphere.xyz - p))));

    // Special case for the moon, to simulate lunar eclipses.
    // We assume the only body that can cast shadow on the moon is the Earth.
    if (u_is_moon == 1) {
        if (d >= sun_r + sph_r) return 1.0; // Outside of shadow.
        if (d <= sph_r - sun_r) return d / (sph_r - sun_r) * 0.6; // Umbra.
        if (d <= sun_r - sph_r) // Penumbra completely inside.
            return 1.0 - sph_r * sph_r / (sun_r * sun_r);
        return ((d - abs(sun_r - sph_r)) /
                (sun_r + sph_r - abs(sun_r - sph_r))) * 0.4 + 0.6;
    }

    if (d >= sun_r + sph_r) return 1.0; // Outside of shadow.
    if (d <= sph_r - sun_r) return 0.0; // Umbra.
    if (d <= sun_r - sph_r) // Penumbra completely inside.
        return 1.0 - sph_r * sph_r / (sun_r * sun_r);

    // Penumbra partially inside.
    // I took this from Stellarium, even though I am not sure how it works.
    mediump float x = (sun_r * sun_r + d * d - sph_r * sph_r) / (2.0 * d);
    mediump float alpha = acos(x / sun_r);
    mediump float beta = acos((d - x) / sph_r);
    mediump float AR = sun_r * sun_r * (alpha - 0.5 * sin(2.0 * alpha));
    mediump float Ar = sph_r * sph_r * (beta - 0.5 * sin(2.0 * beta));
    mediump float AS = sun_r * sun_r * 2.0 * 1.57079633;
    return 1.0 - (AR + Ar) / AS;
}

/*
 * Compute the illumination at a given point.
 * Parameters:
 *   p       - The surface point where we compute the illumination.
 */
float illumination(vec3 p)
{
    mediump float ret = 1.0;
    highp float sun_r = asin(u_sun.w / length(u_sun.xyz - p));
    for (int i = 0; i < 4; ++i) {
        if (u_shadow_spheres_nb > i) {
            highp vec4 sphere = u_shadow_spheres[i];
            ret = min(ret, illumination_sphere(p, sphere, u_sun.xyz, sun_r));
        }
    }
    return ret;
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
    if (u_material == 0) { // oren_nayar.
        float power = oren_nayar_diffuse(light_dir,
                                         normalize(-v_vpos),
                                         n,
                                         0.9, 0.12);
        lowp float illu = illumination(v_vpos);
        power *= illu;
        gl_FragColor.rgb *= power;

        // Earth shadow effect on the moon.
        if (u_is_moon == 1 && illu < 0.99) {
            vec4 shadow_col = texture2D(u_shadow_color_tex, vec2(illu, 0.5));
            gl_FragColor.rgb = mix(
                gl_FragColor.rgb, shadow_col.rgb, shadow_col.a);
        }

    } else if (u_material == 1) { // basic
        vec3 light = vec3(0.0, 0.0, 0.0);
        light += max(0.0, dot(n, light_dir));
        light += u_light_emit;
        gl_FragColor.rgb *= light;
    }
}
