/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "observer.h"

#include "algos/algos.h"
#include "constants.h"
#include "utils/vec.h"

static void update_matrices(observer_t *obs)
{
    eraASTROM *astrom = &obs->astrom;
    // We work with 3x3 matrices, so that we can use the erfa functions.
    double ro2v[3][3];  // Rotate from observed to view.
    double ri2h[3][3];  // Equatorial J2000 (ICRS) to horizontal.
    double rh2i[3][3];  // Horizontal to Equatorial J2000 (ICRS).
    double ri2v[3][3];  // Equatorial J2000 (ICRS) to view.
    double ri2e[3][3];  // Equatorial J2000 (ICRS) to ecliptic.
    double re2i[3][3];  // Eclipic to Equatorial J2000 (ICRS).
    double re2h[3][3];  // Ecliptic to horizontal.
    double re2v[3][3];  // Ecliptic to view.

    mat3_set_identity(ro2v);
    // r2gl changes the coordinate from z up to y up orthonomal.
    double r2gl[3][3] = {{0, 0,-1},
                         {1, 0, 0},
                         {0, 1, 0}};
    mat3_rx(-obs->altitude, ro2v, ro2v);
    mat3_ry(obs->azimuth, ro2v, ro2v);
    mat3_mul(ro2v, r2gl, ro2v);

    // Compute rotation matrix from CIRS to horizontal.
    mat3_set_identity(ri2h);
    // Earth rotation.
    mat3_rz(astrom->eral, ri2h, ri2h);
    // Polar motion.
    double rpl[3][3] = {{1, 0, astrom->xpl},
                        {0, 1, astrom->ypl},
                        {astrom->xpl, astrom->ypl, 1}};
    mat3_mul(ri2h, rpl, ri2h);
    // Cartesian -HA,Dec to Cartesian Az,El (S=0,E=90).
    mat3_ry(-obs->phi + M_PI / 2, ri2h, ri2h);
    double rsx[3][3] = {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    mat3_mul(ri2h, rsx, ri2h);
    mat3_transpose(ri2h, ri2h);

    // Also store its inverse.
    mat3_invert(ri2h, rh2i);
    mat3_mul(ro2v, ri2h, ri2v);

    // Equatorial to ecliptic
    mat3_set_identity(re2i);
    mat3_rx(eraObl80(DJM0, obs->ut1), re2i, re2i);
    mat3_invert(re2i, ri2e);

    // Ecliptic to horizontal.
    mat3_copy(ri2h, re2h);
    mat3_rx(eraObl80(DJM0, obs->ut1), re2h, re2h);
    mat3_mul(ro2v, re2h, re2v);

    // Convert all to 4x4 matrices.
    mat3_to_mat4(ro2v, obs->ro2v);
    mat3_to_mat4(ri2h, obs->ri2h);
    mat3_to_mat4(rh2i, obs->rh2i);
    mat3_to_mat4(ri2v, obs->ri2v);
    mat3_to_mat4(ri2e, obs->ri2e);
    mat3_to_mat4(re2i, obs->re2i);
    mat3_to_mat4(re2h, obs->re2h);
    mat3_to_mat4(re2v, obs->re2v);
}

void observer_recompute_hash(observer_t *obs)
{
    // XXX: do it properly!
    obs->hash++;
}

void observer_update(observer_t *obs, bool fast)
{
    double utc1, utc2, ut11, ut12;
    double dt, dut1 = 0;
    double pvb[2][3];

    if (!fast || obs->force_full_update) obs->dirty = true;
    if (obs->last_update != obs->tt) obs->dirty = true;
    if (!obs->dirty) return;

    // Compute UT1 and UTC time.
    dt = deltat(obs->tt);
    dut1 = 0;
    eraTtut1(DJM0, obs->tt, dt, &ut11, &ut12);
    eraUt1utc(ut11, ut12, dut1, &utc1, &utc2);
    obs->ut1 = ut11 - DJM0 + ut12;
    obs->utc = utc1 - DJM0 + utc2;

    if (obs->force_full_update || fabs(obs->last_full_update - obs->tt) > 1)
        fast = false;

    if (fast) {
        eraAper13(DJM0, obs->ut1, &obs->astrom);
        eraPvu(obs->tt - obs->last_update, obs->earth_pvh, obs->earth_pvh);
    } else {
        eraApco13(DJM0, obs->utc, dut1,
                obs->elong, obs->phi,
                obs->hm,
                0, 0,
                obs->refraction ? obs->pressure : 0,
                15,       // Temperature (dec C)
                0.5,      // Relative humidity (0-1)
                0.55,     // Effective color (micron),
                &obs->astrom,
                &obs->eo);
        // Update earth position.
        eraEpv00(DJM0, obs->tt, obs->earth_pvh, pvb);
        obs->last_full_update = obs->tt;
    }
    obs->last_update = obs->tt;
    update_matrices(obs);
    obs->dirty = false;
    obs->force_full_update = false;
    observer_recompute_hash(obs);
}

