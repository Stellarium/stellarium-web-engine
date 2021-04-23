/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef VEC_H
#define VEC_H

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#define INL static inline

// S macro for C99 static argument array size.
#ifndef __cplusplus
#define S static
#else
#define S
#endif

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

#define MAT3_IDENTITY {{1, 0, 0}, \
                       {0, 1, 0}, \
                       {0, 0, 1}}

#define MAT4_IDENTITY {{1, 0, 0, 0}, \
                       {0, 1, 0, 0}, \
                       {0, 0, 1, 0}, \
                       {0, 0, 0, 1}}

#define QUAT_IDENTITY {1, 0, 0, 0}

/*
 * Some common functions defined as macro so that they work with both
 * double and float (for both arguments).
 */

#define vec2_copy(v, out) do { \
        (out)[0] = (v)[0]; \
        (out)[1] = (v)[1]; } while (0)

#define vec3_copy(v, out) do { \
        (out)[0] = (v)[0]; \
        (out)[1] = (v)[1]; \
        (out)[2] = (v)[2]; } while (0)

#define vec4_copy(v, out) do { \
        (out)[0] = (v)[0]; \
        (out)[1] = (v)[1]; \
        (out)[2] = (v)[2]; \
        (out)[3] = (v)[3]; } while (0)

#define vec2_set(v, x, y) do { \
        (v)[0] = x; (v)[1] = y; } while (0)

#define vec3_set(v, x, y, z) do { \
        (v)[0] = x; (v)[1] = y; (v)[2] = z; } while (0)

INL void vec4_set(double v[S 4], double x, double y, double z, double w);
INL bool vec3_equal(const double a[S 3], const double b[S 3]);
INL bool vec4_equal(const double a[S 4], const double b[S 4]);
INL void vec2_to_float(const double a[S 2], float out[S 2]);
INL void vec3_to_float(const double a[S 3], float out[S 3]);
INL void vec4_to_float(const double a[S 4], float out[S 4]);
INL void vec2_add(const double a[S 2], const double b[S 2], double c[S 2]);
INL void vec3_add(const double a[S 3], const double b[S 3], double c[S 3]);
INL void vec2_addk(const double a[S 2], const double b[S 2], double k,
                   double c[S 2]);
INL void vec3_addk(const double a[S 3], const double b[S 3], double k,
                   double c[S 3]);
INL void vec2_sub(const double a[S 2], const double b[S 2], double c[S 2]);
INL void vec3_sub(const double a[S 3], const double b[S 3], double c[S 3]);
INL void vec2_mul(double k, const double v[S 2], double out[S 2]);
INL void vec3_mul(double k, const double v[S 3], double out[S 3]);
INL void vec4_mul(double k, const double v[S 4], double out[S 4]);
INL void vec4_emul(const double a[S 4], const double b[S 4], double out[S 4]);
INL double vec2_norm2(const double v[S 2]);
INL double vec3_norm2(const double v[S 3]);
INL double vec2_norm(const double v[S 2]);
INL double vec3_norm(const double v[S 3]);
INL void vec2_normalize(const double v[S 2], double out[S 2]);
INL void vec3_normalize(const double v[S 3], double out[S 3]);
INL double vec2_dot(const double a[S 2], const double b[S 2]);
INL double vec3_dot(const double a[S 3], const double b[S 3]);
INL double vec4_dot(const double a[S 4], const double b[S 4]);
INL double vec2_dist2(const double a[S 2], const double b[S 2]);
INL double vec3_dist2(const double a[S 3], const double b[S 3]);
INL double vec2_dist(const double a[S 2], const double b[S 2]);
INL double vec3_dist(const double a[S 3], const double b[S 3]);
INL void vec2_mix(const double a[S 2], const double b[S 2], double k,
                  double out[S 2]);
INL void vec3_mix(const double a[S 3], const double b[S 3], double k,
                  double out[S 3]);
INL void vec4_mix(const double a[S 4], const double b[S 4], double k,
                  double out[S 4]);
INL double vec2_cross(const double a[S 2], const double b[S 2]);
INL void vec3_cross(const double a[S 3], const double b[S 3], double out[S 3]);
INL void vec2_rotate(double angle, const double a[S 2], double out[S 2]);
INL bool vec3_is_normalized(const double v[S 3]);
void vec3_get_ortho(const double v[S 3], double out[S 3]);

INL bool mat2_invert(const double mat[S 2][2], double out[S 2][2]);

