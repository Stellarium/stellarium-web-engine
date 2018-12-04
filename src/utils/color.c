/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "color.h"

#include <math.h>

static void mat3_mul_vec3(double mat[3][3], const double v[3], double out[3])
{
    double x = v[0];
    double y = v[1];
    double z = v[2];
    out[0] = x * mat[0][0] + y * mat[1][0] + z * mat[2][0];
    out[1] = x * mat[0][1] + y * mat[1][1] + z * mat[2][1];
    out[2] = x * mat[0][2] + y * mat[1][2] + z * mat[2][2];
}

void xyY_to_XYZ(const double xyY[3], double XYZ[3])
{
    double X = xyY[0] * xyY[2] / xyY[1];
    double Y = xyY[2];
    double Z = (1.0 - xyY[0] - xyY[1]) * xyY[2] / xyY[1];
    XYZ[0] = X;
    XYZ[1] = Y;
    XYZ[2] = Z;
}

static void rgb_to_srgb(const double rgb[3], double out[3])
{
    int i;
    // Value taken from https://en.wikipedia.org/wiki/SRGB
    for (i = 0; i < 3; i++) {
        out[i] = (rgb[i] <= 0.0031308) ? 12.92 * rgb[i] :
                  1.055 * powf(rgb[i], 1 / 2.4) - 0.055;
    }
}

void XYZ_to_rgb(const double XYZ[3], double rgb[3])
{
    // Value taken from https://en.wikipedia.org/wiki/SRGB
    const double xyz_to_rgb_mat[3][3] = {
        {3.2406, -0.9689, 0.0557},
        {-1.5372, 1.8758, -0.2040},
        {-0.4986, 0.0415, 1.0570},
    };
    mat3_mul_vec3((void*)xyz_to_rgb_mat, XYZ, rgb);
}

void XYZ_to_srgb(const double XYZ[3], double srgb[3])
{
    double rgb[3];
    XYZ_to_rgb(XYZ, rgb);
    rgb_to_srgb(rgb, srgb);
}

void xyY_to_srgb(const double xyY[3], double srgb[3])
{
    double XYZ[3];
    xyY_to_XYZ(xyY, XYZ);
    XYZ_to_srgb(XYZ, srgb);
}

void xyY_to_rgb(const double xyY[3], double rgb[3])
{
    double XYZ[3];
    xyY_to_XYZ(xyY, XYZ);
    XYZ_to_rgb(XYZ, rgb);
}

void hex_to_rgba(uint32_t v, double rgba[4])
{
    rgba[0] = ((v >> 24) & 0xff) / 255.0f,
    rgba[1] = ((v >> 16) & 0xff) / 255.0f,
    rgba[2] = ((v >>  8) & 0xff) / 255.0f,
    rgba[3] = ((v >>  0) & 0xff) / 255.0f;
}
