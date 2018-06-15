/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifdef VEC_INLINE
    #define DEF static inline
#else
    #define DEF
#endif

#ifndef VEC_IMPLEMENTATION

#define VEC(...) ((const double[]){__VA_ARGS__})
#define VEC2_SPLIT(v) (v)[0], (v)[1]
#define VEC3_SPLIT(v) (v)[0], (v)[1], (v)[2]
#define VEC4_SPLIT(v) (v)[0], (v)[1], (v)[2], (v)[3]

static const double mat3_identity[3][3] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
};

static const double mat4_identity[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
};

DEF void vec2_set(double v[2], double x, double y);
DEF void vec3_set(double v[3], double x, double y, double z);
DEF void vec4_set(double v[4], double x, double y, double z, double w);
DEF void vec2_copy(const double v[2], double out[2]);
DEF void vec3_copy(const double v[3], double out[3]);
DEF void vec4_copy(const double v[3], double out[3]);
DEF bool vec3_equal(const double a[3], const double b[3]);
DEF bool vec4_equal(const double a[4], const double b[4]);
DEF void vec2_to_float(const double a[2], float *out);
DEF void vec3_to_float(const double a[3], float *out);
DEF void vec4_to_float(const double a[4], float *out);
DEF void vec2_add(const double a[2], const double b[2], double c[2]);
DEF void vec3_add(const double a[3], const double b[3], double c[3]);
DEF void vec2_addk(const double a[2], const double b[2], double k,
                   double c[2]);
DEF void vec3_addk(const double a[3], const double b[3], double k,
                   double c[3]);
DEF void vec2_sub(const double a[2], const double b[2], double c[2]);
DEF void vec3_sub(const double a[3], const double b[3], double c[3]);
DEF void vec2_mul(double k, const double v[2], double out[2]);
DEF void vec3_mul(double k, const double v[3], double out[3]);
DEF void vec4_mul(double k, const double v[4], double out[4]);
DEF void vec4_emul(const double a[4], const double b[4], double out[4]);
DEF double vec2_norm2(const double v[2]);
DEF double vec3_norm2(const double v[3]);
DEF double vec2_norm(const double v[2]);
DEF double vec3_norm(const double v[3]);
DEF void vec2_normalize(const double v[2], double out[2]);
DEF void vec3_normalize(const double v[3], double out[3]);
DEF double vec2_dot(const double a[2], const double b[2]);
DEF double vec3_dot(const double a[3], const double b[3]);
DEF double vec2_dist2(const double a[2], const double b[2]);
DEF double vec3_dist2(const double a[3], const double b[3]);
DEF double vec2_dist(const double a[2], const double b[2]);
DEF double vec3_dist(const double a[3], const double b[3]);
DEF void vec2_mix(const double a[2], const double b[2], double k,
                  double out[2]);
DEF void vec3_mix(const double a[3], const double b[3], double k,
                  double out[3]);
DEF void vec4_mix(const double a[4], const double b[4], double k,
                  double out[4]);
DEF void vec3_cross(const double a[3], const double b[3], double out[3]);
DEF void vec2_rotate(double angle, const double a[2], double out[2]);

DEF void mat3_copy(double src[3][3], double out[3][3]);
DEF void mat3_set_identity(double mat[3][3]);
DEF void mat3_mul(double a[3][3], double b[3][3], double out[3][3]);
DEF void mat3_mul_vec3(double mat[3][3], const double v[3], double out[3]);
DEF void mat3_mul_vec2(double mat[3][3], const double v[2], double out[2]);
DEF void mat3_rx(double a, double mat[3][3], double out[3][3]);
DEF void mat3_ry(double a, double mat[3][3], double out[3][3]);
DEF void mat3_rz(double a, double mat[3][3], double out[3][3]);
DEF void mat3_itranslate(double m[3][3], double x, double y);
DEF void mat3_iscale(double m[3][3], double x, double y);
DEF bool mat3_invert(double mat[3][3], double out[3][3]);
DEF void mat3_transpose(double mat[3][3], double out[3][3]);
DEF void mat3_to_mat4(double mat[3][3], double out[4][4]);
DEF void mat3_to_float(double mat[3][3], float out[9]);
DEF void mat3_to_float4(double mat[3][3], float out[16]);