INL void mat3_copy(const double src[S 3][3], double out[S 3][3]);
INL void mat3_set_identity(double mat[S 3][3]);
INL void mat3_mul(const double a[3][3], const double b[S 3][3],
                  double out[S 3][3]);

/*
 * Function: mat3_product
 * Compute the product of a list of 3x3 matrices.
 *
 * This is a conveniance function that chains calls to mat3_mul.
 * mat3_product(out, 3, a, b, c) is equivalent to 'out = a * b * c'.
 */
void mat3_product(double out[S 3][3], int n, ...);

INL void mat3_mul_vec3(const double mat[S 3][3], const double v[S 3],
                       double out[S 3]);
INL void mat3_mul_vec3_transposed(const double mat[S 3][3], const double v[S 3],
                                  double out[S 3]);
INL void mat3_mul_vec2(const double mat[S 3][3], const double v[S 2],
                       double out[S 2]);
INL void mat3_rx(double a, const double mat[S 3][3], double out[S 3][3]);
INL void mat3_ry(double a, const double mat[S 3][3], double out[S 3][3]);
INL void mat3_rz(double a, const double mat[S 3][3], double out[S 3][3]);
INL void mat3_itranslate(double m[S 3][3], double x, double y);
INL void mat3_iscale(double m[S 3][3], double x, double y, double z);
bool mat3_invert(const double mat[S 3][3], double out[S 3][3]);
INL void mat3_transpose(const double mat[S 3][3], double out[S 3][3]);
INL void mat3_to_mat4(const double mat[S 3][3], double out[S 4][4]);
INL void mat3_to_float(const double mat[S 3][3], float out[S 9]);
INL void mat3_to_float4(const double mat[S 3][3], float out[S 16]);
    void mat3_to_quat(const double mat[S 3][3], double quat[S 4]);

/*
 * Function: mat3_det
 * Compute the determinant of a 3x3 matrix
 */
double mat3_det(const double mat[S 3][3]);

INL void mat4_mul_vec4(const double mat[S 4][4], const double v[S 4],
                       double out[S 4]);
INL void mat4_mul_vec3(const double mat[S 4][4], const double v[S 3],
                       double out[S 3]);
INL void mat4_mul_dir3(const double mat[S 4][4], const double v[S 3],
                       double out[S 3]);
INL void mat4_copy(const double src[S 4][4], double out[S 4][4]);

void mat4_perspective(double mat[S 4][4], double fovy, double aspect,
                          double nearval, double farval);
/*
 * Perspective projection matrix that puts the far clip at infinity.
 * Idea from 'Projection Matrix Tricks', by Eric Lengyel.
 */
void mat4_inf_perspective(double mat[S 4][4], double fovy, double aspect,
                          double nearval);

void mat4_ortho(double mat[S 4][4], double left, double right,
                double bottom, double top, double nearval, double farval);

INL void mat4_to_float(const double mat[S 4][4], float out[S 16]);
INL void mat4_set_identity(double mat[S 4][4]);
INL bool mat4_is_identity(const double mat[S 4][4]);
INL void mat4_mul(const double a[S 4][4], const double b[S 4][4],
                  double out[S 4][4]);
INL void mat4_mul_mat3(const double a[S 4][4], const double b[S 3][3],
                       double out[S 4][4]);

INL void mat4_rx(double a, const double mat[S 4][4], double out[S 4][4]);
INL void mat4_ry(double a, const double mat[S 4][4], double out[S 4][4]);
INL void mat4_rz(double a, const double mat[S 4][4], double out[S 4][4]);
INL void mat4_itranslate(double m[S 4][4], double x, double y, double z);
INL void mat4_iscale(double m[S 4][4], double x, double y, double z);
bool mat4_invert(const double mat[S 4][4], double out[S 4][4]);
INL void mat4_transpose(const double mat[S 4][4], double out[S 4][4]);

INL void quat_set_identity(double q[S 4]);
INL void quat_ineg(double q[S 4]);
INL void quat_rx(double a, const double q[S 4], double out[S 4]);
INL void quat_ry(double a, const double q[S 4], double out[S 4]);
INL void quat_rz(double a, const double q[S 4], double out[S 4]);
INL void quat_mul(const double a[S 4], const double b[S 4], double out[S 4]);
INL void quat_mul_vec3(const double q[S 4], const double v[S 3],
                       double out[S 3]);
INL void quat_to_mat3(const double q[S 4], double out[S 3][3]);
void quat_slerp(const double a[S 4], const double b[S 4], double t,
                double out[S 4]);
