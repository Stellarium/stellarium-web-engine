/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

EMSCRIPTEN_KEEPALIVE
int convert_coordinates(const observer_t *obs,
                        int origin, int dest, int flags,
                        const double v[4], double out[4])
{
    double p[4];
    double dist; // Distance in AU.
    double pvc[2][3], theta;
    const eraASTROM *astrom;

    obs = obs ?: (observer_t*)core->observer;
    astrom = &obs->astrom;
    assert(dest >= origin); // Only forward transformations.
    assert(v[3] == 0 || v[3] == 1);
    vec4_copy(v, p);

    dist = (p[3] == 0) ? INFINITY : vec3_norm(p);

    // ICRS to CIRS.
    if (origin <= FRAME_ICRS && dest > FRAME_ICRS) {
        vec3_normalize(p, p);
        // Aberration, giving GCRS proper direction.
        eraAb(p, astrom->v, astrom->em, astrom->bm1, p);
        // Bias-precession-nutation, giving CIRS proper direction.
        eraRxp(astrom->bpn, p, p);
        // Put back distance into the pos.
        if (p[3] == 1) eraSxp(dist, p, p);
        // Apply parallax due to observer location.
        // Note: this is not defined in SOFA lib, because it only applies
        // to objects that are very near the earth (mostly for the Moon).
        if (dist < 1.0) {
            eraSxp(DAU, p, p); // Set pos in m
            // XXX: this could be precomputed?
            theta = eraEra00(DJM0, obs->ut1);
            eraPvtob(obs->elong, obs->phi, 0, 0, 0, 0, theta, pvc);
            eraPmp(p, pvc[0], p);
            eraSxp(1. / DAU, p, p); // Set pos back in AU
        }
    }

    // CIRS to OBSERVED.
    if (origin <= FRAME_CIRS && dest > FRAME_CIRS) {
        // Precomputed earth rotation and polar motion.
        vec3_normalize(p, p);
        mat4_mul_vec3(obs->ri2h, p, p);
        refraction(p, astrom->refa, astrom->refb, p);
        vec3_normalize(p, p);
        // Put back distance into the pos.
        if (p[3] == 1) eraSxp(dist, p, p);
    }

    // OBSERVED to VIEW.
    if (origin <= FRAME_OBSERVED && dest > FRAME_OBSERVED)
        mat4_mul_vec4(obs->ro2v, p, p);

    vec4_copy(p, out);
    return 0;
}


// Given position and speed in Geocentric Equatorial J2000.0 (AU, AU/d),
// compute various coordinates.
int compute_coordinates(const observer_t *obs,
                        double pv[2][3],
                        double unit,
                        double *a_ra, double *a_dec,
                        double *az,   double *alt)
{
    double pos[3];
    double ri, di;
    double theta;
    double dist;
    double pvc[2][3];
    double aob, zob, hob, dob, rob;
    eraASTROM *astrom = (eraASTROM*)&obs->astrom;

    eraPn(pv[0], &dist, pos);
    eraC2s(pos, &ri, &di);
    if (a_ra) *a_ra = eraAnp(ri);
    if (a_dec) *a_dec = eraAnpm(di);

    // Light deflection by the Sun, giving BCRS natural direction.
    // XXX: disabled for the moment, need to do it only for objects that
    // are behind the sun.
    if (0)
        eraLdsun(pos, astrom->eh, astrom->em, pos);
    // Aberration, giving GCRS proper direction.
    eraAb(pos, astrom->v, astrom->em, astrom->bm1, pos);
    // Bias-precession-nutation, giving CIRS proper direction.
    eraRxp(astrom->bpn, pos, pos);

    eraC2s(pos, &ri, &di);

    // Apply parallax if needed.
    if (unit < INFINITY) {
        eraSxp(DAU * dist, pos, pos); // Set pos in m
        // XXX: this could be precomputed?
        theta = eraEra00(DJM0, obs->ut1);
        eraPvtob(obs->elong, obs->phi, 0, 0, 0, 0, theta, pvc);
        eraPmp(pos, pvc[0], pos);
        eraC2s(pos, &ri, &di);
    }

    // Observed coordinates (az, alt).
    eraAtioq(ri, di, astrom, &aob, &zob, &hob, &dob, &rob);
    if (az) *az = aob;
    if (alt) *alt = 90 * DD2R - zob;

    return 0;
}

