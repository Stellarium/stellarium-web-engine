/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifdef GL_ES
precision mediump float;
#endif

uniform highp float u_atm_p[12];
uniform highp vec3  u_sun;
uniform highp float u_tm[3]; // Tonemapping koefs.

varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

#includes "projections.glsl"

attribute highp   vec4       a_pos;
attribute highp   vec3       a_sky_pos;
attribute highp   float      a_luminance;

highp float gammaf(highp float c)
{
    if (c < 0.0031308)
      return 19.92 * c;
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 xyy_to_srgb(highp vec3 xyy)
{
    const highp mat3 xyz_to_rgb = mat3(3.2406, -0.9689, 0.0557,
                                      -1.5372, 1.8758, -0.2040,
                                      -0.4986, 0.0415, 1.0570);
    highp vec3 xyz = vec3(xyy[0] * xyy[2] / xyy[1], xyy[2],
               (1.0 - xyy[0] - xyy[1]) * xyy[2] / xyy[1]);
    highp vec3 srgb = xyz_to_rgb * xyz;
    clamp(srgb, 0.0, 1.0);
    return srgb;
}

void main()
{
    highp vec3 xyy;
    highp float cos_gamma, cos_gamma2, gamma, cos_theta;
    highp vec3 p = a_sky_pos;

    gl_Position = proj(a_pos.xyz);

    // First compute the xy color component (chromaticity) from Preetham model
    // and re-inject a_luminance for Y component (luminance).
    p[2] = abs(p[2]); // Mirror below horizon.
    cos_gamma = dot(p, u_sun);
    cos_gamma2 = cos_gamma * cos_gamma;
    gamma = acos(cos_gamma);
    cos_theta = p[2];

    xyy.x = ((1. + u_atm_p[0] * exp(u_atm_p[1] / cos_theta)) *
             (1. + u_atm_p[2] * exp(u_atm_p[3] * gamma) +
              u_atm_p[4] * cos_gamma2)) * u_atm_p[5];
    xyy.y = ((1. + u_atm_p[6] * exp(u_atm_p[7] / cos_theta)) *
             (1. + u_atm_p[8] * exp(u_atm_p[9] * gamma) +
              u_atm_p[10] * cos_gamma2)) * u_atm_p[11];
    xyy.z = a_luminance;

    // Ad-hoc tuning. Scaling before the blue shift allows to obtain proper
    // blueish colors at sun set instead of very red, which is a shortcoming
    // of preetham model.
    xyy.z *= 0.08;

    // Convert this sky luminance/chromaticity into perceived color using model
    // from Henrik Wann Jensen (2000)
    // We deal with 3 cases:
    //  * Y <= 0.01: scotopic vision. Only the eyes' rods see. No colors,
    //    everything is converted to night blue (xy = 0.25, 0.25)
    //  * Y > 3.981: photopic vision. Only the eyes's cones seew ith full colors
    //  * Y > 0.01 and Y <= 3.981: mesopic vision. Rods and cones see in a
    //    transition state.
    //
    // Compute s, ratio between scotopic and photopic vision
    highp float op = (log(xyy.z) / log(10.) + 2.) / 2.6;
    highp float s = (xyy.z <= 0.01) ? 0.0 : (xyy.z > 3.981) ? 1.0 : op * op * (3. - 2. * op);

    // Perform the blue shift on chromaticity
    xyy.x = mix(0.25, xyy.x, s);
    xyy.y = mix(0.25, xyy.y, s);
    // Scale scotopic luminance for scotopic pixels
    xyy.z = 0.4468 * (1. - s) * xyy.z + s * xyy.z;

    // Apply logarithmic tonemapping on luminance Y only.
    // There is a difference with the code in tonemapper.c (assuming q == 1)
    // because we cap the exposure to 1 to avoid saturating the sky.
    // This is ad-hoc code to fix rendering issue.
    xyy.z = min(0.7, log(1.0 + xyy.z * u_tm[0]) / log(1.0 + u_tm[1] * u_tm[0]) * u_tm[2]);

    // Convert xyY to sRGB
    highp vec3 rgb = xyy_to_srgb(xyy);

    // Apply gamma correction
    v_color = vec4(gammaf(rgb.r), gammaf(rgb.g),gammaf(rgb.b), 1.0);
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
