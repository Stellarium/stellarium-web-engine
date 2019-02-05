/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "projection.h"

#include <assert.h>

#include "algos/algos.h"
#include "tests.h"
#include "utils/vec.h"
#include "utils/utils.h"

/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)
/* Radians to degrees */
#define DR2D (57.29577951308232087679815)

static void proj_healpix_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double p[3] = {v[0], v[1], 1};
    assert(flags & PROJ_BACKWARD);
    mat3_mul_vec3(proj->mat3, p, p);
    healpix_xy2vec(p, out);
    out[3] = proj->at_infinity ? 0.0 : 1.0;
}

void projection_init_healpix(projection_t *proj, int nside, int pix,
                             bool swap, bool at_infinity)
{
    double tmp[3];
    memset(proj, 0, sizeof(*proj));
    proj->type = PROJ_HEALPIX;
    proj->nside = nside;
    proj->pix = pix;
    proj->max_fov = 360 * DD2R;
    healpix_get_mat3(nside, pix, proj->mat3);
    if (swap) {
        proj->swapped = true;
        vec3_copy(proj->mat3[0], tmp);
        vec3_copy(proj->mat3[1], proj->mat3[0]);
        vec3_copy(tmp, proj->mat3[1]);
    }
    proj->at_infinity = at_infinity;
    proj->backward = proj_healpix_project;
}


/******* TESTS **********************************************************/
#ifdef COMPILE_TESTS

static void test_healpix(void)
{
    // nside, pix, theta, phi, x, y, z
    // Computed using the official healpix lib.
    const double VALS[][7] = {
{1, 0, 48.189685, 45.000000, 0.527046, 0.527046, 0.666667},
{1, 1, 48.189685, 135.000000, -0.527046, 0.527046, 0.666667},
{1, 2, 48.189685, 225.000000, -0.527046, -0.527046, 0.666667},
{1, 3, 48.189685, 315.000000, 0.527046, -0.527046, 0.666667},
{1, 4, 90.000000, 0.000000, 1.000000, 0.000000, 0.000000},
{1, 5, 90.000000, 90.000000, 0.000000, 1.000000, 0.000000},
{1, 6, 90.000000, 180.000000, -1.000000, 0.000000, 0.000000},
{1, 7, 90.000000, 270.000000, -0.000000, -1.000000, 0.000000},
{1, 8, 131.810315, 45.000000, 0.527046, 0.527046, -0.666667},
{1, 9, 131.810315, 135.000000, -0.527046, 0.527046, -0.666667},
{1, 10, 131.810315, 225.000000, -0.527046, -0.527046, -0.666667},
{1, 11, 131.810315, 315.000000, 0.527046, -0.527046, -0.666667},
{2, 0, 70.528779, 45.000000, 0.666667, 0.666667, 0.333333},
{2, 1, 48.189685, 67.500000, 0.285235, 0.688619, 0.666667},
{2, 2, 48.189685, 22.500000, 0.688619, 0.285235, 0.666667},
{2, 3, 23.556464, 45.000000, 0.282597, 0.282597, 0.916667},
{1024, 7399878, 114.460292, 266.528320, -0.055120, -0.908578, -0.414062},
{256, 70836, 72.573677, 150.820312, -0.833022, 0.465173, 0.299479},
{4, 82, 109.471221, 78.750000, 0.183933, 0.924693, -0.333333},
{16, 32, 77.975301, 33.750000, 0.813225, 0.543380, 0.208333},
{64, 11166, 34.126650, 188.804348, -0.554413, -0.085871, 0.827799},
{4096, 54937164, 60.718881, 341.982422, 0.829458, -0.269789, 0.489095},
{64, 9130, 60.000000, 214.453125, -0.714115, -0.489938, 0.500000},
{256, 490296, 78.737020, 286.523438, 0.278930, -0.940240, 0.195312},
{1024, 11358326, 111.221793, 245.083008, -0.392735, -0.845417, -0.361979},
{256, 594660, 161.091949, 155.097087, -0.293921, 0.136452, -0.946040},
{4, 2, 70.528779, 33.750000, 0.783917, 0.523797, 0.333333},
{256, 162128, 36.235439, 253.615385, -0.166741, -0.567100, 0.806595},
    };

    const double e = 0.0001;
    double v[3], theta, phi;
    int i;
    for (i = 0; i < ARRAY_SIZE(VALS); i++) {
        healpix_pix2vec(VALS[i][0], VALS[i][1], v);
        healpix_pix2ang(VALS[i][0], VALS[i][1], &theta, &phi);
        test_float(theta * DR2D, VALS[i][2], e);
        test_float(phi * DR2D, VALS[i][3], e);
        test_float(v[0], VALS[i][4], e);
        test_float(v[1], VALS[i][5], e);
        test_float(v[2], VALS[i][6], e);
    }
}

TEST_REGISTER(NULL, test_healpix, TEST_AUTO);

#endif
