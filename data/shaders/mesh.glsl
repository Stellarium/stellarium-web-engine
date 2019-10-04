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

attribute highp   vec3 a_pos;


// Mollweide projection implementation.
// Note: we should probably put this in a seperate file and support include.
#ifdef PROJ_MOLLWEIDE

uniform highp vec2 u_proj_scaling;

#define PI 3.14159265
#define MAX_ITER 10
#define PRECISION 1e-7

vec2 project(vec3 v)
{
    float phi, lambda, d, k;

    lambda = atan(v[0], -v[2]);
    phi = atan(v[1], sqrt(v[0] * v[0] + v[2] * v[2]));

    k = PI * sin(phi);
    for (int i = 0; i < MAX_ITER; i++) {
        d = 2.0 + 2.0 * cos(2.0 * phi);
        if (abs(d) < PRECISION) break;
        d = (2.0 * phi + sin(2.0 * phi) - k) / d;
        phi -= d;
        if (abs(d) < PRECISION) break;
    }

    return vec2(lambda * cos(phi), sqrt(2.0) * sin(phi)) / u_proj_scaling;
}

#endif


void main()
{
#ifdef PROJ_MOLLWEIDE
    gl_Position = vec4(project(a_pos), 0.0, 1.0);
#else
    gl_Position = vec4(((a_pos.xy / u_fbo_size) * 2.0 - 1.0) * vec2(1.0, -1.0),
                       0.0, 1.0);
#endif
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = u_color;
}

#endif
