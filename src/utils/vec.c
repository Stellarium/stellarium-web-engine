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

#define S static

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

void mat3_product(double out[S 3][3], int n, ...)
{
    int i;
    va_list ap;
    double (*m)[3];

    mat3_set_identity(out);
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        m = va_arg(ap, double (*)[3]);
        mat3_mul(out, m, out);
    }
    va_end(ap);
}

double mat3_det(const double m[S 3][3])
{
    return + m[0][0] * m[1][1] * m[2][2]
           + m[0][1] * m[1][2] * m[2][0]
           + m[0][2] * m[1][0] * m[2][1]
           - m[0][0] * m[1][2] * m[2][1]
           - m[0][1] * m[1][0] * m[2][2]
           - m[0][2] * m[1][1] * m[2][0];
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

bool mat3_invert(const double mat[S 3][3], double out[S 3][3])
{
    double det;
    int i;
    const double *m = (const double *)mat;
    double inv[9];

#define M(i, j) m[i] * m[j]
    inv[0] =   M(4, 8) - M(7, 5);
    inv[3] =  -M(3, 8) + M(6, 5);
    inv[6] =   M(3, 7) - M(6, 4);

    inv[1] =  -M(1, 8) + M(7, 2);
    inv[4] =   M(0, 8) - M(6, 2);
    inv[7] =  -M(0, 7) + M(6, 1);

    inv[2] =   M(1, 5)  - M(4, 2);
    inv[5] =  -M(0, 5)  + M(3, 2);
    inv[8] =   M(0, 4)  - M(3, 1);
#undef M

    det = m[0] * inv[0] + m[1] * inv[3] + m[2] * inv[6];

    if (det == 0) {
        return false;
    }

    det = 1.0 / det;

    for (i = 0; i < 9; i++)
        ((double*)out)[i] = inv[i] * det;
    return true;
}

bool mat4_invert(const double mat[S 4][4], double out[S 4][4])
{
    double det;
    int i;
    const double *m = (const double*)mat;
    double inv[16];

#define M(i, j, k) m[i] * m[j] * m[k]
    inv[0] =   M(5, 10, 15) - M(5, 11, 14)  - M(9, 6, 15) +
               M(9, 7, 14)  + M(13, 6, 11)  - M(13, 7, 10);
    inv[4] =  -M(4, 10, 15) + M( 4, 11, 14) + M( 8, 6, 15) -
               M(8, 7, 14)  - M(12,  6, 11) + M(12, 7, 10);
    inv[8] =   M(4,  9, 15) - M( 4, 11, 13) - M(8,  5, 15) +
               M(8,  7, 13) + M(12,  5, 11) - M(12, 7, 9);
    inv[12] = -M(4, 9, 14)  + M(4, 10, 13)  + M(8, 5, 14) -
               M(8, 6, 13)  - M(12, 5, 10)  + M(12, 6, 9);
    inv[1] =  -M(1, 10, 15) + M(1, 11, 14)  + M(9, 2, 15) -
               M(9, 3, 14)  - M(13, 2, 11)  + M(13, 3, 10);
    inv[5] =   M(0, 10, 15) - M(0, 11, 14)  - M(8, 2, 15) +
               M(8, 3, 14)  + M(12, 2, 11)  - M(12, 3, 10);
    inv[9] =  -M(0, 9, 15)  + M(0, 11, 13)  + M(8, 1, 15) -
               M(8, 3, 13)  - M(12, 1, 11)  + M(12, 3, 9);
    inv[13] =  M(0, 9, 14)  - M(0, 10, 13)  - M(8, 1, 14) +
               M(8, 2, 13)  + M(12, 1, 10)  - M(12, 2, 9);
    inv[2] =   M(1, 6, 15)  - M(1, 7, 14)   - M(5, 2, 15) +
               M(5, 3, 14)  + M(13, 2, 7)   - M(13, 3, 6);
    inv[6] =  -M(0, 6, 15)  + M(0, 7, 14)   + M(4, 2, 15) -
               M(4, 3, 14)  - M(12, 2, 7)   + M(12, 3, 6);
    inv[10] =  M(0, 5, 15)  - M(0, 7, 13)   - M(4, 1, 15) +
               M(4, 3, 13)  + M(12, 1, 7)   - M(12, 3, 5);
    inv[14] = -M(0, 5, 14)  + M(0, 6, 13)   + M(4, 1, 14) -
               M(4, 2, 13)  - M(12, 1, 6)   + M(12, 2, 5);
    inv[3] =  -M(1, 6, 11)  + M(1, 7, 10)   + M(5, 2, 11) -
               M(5, 3, 10)  - M(9, 2, 7)    + M(9, 3, 6);
    inv[7] =   M(0, 6, 11)  - M(0, 7, 10)   - M(4, 2, 11) +
               M(4, 3, 10)  + M(8, 2, 7)    - M(8, 3, 6);
    inv[11] = -M(0, 5, 11)  + M(0, 7, 9)    + M(4, 1, 11) -
               M(4, 3, 9)   - M(8, 1, 7)    + M(8, 3, 5);
    inv[15] =  M(0, 5, 10)  - M(0, 6, 9)    - M(4, 1, 10) +
               M(4, 2, 9)   + M(8, 1, 6)    - M(8, 2, 5);
#undef M

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        return false;
    }

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        ((double*)out)[i] = inv[i] * det;
    return true;
}