DEF void mat4_mul_vec4(double mat[4][4], const double v[4], double out[4]);
DEF void mat4_mul_vec3(double mat[4][4], const double v[3], double out[3]);
DEF void mat4_mul_vec3_dir(double mat[4][4], const double v[3], double out[3]);
DEF void mat4_copy(double src[4][4], double out[4][4]);
DEF void mat4_perspective(double mat[4][4], double fovy, double aspect,
                          double nearval, double farval);
DEF void mat4_to_float(double mat[4][4], float out[16]);
DEF void mat4_set_identity(double mat[4][4]);
DEF void mat4_mul(double a[4][4], double b[4][4], double out[4][4]);
DEF void mat4_rx(double a, double mat[4][4], double out[4][4]);
DEF void mat4_ry(double a, double mat[4][4], double out[4][4]);
DEF void mat4_rz(double a, double mat[4][4], double out[4][4]);
DEF void mat4_itranslate(double m[4][4], double x, double y, double z);
DEF void mat4_iscale(double m[4][4], double x, double y, double z);
DEF bool mat4_invert(double mat[4][4], double out[4][4]);
DEF void mat4_transpose(double mat[4][4], double out[4][4]);

DEF void quat_set_identity(double q[4]);
DEF void quat_ineg(double q[4]);
DEF void quat_rx(double a, const double q[4], double out[4]);
DEF void quat_ry(double a, const double q[4], double out[4]);
DEF void quat_rz(double a, const double q[4], double out[4]);
DEF void quat_mul(const double a[4], const double b[4], double out[4]);
DEF void quat_mul_vec3(const double q[4], const double v[3], double out[3]);
DEF void quat_to_mat3(const double q[4], double out[3][3]);
DEF void quat_slerp(const double a[4], const double b[4], double t,
                    double out[4]);

#endif

#if defined(VEC_INLINE) || defined(VEC_IMPLEMENTATION)

DEF void vec2_set(double v[2], double x, double y)
{
    v[0] = x;
    v[1] = y;
}

DEF void vec3_set(double v[3], double x, double y, double z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

DEF void vec4_set(double v[4], double x, double y, double z, double w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

DEF void vec2_copy(const double v[2], double out[2])
{
    memcpy(out, v, 2 * sizeof(*v));
}

DEF void vec3_copy(const double v[3], double out[3])
{
    memcpy(out, v, 3 * sizeof(*v));
}

DEF void vec4_copy(const double v[3], double out[3])
{
    memcpy(out, v, 4 * sizeof(*v));
}

DEF bool vec3_equal(const double a[3], const double b[3])
{
    return memcmp(a, b, 3 * sizeof(double)) == 0;
}

DEF bool vec4_equal(const double a[4], const double b[4])
{
    return memcmp(a, b, 4 * sizeof(double)) == 0;
}

DEF void vec2_to_float(const double a[2], float *out)
{
    out[0] = a[0];
    out[1] = a[1];
}

DEF void vec3_to_float(const double a[3], float *out)
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
}

DEF void vec4_to_float(const double a[4], float *out)
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
    out[3] = a[3];
}

DEF void vec2_addk(const double a[2], const double b[2], double k, double c[2])
{
    c[0] = a[0] + k * b[0];
    c[1] = a[1] + k * b[1];
}

DEF void vec3_addk(const double a[3], const double b[3], double k, double c[3])
{
    c[0] = a[0] + k * b[0];
    c[1] = a[1] + k * b[1];
    c[2] = a[2] + k * b[2];
}

DEF void vec2_add(const double a[2], const double b[2], double c[2])
{
    c[0] = a[0] + b[0];
    c[1] = a[1] + b[1];
}

DEF void vec3_add(const double a[3], const double b[3], double c[3])
{
    c[0] = a[0] + b[0];
    c[1] = a[1] + b[1];
    c[2] = a[2] + b[2];
}

DEF void vec2_sub(const double a[2], const double b[2], double c[2])
{
    c[0] = a[0] - b[0];
    c[1] = a[1] - b[1];
}

DEF void vec3_sub(const double a[3], const double b[3], double c[3])
{
    c[0] = a[0] - b[0];
    c[1] = a[1] - b[1];
    c[2] = a[2] - b[2];
}

DEF void vec2_mul(double k, const double v[2], double out[2])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
}

DEF void vec3_mul(double k, const double v[3], double out[3])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
    out[2] = k * v[2];
}

DEF void vec4_mul(double k, const double v[4], double out[4])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
    out[2] = k * v[2];
    out[3] = k * v[3];
}

