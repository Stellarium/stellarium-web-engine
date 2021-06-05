/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#define PI 3.14159265

#ifdef GL_ES
precision mediump float;
#endif

uniform lowp    vec4      u_color;
uniform mediump sampler2D u_tex;
uniform mediump sampler2D u_normal_tex;
uniform mediump mat3      u_tex_transf;
uniform mediump mat3      u_normal_tex_transf;
uniform lowp    vec3      u_light_emit;
uniform lowp    float     u_min_brightness;
uniform mediump mat4      u_mv;  // Model view matrix.
uniform lowp    int       u_has_normal_tex;
uniform lowp    int       u_material; // 0: Oren Nayar, 1: generic, 2: ring
uniform lowp    int       u_is_moon; // Set to 1 for the Moon only.
uniform lowp    float     u_contrast;

uniform highp   vec4      u_sun; // Sun pos (xyz) and radius (w).

#ifdef HAS_SHADOW
// Up to four spheres for illumination ray tracing.
uniform mediump sampler2D u_shadow_color_tex; // Used for the Moon.
uniform lowp    int       u_shadow_spheres_nb;
uniform highp   mat4      u_shadow_spheres;
#endif

varying highp   vec3 v_mpos;
varying mediump vec2 v_tex_pos;
varying mediump vec2 v_normal_tex_pos;
varying lowp    vec4 v_color;
varying highp   vec3 v_normal;
varying highp   vec3 v_tangent;
varying highp   vec3 v_bitangent;

#ifdef VERTEX_SHADER

#includes "projections.glsl"

attribute highp   vec3 a_pos;     // View position (with fake scaling).
attribute highp   vec3 a_mpos;    // Model position (without fake scaling).
attribute mediump vec2 a_tex_pos;
attribute lowp    vec3 a_color;
attribute highp   vec3 a_normal;
attribute highp   vec3 a_tangent;

void main()
{
    gl_Position = proj(a_pos);

    v_mpos = a_mpos;
    v_tex_pos = (u_tex_transf * vec3(a_tex_pos, 1.0)).xy;
    v_normal_tex_pos = (u_normal_tex_transf * vec3(a_tex_pos, 1.0)).xy;
    v_color = vec4(a_color, 1.0) * u_color;

    v_normal = normalize(a_normal);
    v_tangent = normalize(a_tangent);
    v_bitangent = normalize(cross(v_normal, v_tangent));
}

#endif
#ifdef FRAGMENT_SHADER

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
float illumination_sphere(highp vec3 p, highp vec4 sphere,
                          highp vec3 sun_pos, highp float sun_r)
{
    // Sphere angular radius as viewed from the point.
    highp float sph_r = asin(sphere.w / length(sphere.xyz - p));
    // Angle <sun, pos, sphere>
    highp float d = acos(min(1.0, dot(normalize(sun_pos - p),
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
    highp float x = (sun_r * sun_r + d * d - sph_r * sph_r) / (2.0 * d);
    highp float alpha = acos(x / sun_r);
    highp float beta = acos((d - x) / sph_r);
    highp float AR = sun_r * sun_r * (alpha - 0.5 * sin(2.0 * alpha));
    highp float Ar = sph_r * sph_r * (beta - 0.5 * sin(2.0 * beta));
    highp float AS = sun_r * sun_r * 2.0 * 1.57079633;
    return 1.0 - (AR + Ar) / AS;
}

/*
 * Compute the illumination at a given point.
 * Parameters:
 *   p       - The surface point where we compute the illumination.
 */
float illumination(highp vec3 p)
{
#ifndef HAS_SHADOW
    return 1.0;
#else
    if (u_shadow_spheres_nb == 0) return 1.0;
    mediump float ret = 1.0;
    highp float sun_r = asin(u_sun.w / length(u_sun.xyz - p));
    for (int i = 0; i < 4; ++i) {
        if (u_shadow_spheres_nb > i) {
            highp vec4 sphere = u_shadow_spheres[i];
            ret = min(ret, illumination_sphere(p, sphere, u_sun.xyz, sun_r));
        }
    }
    return ret;
#endif
}

void main()
{
    vec3 light_dir = normalize(u_sun.xyz - v_mpos);
    // Compute N in view space
    vec3 n = v_normal;
    if (u_has_normal_tex != 0) {
        n = texture2D(u_normal_tex, v_normal_tex_pos).rgb - vec3(0.5, 0.5, 0.0);
        // XXX: inverse the Y coordinates, don't know why!
        n = +n.x * v_tangent - n.y * v_bitangent + n.z * v_normal;
    }
    n = normalize(n);
    vec4 base_color = texture2D(u_tex, v_tex_pos) * v_color;
    vec3 color = (base_color.rgb - 0.5) * u_contrast + 0.5;

    if (u_material == 0) { // oren_nayar.
        float power = oren_nayar_diffuse(light_dir,
                                         normalize(-v_mpos),
                                         n,
                                         0.9, 0.12);
        #ifdef HAS_SHADOW
        lowp float illu = illumination(v_mpos);
        power *= illu;
        #endif

        power = max(power, u_min_brightness);
        color *= power;

        // Earth shadow effect on the moon.
        #ifdef HAS_SHADOW
        if (u_is_moon == 1 && illu < 0.99) {
            vec4 shadow_col = texture2D(u_shadow_color_tex, vec2(illu, 0.5));
            color = mix(color, shadow_col.rgb, shadow_col.a);
        }
        #endif

    } else if (u_material == 1) { // basic
        float light = max(0.0, dot(n, light_dir));
        light = max(light, u_min_brightness);
        color *= vec3(light) + u_light_emit;

    } else if (u_material == 2) { // ring
        lowp float illu = illumination(v_mpos);
        illu = max(illu, u_min_brightness);
        color *= illu;
    }

    gl_FragColor = vec4(color, base_color.a);
}

#endif