// Separation angle between two quaternions.
double quat_sep(const double a[S 4], const double b[S 4]);
void quat_rotate_towards(const double a[S 4], const double b[S 4],
                         double max_angle, double out[S 4]);
INL void quat_normalize(const double q[S 4], double out[S 4]);

INL bool cap_contains_vec3(const double cap[S 4], const double v[S 3]);
INL bool cap_contains_cap(const double cap[S 4], const double c[S 4]);
INL bool cap_intersects_cap(const double cap[S 4], const double c[S 4]);
bool cap_intersects_segment(const double cap[S 4], const double p0[S 3],
                            const double p1[S 3]);
void cap_great_circle_closest_point(const double cap[S 4],
                                    const double u[S 3], double out[S 3]);

enum  {
    EULER_ORDER_DEFAULT = 0, // XYZ.
    EULER_ORDER_XYZ = 0,
    EULER_ORDER_XZY,
    EULER_ORDER_YXZ,
    EULER_ORDER_YZX,
    EULER_ORDER_ZXY,
    EULER_ORDER_ZYX
};

void eul_to_quat(const double e[S 3], int order, double out[S 4]);
void quat_to_eul(const double quat[S 4], int order, double e[S 3]);
void mat3_to_eul(const double m[S 3][3], int order, double e[S 3]);



// ********* Implementation of inline functions *************************//

INL void vec4_set(double v[S 4], double x, double y, double z, double w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

INL bool vec3_equal(const double a[S 3], const double b[S 3])
{
    return memcmp(a, b, 3 * sizeof(double)) == 0;
}

INL bool vec4_equal(const double a[S 4], const double b[S 4])
{
    return memcmp(a, b, 4 * sizeof(double)) == 0;
}

INL void vec2_to_float(const double a[S 2], float out[S 2])
{
    out[0] = a[0];
    out[1] = a[1];
}

INL void vec3_to_float(const double a[S 3], float out[S 3])
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
}

INL void vec4_to_float(const double a[S 4], float out[S 4])
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
    out[3] = a[3];
}

INL void vec2_addk(const double a[S 2], const double b[S 2], double k,
                   double c[S 2])
{
    c[0] = a[0] + k * b[0];
    c[1] = a[1] + k * b[1];
}

INL void vec3_addk(const double a[S 3], const double b[S 3], double k,
                   double c[S 3])
{
    c[0] = a[0] + k * b[0];
    c[1] = a[1] + k * b[1];
    c[2] = a[2] + k * b[2];
}

INL void vec2_add(const double a[S 2], const double b[S 2], double c[S 2])
{
    c[0] = a[0] + b[0];
    c[1] = a[1] + b[1];
}

INL void vec3_add(const double a[S 3], const double b[S 3], double c[S 3])
{
    c[0] = a[0] + b[0];
    c[1] = a[1] + b[1];
    c[2] = a[2] + b[2];
}

INL void vec2_sub(const double a[S 2], const double b[S 2], double c[S 2])
{
    c[0] = a[0] - b[0];
    c[1] = a[1] - b[1];
}

INL void vec3_sub(const double a[S 3], const double b[S 3], double c[S 3])
{
    c[0] = a[0] - b[0];
    c[1] = a[1] - b[1];
    c[2] = a[2] - b[2];
}

INL void vec2_mul(double k, const double v[S 2], double out[S 2])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
}

INL void vec3_mul(double k, const double v[S 3], double out[S 3])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
    out[2] = k * v[2];
}

INL void vec4_mul(double k, const double v[S 4], double out[S 4])
{
    out[0] = k * v[0];
    out[1] = k * v[1];
    out[2] = k * v[2];
    out[3] = k * v[3];
}

INL void vec4_emul(const double a[S 4], const double b[S 4], double out[S 4])
{
    out[0] = a[0] * b[0];
    out[1] = a[1] * b[1];
    out[2] = a[2] * b[2];
    out[3] = a[3] * b[3];
}

INL double vec2_norm2(const double v[S 2])
{
    return v[0] * v[0] + v[1] * v[1];
}

INL double vec3_norm2(const double v[S 3])
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

INL double vec2_norm(const double v[S 2])
{
    return sqrt(vec2_norm2(v));
}

INL double vec3_norm(const double v[S 3])
{
    return sqrt(vec3_norm2(v));
}

INL void vec2_normalize(const double v[S 2], double out[S 2])
{
    double n = vec2_norm(v);
    vec2_mul(1.0 / n, v, out);
}