DEF void vec4_emul(const double a[4], const double b[4], double out[4])
{
    out[0] = a[0] * b[0];
    out[1] = a[1] * b[1];
    out[2] = a[2] * b[2];
    out[3] = a[3] * b[3];
}

DEF double vec2_norm2(const double v[2])
{
    return v[0] * v[0] + v[1] * v[1];
}

DEF double vec3_norm2(const double v[3])
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

DEF double vec2_norm(const double v[2])
{
    return sqrt(vec2_norm2(v));
}

DEF double vec3_norm(const double v[3])
{
    return sqrt(vec3_norm2(v));
}

DEF void vec2_normalize(const double v[2], double out[2])
{
    double n = vec2_norm(v);
    vec2_mul(1.0 / n, v, out);
}

DEF void vec3_normalize(const double v[3], double out[3])
{
    double n = vec3_norm(v);
    vec3_mul(1.0 / n, v, out);
}

DEF double vec2_dot(const double a[2], const double b[2])
{
    return a[0] * b[0] + a[1] * b[1];
}

DEF double vec3_dot(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

DEF double vec2_dist2(const double a[2], const double b[2])
{
    double c[2];
    vec2_sub(a, b, c);
    return vec2_norm2(c);
}

DEF double vec3_dist2(const double a[3], const double b[3])
{
    double c[3];
    vec3_sub(a, b, c);
    return vec3_norm2(c);
}

DEF double vec2_dist(const double a[2], const double b[2])
{
    return sqrt(vec2_dist2(a, b));
}

DEF double vec3_dist(const double a[3], const double b[3])
{
    return sqrt(vec3_dist2(a, b));
}

DEF void vec2_mix(const double a[2], const double b[2], double k,
                  double out[2])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
}

DEF void vec3_mix(const double a[3], const double b[3], double k,
                  double out[3])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
    out[2] = a[2] * (1.0 - k) + b[2] * k;
}

DEF void vec4_mix(const double a[4], const double b[4], double k,
                  double out[4])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
    out[2] = a[2] * (1.0 - k) + b[2] * k;
    out[3] = a[3] * (1.0 - k) + b[3] * k;
}

DEF void vec3_cross(const double a[3], const double b[3], double out[3])
{
    double tmp[3];
    tmp[0] = a[1] * b[2] - a[2] * b[1];
    tmp[1] = a[2] * b[0] - a[0] * b[2];
    tmp[2] = a[0] * b[1] - a[1] * b[0];
    vec3_copy(tmp, out);
}

DEF void vec2_rotate(double angle, const double a[2], double out[2])
{
    double x, y;
    x = a[0] * cos(angle) - a[1] * sin(angle);
    y = a[0] * sin(angle) + a[1] * cos(angle);
    out[0] = x;
    out[1] = y;
}

DEF void mat3_to_mat4(double mat[3][3], double out[4][4])
{
    int i, j;
    memset(out, 0, 16 * sizeof(double));
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i][j] = mat[i][j];
    out[3][3] = 1.0;
}

DEF void mat3_mul_vec3(double mat[3][3], const double v[3], double out[3])
{
    double x = v[0];
    double y = v[1];
    double z = v[2];
    out[0] = x * mat[0][0] + y * mat[1][0] + z * mat[2][0];
    out[1] = x * mat[0][1] + y * mat[1][1] + z * mat[2][1];
    out[2] = x * mat[0][2] + y * mat[1][2] + z * mat[2][2];
}

DEF void mat3_mul_vec2(double mat[3][3], const double v[2], double out[2])
{
    double tmp[3] = {v[0], v[1], 1.0};
    double ret[3];
    mat3_mul_vec3(mat, tmp, ret);
    vec2_copy(ret, out);
}

DEF void mat4_mul_vec4(double mat[4][4], const double v[4], double out[4])
{
    double ret[4] = {};
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret[i] += mat[j][i] * v[j];
        }
    }
    vec4_copy(ret, out);
}

DEF void mat4_mul_vec3(double mat[4][4], const double v[3], double out[3])
{
    double tmp[4] = {v[0], v[1], v[2], 1.0};
    double ret[4];
    mat4_mul_vec4(mat, tmp, ret);
    vec3_copy(ret, out);
}

