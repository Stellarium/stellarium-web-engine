/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>

// Some of the code comes from the official healpix C implementation.

/*
   utab[m] = (short)(
      (m&0x1 )       | ((m&0x2 ) << 1) | ((m&0x4 ) << 2) | ((m&0x8 ) << 3)
    | ((m&0x10) << 4) | ((m&0x20) << 5) | ((m&0x40) << 6) | ((m&0x80) << 7));
*/
static const short utab[]={
#define Z(a) 0x##a##0, 0x##a##1, 0x##a##4, 0x##a##5
#define Y(a) Z(a##0), Z(a##1), Z(a##4), Z(a##5)
#define X(a) Y(a##0), Y(a##1), Y(a##4), Y(a##5)
X(0),X(1),X(4),X(5)
#undef X
#undef Y
#undef Z
};

/*
   ctab[m] = (short)(
       (m&0x1 )       | ((m&0x2 ) << 7) | ((m&0x4 ) >> 1) | ((m&0x8 ) << 6)
    | ((m&0x10) >> 2) | ((m&0x20) << 5) | ((m&0x40) >> 3) | ((m&0x80) << 4));
*/
static const short ctab[]={
#define Z(a) a,a+1,a+256,a+257
#define Y(a) Z(a),Z(a+2),Z(a+512),Z(a+514)
#define X(a) Y(a),Y(a+4),Y(a+1024),Y(a+1028)
X(0),X(8),X(2048),X(2056)
#undef X
#undef Y
#undef Z
};

// Position of the healpix faces.
static const int FACES[12][2] = {{1,  0}, {3,  0}, {5,  0}, {7,  0},
                                 {0, -1}, {2, -1}, {4, -1}, {6, -1},
                                 {1, -2}, {3, -2}, {5, -2}, {7, -2}};

/*! Returns the remainder of the division \a v1/v2.
    The result is non-negative.
    \a v1 can be positive or negative; \a v2 must be positive. */
static double fmodulo (double v1, double v2)
{
    if (v1 >= 0)
        return (v1 < v2) ? v1 : fmod(v1, v2);
    double tmp = fmod(v1, v2) + v2;
    return (tmp == v2) ? 0. : tmp;
}

int healpix_xyf2nest(int nside, int ix, int iy, int face_num)
{
  return (face_num*nside*nside) +
      (utab[ix&0xff] | (utab[ix>>8]<<16)
    | (utab[iy&0xff]<<1) | (utab[iy>>8]<<17));
}

static void nest2xyf(int nside, int pix, int *ix, int *iy, int *face_num)
{
    int npface_ = nside * nside, raw;
    *face_num = pix / npface_;
    pix &= (npface_ - 1);
    raw = (pix & 0x5555) | ((pix & 0x55550000) >> 15);
    *ix = ctab[raw & 0xff] | (ctab[raw >> 8] << 4);
    pix >>= 1;
    raw = (pix & 0x5555) | ((pix & 0x55550000) >> 15);
    *iy = ctab[raw & 0xff] | (ctab[raw >> 8] << 4);
}

void healpix_get_mat3(int nside, int pix, double out[3][3])
{
    int ix, iy, face;
    nest2xyf(nside, pix, &ix, &iy, &face);
    out[0][0] = +M_PI / 4 / nside;
    out[0][1] = +M_PI / 4 / nside;
    out[0][2] = 0;
    out[1][0] = -M_PI / 4 / nside;
    out[1][1] = +M_PI / 4 / nside;
    out[1][2] = 0;
    out[2][0] = (FACES[face][0] + (ix - iy + 0.0) / nside) * M_PI / 4;
    out[2][1] = (FACES[face][1] + (ix + iy + 0.0) / nside) * M_PI / 4;
    out[2][2] = 1;
}

