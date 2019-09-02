/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "vec.h"
#include "tests.h"

#if !VEC_INLINE
    #define VEC_IMPLEMENTATION
    #include "vec.h"
#endif

static double min(double x, double y)
{
    return x < y ? x : y;
}

void vec3_get_ortho(const double v_[3], double out[3])
{
    int axis;
    double v[3];

    vec3_copy(v_, v);

    // Get dominant axis:
    if (fabs(v[0]) >= fabs(v[1]) && fabs(v[0]) > fabs(v[2])) axis = 0;
    if (fabs(v[1]) >= fabs(v[0]) && fabs(v[1]) > fabs(v[2])) axis = 1;
    if (fabs(v[2]) >= fabs(v[0]) && fabs(v[2]) > fabs(v[1])) axis = 2;

    switch (axis) {
    case 0:
        out[0] = -v[1] - v[2];
        out[1] =  v[0];
        out[2] =  v[0];
        break;
    case 1:
        out[0] =  v[1];
        out[1] = -v[0] - v[2];
        out[2] =  v[1];
        break;
    case 2:
        out[0] =  v[2];
        out[1] =  v[2];
        out[2] = -v[0] - v[1];
        break;
    }
}

void mat3_to_quat(const double m[3][3], double quat[4])
{
    double t;
    if (m[2][2] < 0) {
        if (m[0][0] > m[1][1]) {
            t = 1 + m[0][0] - m[1][1] - m[2][2];
            quat[0] = m[1][2] - m[2][1];
            quat[1] = t;
            quat[2] = m[0][1] + m[1][0];
            quat[3] = m[2][0] + m[0][2];
        } else {
            t = 1 - m[0][0] + m[1][1] - m[2][2];
            quat[0] = m[2][0] - m[0][2];
            quat[1] = m[0][1] + m[1][0];
            quat[2] = t;
            quat[3] = m[1][2] + m[2][1];
        }
    } else {
        if (m[0][0] < -m[1][1]) {
            t = 1 - m[0][0] - m[1][1] + m[2][2];
            quat[0] = m[0][1] - m[1][0];
            quat[1] = m[2][0] + m[0][2];
            quat[2] = m[1][2] + m[2][1];
            quat[3] = t;
        } else {
            t = 1 + m[0][0] + m[1][1] + m[2][2];
            quat[0] = t;
            quat[1] = m[1][2] - m[2][1];
            quat[2] = m[2][0] - m[0][2];
            quat[3] = m[0][1] - m[1][0];
        }
    }
    vec4_mul(0.5 / sqrt(t), quat, quat);
}

double quat_sep(const double a[4], const double b[4])
{
    double f = vec4_dot(a, b);
    return acos(min(fabs(f), 1.0)) * 2.0;
}

void quat_rotate_towards(const double a[4], const double b[4],
                         double max_angle, double out[4])
{
    double t, sep;
    sep = quat_sep(a, b);
    if (sep == 0) return;
    t = min(1.0, max_angle / sep);
    quat_slerp(a, b, t, out);
}

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

static void test_caps(void)
{
    const double p0[3] = {1, 0, 0};
    const double p1[3] = {-1, 0, 0};
    double p2[3] = {1, 1, 1};
    vec3_normalize(p2, p2);
    const double p3[3] = {0, 1, 0};

    double h0[4] = {VEC3_SPLIT(p0), 0};
    double h1[4] = {VEC3_SPLIT(p0), 0.8};
    double h2[4] = {VEC3_SPLIT(p0), -0.5};
    double h3[4] = {VEC3_SPLIT(p1), 0.5};
    double h4[4] = {VEC3_SPLIT(p2), 0.8};
    double h5[4] = {VEC3_SPLIT(p2), 1};
    double h6[4] = {VEC3_SPLIT(p1), 0};

    assert(cap_contains_vec3(h0, p0));
    assert(cap_contains_vec3(h1, p0));
    assert(cap_contains_vec3(h0, p3));
    assert(cap_contains_vec3(h6, p3));

    assert(cap_intersects_cap(h0, h1));
    assert(cap_intersects_cap(h0, h2));
    assert(cap_intersects_cap(h1, h2));
    assert(cap_intersects_cap(h4, h1));
    assert(!cap_intersects_cap(h0, h3));
    assert(!cap_intersects_cap(h1, h3));
    assert(cap_intersects_cap(h2, h3));
    assert(cap_intersects_cap(h0, h5));

    assert(cap_intersects_cap(h0, h0));
    assert(cap_intersects_cap(h1, h1));
    assert(cap_intersects_cap(h2, h2));
    assert(cap_intersects_cap(h3, h3));
    assert(cap_intersects_cap(h4, h4));
    assert(cap_intersects_cap(h5, h5));
    assert(cap_intersects_cap(h6, h0));
    assert(cap_intersects_cap(h0, h6));

    assert(cap_contains_cap(h0, h1));
    assert(!cap_contains_cap(h1, h0));
    assert(cap_contains_cap(h2, h0));
    assert(!cap_contains_cap(h0, h2));
    assert(!cap_contains_cap(h6, h0));
    assert(!cap_contains_cap(h0, h6));
    assert(cap_contains_cap(h2, h1));
    assert(!cap_contains_cap(h1, h2));
    assert(!cap_contains_cap(h0, h3));
    assert(!cap_contains_cap(h1, h3));
    assert(cap_contains_cap(h0, h5));
    assert(cap_contains_cap(h2, h5));
    assert(!cap_contains_cap(h5, h0));
    assert(!cap_contains_cap(h5, h1));
    assert(!cap_contains_cap(h5, h2));
    assert(!cap_contains_cap(h5, h3));
    assert(!cap_contains_cap(h5, h4));
    assert(cap_contains_cap(h0, h0));
    assert(cap_contains_cap(h1, h1));
    assert(cap_contains_cap(h2, h2));
    assert(cap_contains_cap(h3, h3));
    assert(cap_contains_cap(h4, h4));
    assert(cap_contains_cap(h5, h5));

    // Segment completely crosses the cap
    double seg1[3] = {1, 1, 0};
    vec3_normalize(seg1, seg1);
    double seg2[3] = {1, -1, 0};
    vec3_normalize(seg2, seg2);
    assert(cap_intersects_segment(h1, seg1, seg2));

    // Segment fully inside cap
    seg1[1] = 0.1;
    vec3_normalize(seg1, seg1);
    seg2[1] = -0.1;
    vec3_normalize(seg2, seg2);
    assert(cap_intersects_segment(h1, seg1, seg2));

    // Segment outside cap
    seg1[1] = -0.8;
    vec3_normalize(seg1, seg1);
    seg2[1] = -0.9;
    vec3_normalize(seg2, seg2);
    assert(!cap_intersects_segment(h1, seg1, seg2));

    // Segment partially inside cap
    assert(cap_intersects_segment(h1, seg1, h1));

    // Segment great circle aligned with cap
    assert(!cap_intersects_segment(VEC(1, 0, 0, 0.8), VEC(0, 1, 0),
                                   VEC(0, 0, 1)));
    assert(cap_intersects_segment(VEC(1, 0, 0, -0.8), VEC(0, 1, 0),
                                  VEC(0, 0, 1)));
    assert(cap_intersects_segment(VEC(1, 0, 0, 0), VEC(0, 1, 0),
                                  VEC(0, 0, 1)));
}


TEST_REGISTER(NULL, test_caps, TEST_AUTO);

#endif