INL void vec3_normalize(const double v[S 3], double out[S 3])
{
    double n = vec3_norm(v);
    vec3_mul(1.0 / n, v, out);
}

INL double vec2_dot(const double a[S 2], const double b[S 2])
{
    return a[0] * b[0] + a[1] * b[1];
}

INL double vec3_dot(const double a[S 3], const double b[S 3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

INL double vec4_dot(const double a[S 4], const double b[S 4])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

INL double vec2_dist2(const double a[S 2], const double b[S 2])
{
    double c[2];
    vec2_sub(a, b, c);
    return vec2_norm2(c);
}

INL double vec3_dist2(const double a[S 3], const double b[S 3])
{
    double c[3];
    vec3_sub(a, b, c);
    return vec3_norm2(c);
}

INL double vec2_dist(const double a[S 2], const double b[S 2])
{
    return sqrt(vec2_dist2(a, b));
}

INL double vec3_dist(const double a[S 3], const double b[S 3])
{
    return sqrt(vec3_dist2(a, b));
}

INL void vec2_mix(const double a[S 2], const double b[S 2], double k,
                  double out[S 2])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
}

INL void vec3_mix(const double a[S 3], const double b[S 3], double k,
                  double out[S 3])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
    out[2] = a[2] * (1.0 - k) + b[2] * k;
}

INL void vec4_mix(const double a[S 4], const double b[S 4], double k,
                  double out[S 4])
{
    out[0] = a[0] * (1.0 - k) + b[0] * k;
    out[1] = a[1] * (1.0 - k) + b[1] * k;
    out[2] = a[2] * (1.0 - k) + b[2] * k;
    out[3] = a[3] * (1.0 - k) + b[3] * k;
}

INL double vec2_cross(const double a[S 2], const double b[S 2])
{
    return a[0] * b[1] - a[1] * b[0];
}

INL void vec3_cross(const double a[S 3], const double b[S 3], double out[S 3])
{
    double tmp[3];
    tmp[0] = a[1] * b[2] - a[2] * b[1];
    tmp[1] = a[2] * b[0] - a[0] * b[2];
    tmp[2] = a[0] * b[1] - a[1] * b[0];
    vec3_copy(tmp, out);
}

INL void vec2_rotate(double angle, const double a[S 2], double out[S 2])
{
    double x, y;
    x = a[0] * cos(angle) - a[1] * sin(angle);
    y = a[0] * sin(angle) + a[1] * cos(angle);
    out[0] = x;
    out[1] = y;
}

INL bool vec3_is_normalized(const double v[S 3]) {
    return fabs(vec3_norm2(v) - 1.0) <= 0.0000000001;
}

INL void mat3_to_mat4(const double mat[S 3][3], double out[S 4][4])
{
    int i, j;
    memset(out, 0, 16 * sizeof(double));
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i][j] = mat[i][j];
    out[3][3] = 1.0;
}

INL void mat3_mul_vec3(const double mat[S 3][3], const double v[S 3],
                       double out[S 3])
{
    double x = v[0];
    double y = v[1];
    double z = v[2];
    out[0] = x * mat[0][0] + y * mat[1][0] + z * mat[2][0];
    out[1] = x * mat[0][1] + y * mat[1][1] + z * mat[2][1];
    out[2] = x * mat[0][2] + y * mat[1][2] + z * mat[2][2];
}

INL void mat3_mul_vec3_transposed(const double mat[S 3][3], const double v[S 3],
                                  double out[S 3])
{
    double x = v[0];
    double y = v[1];
    double z = v[2];
    out[0] = x * mat[0][0] + y * mat[0][1] + z * mat[0][2];
    out[1] = x * mat[1][0] + y * mat[1][1] + z * mat[1][2];
    out[2] = x * mat[2][0] + y * mat[2][1] + z * mat[2][2];
}

INL void mat3_mul_vec2(const double mat[S 3][3], const double v[S 2],
                       double out[S 2])
{
    double tmp[3] = {v[0], v[1], 1.0};
    double ret[3];
    mat3_mul_vec3(mat, tmp, ret);
    vec2_copy(ret, out);
}

INL void mat4_mul_vec4(const double mat[S 4][4], const double v[S 4],
                       double out[S 4])
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

INL void mat4_mul_vec3(const double mat[S 4][4], const double v[S 3],
                       double out[S 3])
{
    double tmp[4] = {v[0], v[1], v[2], 1.0};
    double ret[4];
    mat4_mul_vec4(mat, tmp, ret);
    vec3_copy(ret, out);
}