void mat4_perspective(double mat[S 4][4], double fovy, double aspect,
                      double nearval, double farval)
{
    double radian = fovy * M_PI / 180;
    double f = 1. / tan(radian / 2.);
    double ret[4][4] = {
        {f / aspect, 0., 0., 0.},
        {0., f, 0., 0.},
        {0., 0., (farval + nearval) / (nearval - farval), -1},
        {0., 0., 2. * farval * nearval / (nearval - farval), 0}
    };
    mat4_copy(ret, mat);
}

/*
 * Perspective projection matrix that puts the far clip at infinity.
 * Idea from 'Projection Matrix Tricks', by Eric Lengyel.
 */
void mat4_inf_perspective(double mat[S 4][4], double fovy, double aspect,
                          double nearval)
{
    double radian = fovy * M_PI / 180;
    double f = 1. / tan(radian / 2.);
    const double eps = FLT_EPSILON;
    double ret[4][4] = {
        {f / aspect, 0., 0., 0.},
        {0., f, 0., 0.},
        {0., 0., eps - 1, -1},
        {0., 0., (eps - 2) * nearval, 0}
    };
    mat4_copy(ret, mat);
}

void mat4_ortho(double mat[S 4][4], double left, double right,
                double bottom, double top, double nearval, double farval)
{
    double tx = -(right + left) / (right - left);
    double ty = -(top + bottom) / (top - bottom);
    double tz = -(farval + nearval) / (farval - nearval);
    const double ret[4][4] = {
        {2 / (right - left), 0, 0, 0},
        {0, 2 / (top - bottom), 0, 0},
        {0, 0, -2 / (farval - nearval), 0},
        {tx, ty, tz, 1},
    };
    mat4_copy(ret, mat);
}


void quat_slerp(const double a_[S 4], const double b_[S 4], double t,
                double out[S 4])
{
    double a[4] = {a_[0], a_[1], a_[2], a_[3]};
    double b[4] = {b_[0], b_[1], b_[2], b_[3]};
    double dot, f1, f2, angle, sin_angle;
    if (t <= 0.0) {
        memcpy(out, a, sizeof(a));
        return;
    } else if (t >= 1.0) {
        memcpy(out, b, sizeof(b));
        return;
    }
    dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot <= 0.0) {
        quat_ineg(b);
        dot = -dot;
    }
    f1 = 1.0 - t;
    f2 = t;
    if ((1.0 - dot) > 0.0000001) {
        angle = acos(dot);
        sin_angle = sin(angle);
        if (sin_angle > 0.0000001) {
            f1 = sin((1.0 - t) * angle) / sin_angle;
            f2 = sin(t * angle) / sin_angle;
        }
    }
    out[0] = a[0] * f1 + b[0] * f2;
    out[1] = a[1] * f1 + b[1] * f2;
    out[2] = a[2] * f1 + b[2] * f2;
    out[3] = a[3] * f1 + b[3] * f2;
}

void cap_great_circle_closest_point(const double cap[S 4],
                                    const double u[S 3], double out[S 3])
{
    double p[3], minusp[3];

    assert(vec3_is_normalized(cap));

    // Look for the point p of the great circle defined by u the closest to cap
    vec3_cross(u, cap, p);
    vec3_cross(u, p, p);

    vec3_normalize(p, p);
    vec3_mul(-1, p, minusp);

    // Pick the correct one from the 2 opposite points
    if (vec3_dot(cap, p) < vec3_dot(cap, minusp)) {
        vec3_mul(-1, p, p);
    }

    vec3_copy(p, out);
}