static void healpix_xy2_z_phi(const double xy[2], double *z, double *phi)
{
    double x = xy[0], y = xy[1];
    double sigma, xc;

    if (fabs(y) > M_PI / 4) {
        // Polar
        sigma = 2 - fabs(y * 4) / M_PI;
        *z = (y > 0 ? 1 : -1) * (1 - sigma * sigma / 3);
        xc = -M_PI + (2 * floor((x + M_PI) * 4 / (2 * M_PI)) + 1) * M_PI / 4;
        *phi = sigma ? (xc + (x - xc) / sigma) : x;
    } else {
        // Equatorial
        *phi = x;
        *z = y * 8 / (M_PI * 3);
    }
}

void healpix_xy2ang(const double xy[2], double *theta, double *phi)
{
    double z;
    healpix_xy2_z_phi(xy, &z, phi);
    *theta = acos(z);
}

void healpix_xy2vec(const double xy[2], double out[3])
{
    double z, phi, stheta;
    healpix_xy2_z_phi(xy, &z, &phi);
    stheta = sqrt((1 - z) * (1 + z));
    out[0] = stheta * cos(phi);
    out[1] = stheta * sin(phi);
    out[2] = z;
}

void healpix_pix2vec(int nside, int pix, double out[3])
{
    int ix, iy, face;
    double xy[2];
    nest2xyf(nside, pix, &ix, &iy, &face);
    xy[0] = (FACES[face][0] + (ix - iy + 0.0) / nside) * M_PI / 4;
    xy[1] = (FACES[face][1] + (ix + iy + 1.0) / nside) * M_PI / 4;
    healpix_xy2vec(xy, out);
}

void healpix_pix2ang(int nside, int pix, double *theta, double *phi)
{
    int ix, iy, face;
    double xy[2];
    nest2xyf(nside, pix, &ix, &iy, &face);
    xy[0] = (FACES[face][0] + (ix - iy + 0.0) / nside) * M_PI / 4;
    xy[1] = (FACES[face][1] + (ix + iy + 1.0) / nside) * M_PI / 4;
    healpix_xy2ang(xy, theta, phi);
}

static int ang2pix_nest_z_phi (long nside_, double z, double phi)
{
    double za = fabs(z);
    double tt = fmodulo(phi, 2 * M_PI) * (2 / M_PI); /* in [0,4) */
    int face_num, ix, iy;

    if (za <= 2. / 3.) /* Equatorial region */
    {
        double temp1 = nside_*(0.5+tt);
        double temp2 = nside_*(z*0.75);
        int jp = (int)(temp1-temp2); /* index of  ascending edge line */
        int jm = (int)(temp1+temp2); /* index of descending edge line */
        int ifp = jp/nside_;  /* in {0,4} */
        int ifm = jm/nside_;
        face_num = (ifp==ifm) ? (ifp|4) : ((ifp<ifm) ? ifp : (ifm+8));
        ix = jm & (nside_-1);
        iy = nside_ - (jp & (nside_-1)) - 1;
    }
    else /* polar region, za > 2/3 */
    {
        int ntt = (int)tt, jp, jm;
        double tp, tmp;
        if (ntt>=4) ntt=3;
        tp = tt-ntt;
        tmp = nside_*sqrt(3*(1-za));

        jp = (int)(tp*tmp); /* increasing edge line index */
        jm = (int)((1.0-tp)*tmp); /* decreasing edge line index */
        if (jp>=nside_) jp = nside_-1; /* too close to the boundary */
        if (jm>=nside_) jm = nside_-1;
        if (z >= 0)
        {
            face_num = ntt;  /* in {0,3} */
            ix = nside_ - jm - 1;
            iy = nside_ - jp - 1;
        }
        else
        {
            face_num = ntt + 8; /* in {8,11} */
            ix =  jp;
            iy =  jm;
        }
    }

    return healpix_xyf2nest(nside_,ix,iy,face_num);
}

void healpix_ang2pix(int nside, double theta, double phi, int *pix)
{
    *pix = ang2pix_nest_z_phi(nside, cos(theta), phi);
}
