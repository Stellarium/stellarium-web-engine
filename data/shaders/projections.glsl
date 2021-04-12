/* Stellarium Web Engine - Copyright (c) 2021 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Helper to perform different non linear projection.
 *
 * Define one function: vec4 proj(vec3).
 *
 * PROJ should be defined with the id of the projection used.
 */

#define PROJ_PERSPECTIVE        1
#define PROJ_STEROGRAPHIC       2
#define PROJ_MOLLWEIDE          5

#ifndef PROJ
#error PROJ undefined
#endif

uniform highp mat4 u_proj_mat;

#if (PROJ == PROJ_PERSPECTIVE)
highp vec4 proj(highp vec3 pos) {
    return u_proj_mat * vec4(pos, 1.0);
}
#endif

#if (PROJ == PROJ_STEROGRAPHIC)

highp vec4 proj(highp vec3 pos) {
    highp float dist = length(pos);
    highp vec3 p = pos / dist;
    p.xy /= 0.5 * (1.0 - p.z);
    p.z = -1.0;
    p *= dist;
    return u_proj_mat * vec4(p, 1.0);
}

#endif

#if (PROJ == PROJ_MOLLWEIDE)

#define PI 3.14159265
#define MAX_ITER 10
#define PRECISION 1e-7
#define SQRT2 1.4142135623730951

highp vec4 proj(highp vec3 v)
{
    highp float dist = length(v);
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

    highp vec3 p;
    p.xy = vec2(2.0 * SQRT2 / PI * lambda * cos(theta), SQRT2 * sin(theta));
    p.z = -1.0;
    p *= dist;
    vec4 ret = u_proj_mat * vec4(p, 1.0);
    ret.z = 0.0;
    ret.w = 1.0;
    return ret;
}

#endif