bool cap_intersects_segment(const double cap[S 4], const double p0[S 3],
                            const double p1[S 3])
{
    // Construct the vec3 u, orthogonal to the great circle containing p0 and p1
    double u[3], k[3], p[3];
    double c;

    vec3_cross(p0, p1, u);

    // Deal with the case where the cap and u are co-linear
    c = vec3_dot(cap, u);
    if (c*c >= vec3_norm2(u))
        return cap[3] <= 0;

    cap_great_circle_closest_point(cap, u, p);

    // If the closest point is not in the cap there is no intersection
    if (!cap_contains_vec3(cap, p))
        return false;

    // Construct the cap with p0 and p1 on its edge
    double cap_geo[4];
    vec3_add(p0, p1, k);
    vec3_normalize(k, cap_geo);
    cap_geo[3] = vec3_dot(cap_geo, p1);

    // If the closest point is in the cap and within the segment boundaries
    // we know that they intersect.
    if (cap_contains_vec3(cap_geo, p))
        return true;

    return cap_contains_vec3(cap, p0) || cap_contains_vec3(cap, p1);
}

static const int EUL_ORDERS[][4] = {
    // i, j, k, n
    {0, 1, 2, 0}, // XYZ
    {0, 2, 1, 1}, // XZY
    {1, 0, 2, 1}, // YXZ
    {1, 2, 0, 0}, // YZX
    {2, 0, 1, 0}, // ZXY
    {2, 1, 0, 1}  // ZYX
};

void eul_to_quat(const double e[S 3], int order, double q[S 4])
{
    const int *r = EUL_ORDERS[order];
    int i = r[0], j = r[1], k = r[2];
    int parity = r[3];
    double a[3];
    double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

    ti = e[i] * 0.5f;
    tj = e[j] * (parity ? -0.5f : 0.5f);
    th = e[k] * 0.5f;
    ci = cos(ti);
    cj = cos(tj);
    ch = cos(th);
    si = sin(ti);
    sj = sin(tj);
    sh = sin(th);
    cc = ci * ch;
    cs = ci * sh;
    sc = si * ch;
    ss = si * sh;

    a[i] = cj * sc - sj * cs;
    a[j] = cj * ss + sj * cc;
    a[k] = cj * cs - sj * sc;

    q[0] = cj * cc + sj * ss;
    q[1] = a[0];
    q[2] = a[1];
    q[3] = a[2];

    if (parity) q[j + 1] = -q[j + 1];
}

void quat_to_eul(const double q[S 4], int order, double e[S 3])
{
    double m[3][3];
    quat_to_mat3(q, m);
    mat3_to_eul(m, order, e);
}

void mat3_normalize_(const double m[S 3][3], double out[S 3][3])
{
    int i;
    for (i = 0; i < 3; i++)
        vec3_normalize(m[i], out[i]);
}

void mat3_to_eul2(const double m[3][3], int order, double e1[3], double e2[3])
{

    const int *r = EUL_ORDERS[order];
    int i = r[0], j = r[1], k = r[2];
    int parity = r[3];
    double cy = hypot(m[i][i], m[i][j]);
    double n[3][3];

    mat3_normalize_(m, n);
    if (cy > 16.0f * DBL_EPSILON) {
        e1[i] = atan2(m[j][k], m[k][k]);
        e1[j] = atan2(-m[i][k], cy);
        e1[k] = atan2(m[i][j], m[i][i]);
        e2[i] = atan2(-m[j][k], -m[k][k]);
        e2[j] = atan2(-m[i][k], -cy);
        e2[k] = atan2(-m[i][j], -m[i][i]);
    } else {
        e1[i] = atan2(-m[k][j], m[j][j]);
        e1[j] = atan2(-m[i][k], cy);
        e1[k] = 0.0;
        vec3_copy(e1, e2);
    }
    if (parity) {
        vec3_mul(-1, e1, e1);
        vec3_mul(-1, e2, e2);
    }
}

void mat3_to_eul(const double m[3][3], int order, double e[3])
{
    double e1[3], e2[3];
    mat3_to_eul2(m, order, e1, e2);

    // Pick best.
    if (    fabs(e1[0]) + fabs(e1[1]) + fabs(e1[2]) >
            fabs(e2[0]) + fabs(e2[1]) + fabs(e2[2])) {
        vec3_copy(e2, e);
    } else {
        vec3_copy(e1, e);
    }
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