INL void mat4_mul_dir3(const double mat[S 4][4], const double v[S 3],
                       double out[S 3])
{
    double tmp[4] = {v[0], v[1], v[2], 0.0};
    double ret[4];
    mat4_mul_vec4(mat, tmp, ret);
    vec3_copy(ret, out);
}

INL bool mat2_invert(const double mat[S 2][2], double out[S 2][2])
{
    double det, inv[2][2];
    det = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
    if (det == 0)
        return false;
    inv[0][0] =  mat[1][1] / det;
    inv[0][1] = -mat[0][1] / det;
    inv[1][0] = -mat[1][0] / det;
    inv[1][1] =  mat[0][0] / det;
    memcpy(out, inv, sizeof(inv));
    return true;
}

INL void mat3_copy(const double src[S 3][3], double out[S 3][3])
{
    memcpy(out, src, 3 * 3 * sizeof(src[0][0]));
}

INL void mat4_copy(const double src[S 4][4], double out[S 4][4])
{
    memcpy(out, src, 4 * 4 * sizeof(src[0][0]));
}

INL void mat3_to_float(const double mat[S 3][3], float out[S 9])
{
    int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i * 3 + j] = mat[i][j];
}

INL void mat3_to_float4(const double mat[S 3][3], float out[S 16])
{
    int i, j;
    memset(out, 0, 16 * sizeof(float));
    out[15] = 1.0f;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
        out[i * 4 + j] = mat[i][j];
}

INL void mat4_to_float(const double mat[S 4][4], float out[S 16])
{
    int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++)
        out[i * 4 + j] = mat[i][j];
}

INL void mat3_set_identity(double mat[S 3][3])
{
    memset(mat, 0, 9 * sizeof(double));
    mat[0][0] = 1.0;
    mat[1][1] = 1.0;
    mat[2][2] = 1.0;
}

INL void mat4_set_identity(double mat[S 4][4])
{
    memcpy(mat, mat4_identity, sizeof(mat4_identity));
}

INL bool mat4_is_identity(const double mat[S 4][4])
{
    return memcmp(mat, mat4_identity, sizeof(mat4_identity)) == 0;
}

INL void mat3_mul(const double a[S 3][3], const double b[S 3][3],
                  double out[S 3][3])
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

INL void mat4_mul(const double a[S 4][4], const double b[S 4][4],
                  double out[S 4][4])
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

INL void mat4_mul_mat3(const double a[S 4][4], const double b[S 3][3],
                       double out[S 4][4])
{
    double b4[4][4];
    mat3_to_mat4(b, b4);
    mat4_mul(a, b4, out);
}

INL void mat3_rx(double a, const double mat[S 3][3], double out[S 3][3])
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

INL void mat3_ry(double a, const double mat[S 3][3], double out[S 3][3])
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

INL void mat3_rz(double a, const double mat[S 3][3], double out[S 3][3])
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

INL void mat4_rx(double a, const double mat[S 4][4], double out[S 4][4])
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

INL void mat4_ry(double a, const double mat[S 4][4], double out[S 4][4])
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

INL void mat4_rz(double a, const double mat[S 4][4], double out[S 4][4])
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

INL void mat3_itranslate(double m[S 3][3], double x, double y)
{
    int i;
    for (i = 0; i < 3; i++)
       m[2][i] += m[0][i] * x + m[1][i] * y;
}

INL void mat4_itranslate(double m[S 4][4], double x, double y, double z)
{
    int i;
    for (i = 0; i < 4; i++)
       m[3][i] += m[0][i] * x + m[1][i] * y + m[2][i] * z;
}

INL void mat3_iscale(double m[S 3][3], double x, double y, double z)
{
    int i;
    for (i = 0; i < 3; i++) {
        m[0][i] *= x;
        m[1][i] *= y;
        m[2][i] *= z;
    }
}

INL void mat4_iscale(double m[S 4][4], double x, double y, double z)
{
    int i;
    for (i = 0; i < 4; i++) {
        m[0][i] *= x;
        m[1][i] *= y;
        m[2][i] *= z;
    }
}

INL void mat3_transpose(const double mat[S 3][3], double out[S 3][3])
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

INL void mat4_transpose(const double mat[S 4][4], double out[S 4][4])
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

INL void quat_set_identity(double q[S 4])
{
    q[0] = 1.0;
    q[1] = 0.0;
    q[2] = 0.0;
    q[3] = 0.0;
}

