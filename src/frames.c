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
                        const double icrs[4],
                        double *ra, double *dec,
                        double *az,   double *alt)
{
    double pos[4];
    double ri, di;
    vec4_copy(icrs, pos);
    convert_coordinates(obs, FRAME_ICRS, FRAME_CIRS, 0, pos, pos);
    if (ra || dec) {
        eraC2s(pos, &ri, &di);
        if (ra) *ra = eraAnp(ri);
        if (dec) *dec = eraAnpm(di);
    }
    convert_coordinates(obs, FRAME_CIRS, FRAME_OBSERVED, 0, pos, pos);
    if (az || alt) {
        eraC2s(pos, &ri, &di);
        if (az) *az = eraAnp(ri);
        if (alt) *alt = eraAnpm(di);
    }
    return 0;
}

