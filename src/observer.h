/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef OBSERVER_H
#define OBSERVER_H

#include "obj.h"
#include "erfa.h"

typedef struct {
    double d[5];
    bool   b;
} _h1_t;

typedef struct {
    double d[5];
} _h2_t;

/*
 * Type: observer_t
 * Store informations about the observer current position.
 */
struct observer
{
    obj_t  obj;

    union {
        // Put in this struct all elements contributing to the hash_partial
        struct {
            double elong;       // Observer longitude
            double phi;         // Observer latitude
            double hm;          // height above ellipsoid (m)
            double horizon;     // altitude of horizon (used for rise/set).
            double pressure;    // Set to NAN to compute it from the altitude.
            bool   refraction;  // Whether we use refraction or not.
        };
        _h1_t h1;               // Used for fast hash computation
    };

    union {
        // Put in this struct all elements contributing to the hash (full)
        struct {
            double altitude;
            double azimuth;
            double roll;
            // Extra rotations applied to the view matrix in altitude.
            // Set this to have the centered objet not located at screen center
            // but somewhere else.
            double view_offset_alt;
            double tt;          // TT time in MJD
        };
        _h2_t h2;               // Used for fast hash computation
    };

    obj_t  *city;

    double last_update;
    double last_accurate_update;

    // Hash value that represents a given observer state for which the accurate
    // values have been computed. Used to prevent updating object data several
    // times with the same observer.
    uint64_t hash_accurate;

    // Hash value that represents the last observer state for which the
    // values have been computed. Used to prevent updating object data several
    // times with the same observer.
    uint64_t hash;

    // Hash of a partial state of the observer. If it is unchanged, it is
    // safe to use make fast update.
    uint64_t hash_partial;

    // Different times, all in MJD.
    double ut1;
    double utc;

    double eo;  // Equation of origin.
    eraASTROM astrom;
    // Heliocentric position/speed of the earth in ICRF reference frame and in
    // BCRS reference system. AU, AU/day.
    double earth_pvh[2][3];
    // Barycentric position/speed of the earth in ICRS, i.e. as seen from the
    // SSB in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double earth_pvb[2][3];
    // Barycentric position/speed of the sun in ICRS, i.e. as seen from the SSB
    // in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double sun_pvb[2][3];
    // Apparent position/speed of the sun (as seen from the observer) in ICRF
    // reference frame, in local reference system. AU, AU/day.
    double sun_pvo[2][3];
    // Barycentric position/speed of the observer in ICRS, i.e. as seen from the
    // SSB in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double obs_pvb[2][3];
    double obs_pvg[2][3];

    // The pointed position and constellation.
    struct {
        double icrs[3];
        char cst[4];
    } pointer;

    // Transformation matrices.
    // h: Horizontal (RA/DE, left handed, X->N, Y->E, Z->up).
    // o: Observed: horizontal with refraction (RA/DE, left handed).
    // i: ICRS (right handed).
    // e: Ecliptic (right handed).
    // v: View (observed with view direction).
    double ro2v[3][3];  // Rotate from observed to view.
    double rv2o[3][3];  // Rotate from view to observed.
    double ri2h[3][3];  // Equatorial J2000 (ICRS) to horizontal.
    double rh2i[3][3];  // Horizontal to Equatorial J2000 (ICRS).
    double ri2v[3][3];  // Equatorial J2000 (ICRS) to view.
    double ri2e[3][3];  // Equatorial J2000 (ICRS) to ecliptic.
    double re2i[3][3];  // Eclipic to Equatorial J2000 (ICRS).
    double re2h[3][3];  // Ecliptic to horizontal.
    double re2v[3][3];  // Ecliptic to view.
};

void observer_update(observer_t *obs, bool fast);

#endif // OBSERVER_H