INL void quat_from_axis(double q[S 4], double a, double x, double y, double z)
{
    double sin_angle;
    double vn[3] = {x, y, z};
    a *= 0.5;
    vec3_normalize(vn, vn);
    sin_angle = sin(a);
    q[0] = cos(a);
    q[1] = vn[0] * sin_angle;
    q[2] = vn[1] * sin_angle;
    q[3] = vn[2] * sin_angle;
}

INL void quat_mul(const double a[S 4], const double b[S 4], double out[S 4])
{
    double ax, ay, az, aw, bx, by, bz, bw;
    aw = a[0]; ax = a[1]; ay = a[2]; az = a[3];
    bw = b[0]; bx = b[1]; by = b[2]; bz = b[3];
    out[0] = aw * bw - ax * bx - ay * by - az * bz;
    out[1] = aw * bx + ax * bw + ay * bz - az * by;
    out[2] = aw * by + ay * bw + az * bx - ax * bz;
    out[3] = aw * bz + az * bw + ax * by - ay * bx;
}

INL void quat_mul_vec3(const double q[S 4], const double v[S 3],
                       double out[S 3])
{
    double m[3][3];
    quat_to_mat3(q, m);
    mat3_mul_vec3(m, v, out);
}

INL void quat_to_mat3(const double q[S 4], double out[S 3][3])
{
    double q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;
    const double SQRT2 = 1.41421356237309504880;

    q0 = SQRT2 * q[0];
    q1 = SQRT2 * q[1];
    q2 = SQRT2 * q[2];
    q3 = SQRT2 * q[3];

    qda = q0 * q1;
    qdb = q0 * q2;
    qdc = q0 * q3;
    qaa = q1 * q1;
    qab = q1 * q2;
    qac = q1 * q3;
    qbb = q2 * q2;
    qbc = q2 * q3;
    qcc = q3 * q3;

    out[0][0] = (1.0 - qbb - qcc);
    out[0][1] = (qdc + qab);
    out[0][2] = (-qdb + qac);

    out[1][0] = (-qdc + qab);
    out[1][1] = (1.0 - qaa - qcc);
    out[1][2] = (qda + qbc);

    out[2][0] = (qdb + qac);
    out[2][1] = (-qda + qbc);
    out[2][2] = (1.0 - qaa - qbb);
}

INL void quat_ineg(double q[S 4])
{
    q[0] = -q[0];
    q[1] = -q[1];
    q[2] = -q[2];
    q[3] = -q[3];
}

INL void quat_rx(double a, const double q[S 4], double out[S 4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 1.0, 0.0, 0.0);
    quat_mul(q, tmp, out);
}

INL void quat_ry(double a, const double q[S 4], double out[S 4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 0.0, 1.0, 0.0);
    quat_mul(q, tmp, out);
}

INL void quat_rz(double a, const double q[S 4], double out[S 4])
{
    double tmp[4];
    quat_from_axis(tmp, a, 0.0, 0.0, 1.0);
    quat_mul(q, tmp, out);
}

INL void quat_normalize(const double q[S 4], double out[S 4])
{
    double n = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    out[0] = q[0] / n;
    out[1] = q[1] / n;
    out[2] = q[2] / n;
    out[3] = q[3] / n;
}

INL bool cap_contains_vec3(const double cap[S 4], const double v[S 3])
{
  assert(vec3_is_normalized(cap));
  assert(vec3_is_normalized(v));
  return vec3_dot(cap, v) >= cap[3];
}

INL bool cap_contains_cap(const double cap[S 4], const double c[S 4])
{
  const double d1 = cap[3];
  const double d2 = c[3];
  const double a = vec3_dot(cap, c) - d1 * d2;
  return d1 <= d2 && (a >= 1.0 ||
                      (a >= 0.0 && a*a >= (1. - d1 * d1) * (1.0 - d2 * d2)));
}

// see http://f4bien.blogspot.com/2009/05/spherical-geometry-optimisations.html
// for detailed explanations.
INL bool cap_intersects_cap(const double cap[S 4], const double c[S 4])
{
  const double d1 = cap[3];
  const double d2 = c[3];
  const double a = d1 * d2 - vec3_dot(cap, c);
  return d1 + d2 <= 0.0 || a <= 0.0 ||
      (a <= 1.0 && a * a <= (1.0 - d1 * d1) * (1.0 - d2 * d2));
}


#undef INL
#undef S

#endif // VEC_H
