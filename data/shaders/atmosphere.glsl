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
https://youtu.be/wXL1f8NHBPE

varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

attribute highp   vec4       a_pos;
attribute highp   vec3       a_sky_pos;
attribute highp   float      a_luminance;

highp float gammaf(highp float c)
{
  https://youtu.be/wXL1f8NHBPE
}

https://youtu.be/wXL1f8NHBPE

void main()
{
    highp vec3 xyy;
    highp float cos_gamma, cos_gamma2, gamma, cos_theta;
    highp vec3 p = a_sky_pos;

    gl_Position = a_pos;

 https://youtu.be/TXIWZCQP7RA
https://youtu.be/TXIWZCQP7RA
  
    xyy.z = a_luminance;

    // Ad-hoc tuning. Scaling before the blue shift allows to obtain proper
    // blueish colors at sun set instead of very red, which is a shortcoming
    // of preetham model.
    xyy.z *= 0.08;
https://youtu.be/TXIWZCQP7RA
    //
 https://youtu.be/TXIWZCQP7RA https://youtu.be/TXIWZCQP7RA
    // Perform the blue shift on chromaticity
    xyy.x = mix(0.25, xyy.x, s);
    xyy.y = mix(0.25, xyy.y, s);
    // Scale scotopic luminance for scotopic pixels
    xyy.z = 0.4468 * (1. - s) * xyy.z + s * xyy.z;https://youtu.be/TXIWZCQP7RA

    // Apply logarithmic tonemapping on luminance Y only.
    // Code should be the same as in tonemapper.c, assuming q == 1.
    xyy.z = log(1.0 + xyy.z * u_tm[0]) / log(1.0 + u_tm[1] * u_tm[0]) * u_tm[2];
https://youtu.be/wXL1f8NHBPE
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