DEF void mat4_mul_vec3_dir(double mat[4][4], const double v[3], double out[3])
{
    double tmp[4] = {v[0], v[1], v[2], 0.0};
    double ret[4];
    mat4_mul_vec4(mat, tmp, ret);
    vec3_copy(ret, out);
}


DEF void mat3_copy(double src[3][3], double out[3][3])
{
    memcpy(out, src, 3 * 3 * sizeof(src[0][0]));
}

DEF void mat4_copy(double src[4][4], double out[4][4])
{
    memcpy(out, src, 4 * 4 * sizeof(src[0][0]));
}

DEF void mat4_perspective(double mat[4][4], double fovy, double aspect,
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

DEF void mat3_to_float(double mat[3][3], float out[9])
{
    int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i * 3 + j] = mat[i][j];
}

DEF void mat3_to_float4(double mat[3][3], float out[16])
{
    int i, j;
    memset(out, 0, 16 * sizeof(float));
    out[15] = 1.0f;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i * 4 + j] = mat[i][j];
}

DEF void mat4_to_float(double mat[4][4], float out[16])
{
    int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++)
        out[i * 4 + j] = mat[i][j];
}

DEF void mat3_set_identity(double mat[3][3])
{
    memset(mat, 0, 9 * sizeof(double));
    mat[0][0] = 1.0;
    mat[1][1] = 1.0;
    mat[2][2] = 1.0;
}

DEF void mat4_set_identity(double mat[4][4])
{
    memcpy(mat, mat4_identity, sizeof(mat4_identity));
}

DEF void mat3_mul(double a[3][3], double b[3][3], double out[3][3])
{
    int i, j, k;
    double ret[3][3] = {{0}};
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 3; ++k) {
                ret[j][i] += a[k][i] * b[j][k];
            }
        }
    }
    mat3_copy(ret, out);
}

DEF void mat4_mul(double a[4][4], double b[4][4], double out[4][4])
{
    int i, j, k;
    double ret[4][4] = {{0}};
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; ++k) {
                ret[j][i] += a[k][i] * b[j][k];
            }
        }
    }
    mat4_copy(ret, out);
}

DEF void mat3_rx(double a, double mat[3][3], double out[3][3])
{
    double tmp[3][3];
    double s = sin(a);
    double c = cos(a);
    mat3_set_identity(tmp);
    tmp[1][1] = c;
    tmp[2][2] = c;
    tmp[2][1] = -s;
    tmp[1][2] = s;
    mat3_mul(mat, tmp, out);
}

DEF void mat3_ry(double a, double mat[3][3], double out[3][3])
{
    double tmp[3][3];
    double s = sin(a);
    double c = cos(a);
    mat3_set_identity(tmp);
    tmp[0][0] = c;
    tmp[2][2] = c;
    tmp[2][0] = s;
    tmp[0][2] = -s;
    mat3_mul(mat, tmp, out);
}

DEF void mat3_rz(double a, double mat[3][3], double out[3][3])
{
    double tmp[3][3];
    double s = sin(a);
    double c = cos(a);
    mat3_set_identity(tmp);
    tmp[0][0] = c;
    tmp[1][1] = c;
    tmp[1][0] = -s;
    tmp[0][1] = s;
    mat3_mul(mat, tmp, out);
}

DEF void mat4_rx(double a, double mat[4][4], double out[4][4])
{
    double tmp[4][4];
    double s = sin(a);
    double c = cos(a);
    mat4_set_identity(tmp);
    tmp[1][1] = c;
    tmp[2][2] = c;
    tmp[2][1] = -s;
    tmp[1][2] = s;
    mat4_mul(mat, tmp, out);
}

DEF void mat4_ry(double a, double mat[4][4], double out[4][4])
{
    double tmp[4][4];
    double s = sin(a);
    double c = cos(a);
    mat4_set_identity(tmp);
    tmp[0][0] = c;
    tmp[2][2] = c;
    tmp[2][0] = s;
    tmp[0][2] = -s;
    mat4_mul(mat, tmp, out);
}

DEF void mat4_rz(double a, double mat[4][4], double out[4][4])
{
    double tmp[4][4];
    double s = sin(a);
    double c = cos(a);
    mat4_set_identity(tmp);
    tmp[0][0] = c;
    tmp[1][1] = c;
    tmp[1][0] = -s;
    tmp[0][1] = s;
    mat4_mul(mat, tmp, out);
}

