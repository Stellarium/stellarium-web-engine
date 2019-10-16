/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

uniform   lowp    vec4 u_color;
uniform   lowp    vec2 u_fbo_size;

#ifdef VERTEX_SHADER

attribute highp   vec4 a_pos;


// Mollweide projection implementation.
// Note: we should probably put this in a seperate file and support include.
#ifdef PROJ_MOLLWEIDE

uniform highp vec2 u_proj_scaling;

#define PI 3.14159265
#define MAX_ITER 10
#define PRECISION 1e-7
#define SQRT2 1.4142135623730951

vec2 project(vec3 v)
{
    highp float phi, lambda, theta, d, k;

    lambda = atan(v[0], -v[2]);
    phi = atan(v[1], sqrt(v[0] * v[0] + v[2] * v[2]));

    k = PI * sin(phi);
    theta = phi;
    for (int i = 0; i < MAX_ITER; i++) {
        d = 2.0 + 2.0 * cos(2.0 * theta);
        if (abs(d) < PRECISION) break;
        d = (2.0 * theta + sin(2.0 * theta) - k) / d;
        theta -= d;
        if (abs(d) < PRECISION) break;
    }

    return vec2(2.0 * SQRT2 / PI * lambda * cos(theta),
                SQRT2 * sin(theta)) / u_proj_scaling;
}

#endif


void main()
{
#ifdef PROJ_MOLLWEIDE
    gl_Position = vec4(project(a_pos.xyz), 0.0, 1.0);
#else
    gl_Position = a_pos;
#endif
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = u_color;
}

#endif