DEF void mat3_itranslate(double m[3][3], double x, double y)
{
    int i;
    for (i = 0; i < 3; i++)
       m[2][i] += m[0][i] * x + m[1][i] * y;
}

DEF void mat4_itranslate(double m[4][4], double x, double y, double z)
{
    int i;
    for (i = 0; i < 4; i++)
       m[3][i] += m[0][i] * x + m[1][i] * y + m[2][i] * z;
}

DEF void mat3_iscale(double m[3][3], double x, double y)
{
    int i;
    for (i = 0; i < 3; i++) {
        m[0][i] *= x;
        m[1][i] *= y;
    }
}

DEF void mat4_iscale(double m[4][4], double x, double y, double z)
{
    int i;
    for (i = 0; i < 4; i++) {
        m[0][i] *= x;
        m[1][i] *= y;
        m[2][i] *= z;
    }
}

DEF bool mat3_invert(double mat[3][3], double out[3][3])
{
    double det;
    int i;
    const double *m = (void*)mat;
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

DEF bool mat4_invert(double mat[4][4], double out[4][4])
{
    double det;
    int i;
    double *m = (void*)mat;
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

DEF void mat3_transpose(double mat[3][3], double out[3][3])
{
    double tmp[3][3];
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            tmp[i][j] = mat[j][i];
        }
    }
    mat3_copy(tmp, out);
}

DEF void mat4_transpose(double mat[4][4], double out[4][4])
{
    double tmp[4][4];
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[i][j] = mat[j][i];
        }
    }
    mat4_copy(tmp, out);
}

DEF void quat_set_identity(double q[4])
{
    q[0] = 0.0;
    q[1] = 0.0;
    q[2] = 0.0;
    q[3] = 1.0;
}

DEF void quat_from_axis(double q[4], double a, double x, double y, double z)
{
    double sin_angle;
    double vn[3] = {x, y, z};
    a *= 0.5;
    vec3_normalize(vn, vn);
    sin_angle = sin(a);
    q[0] = vn[0] * sin_angle;
    q[1] = vn[1] * sin_angle;
    q[2] = vn[2] * sin_angle;
    q[3] = cos(a);
}

DEF void quat_mul(const double a[4], const double b[4], double out[4])
{
    double ax, ay, az, aw, bx, by, bz, bw;
    ax = a[0]; ay = a[1]; az = a[2]; aw = a[3];
    bx = b[0]; by = b[1]; bz = b[2]; bw = b[3];
    out[0] = aw * bx + ax * bw + ay * bz - az * by;
    out[1] = aw * by + ay * bw + az * bx - ax * bz;
    out[2] = aw * bz + az * bw + ax * by - ay * bx;
    out[3] = aw * bw - ax * bx - ay * by - az * bz;
}

DEF void quat_mul_vec3(const double q[4], const double v[3], double out[3])
{
    double m[3][3];
    quat_to_mat3(q, m);
    mat3_mul_vec3(m, v, out);
}

DEF void quat_to_mat3(const double q[4], double out[3][3])
{
    double x, y, z, w;
    x = q[0]; y = q[1]; z = q[2]; w = q[3];
    out[0][0] = 1 - 2 * y * y - 2 * z * z;
    out[0][1] = 2 *x *y + 2 * z * w;
    out[0][2] = 2 *x *z - 2 * y * w;
    out[1][0] = 2 *x *y - 2 * z * w;
    out[1][1] = 1 -2 *x * x - 2 * z * z;
    out[1][2] = 2 * y * z + 2 * x * w;
    out[2][0] = 2 * x * z + 2 * y * w;
    out[2][1] = 2 * y * z - 2 * x * w;
    out[2][2] = 1 - 2 * x * x - 2 * y * y;
}

DEF void quat_ineg(double q[4])
{
    q[0] = -q[0];
    q[1] = -q[1];
    q[2] = -q[2];
    q[3] = -q[3];
}

DEF void quat_rx(double a, const double q[4], double out[4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 1.0, 0.0, 0.0);
    quat_mul(q, tmp, out);
}

DEF void quat_ry(double a, const double q[4], double out[4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 0.0, 1.0, 0.0);
    quat_mul(q, tmp, out);
}

DEF void quat_rz(double a, const double q[4], double out[4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 0.0, 0.0, 1.0);
    quat_mul(q, tmp, out);
}

DEF void quat_slerp(const double a_[4], const double b_[4], double t,
                    double out[4])
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

#endif
