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

/*
 * Type: observer_t
 * Store informations about the observer current position.
 */
struct observer
{
    obj_t  obj;
    double elong;
    double phi;
    double hm;          // height above ellipsoid (m)
    double horizon;     // altitude of horizon (used for rising/setting).
    double pressure;    // Set to NAN to compute it from the altitude.
    bool   refraction;  // Whether we use refraction or not.

    double altitude;
    double azimuth;
    double roll;
    obj_t  *city;

    bool dirty;                 // Require an update.
    bool force_full_update;     // Set to true to force a full update.
    double last_update;
    double last_full_update;

    // Hash value that represents a given observer state.  Used to prevent
    // updating object data several times with the same observer.
    uint64_t hash;

    // Different times, all in MJD.
    double tt;
    double ut1;
    double utc;

    double eo;  // Equation of origin.
    eraASTROM astrom;
    // Position and speed of the earth. equ, J2000.0, AU heliocentric.
    double earth_pvh[2][3];

    // The pointed position and constellation.
    struct {
        double icrs[4];
        char cst[4];
    } pointer;

    // Transformation matrices.
    // h: Horizontal (RA/DE, left handed, X->N, Y->E, Z->up).
    // o: Observed: horizontal with refraction (RA/DE, left handed).
    // i: ICRS (right handed).
    // e: Ecliptic (right handed).
    // v: View (observed with view direction).
    double ro2v[4][4];  // Rotate from observed to view.
    double ri2h[4][4];  // Equatorial J2000 (ICRS) to horizontal.
    double rh2i[4][4];  // Horizontal to Equatorial J2000 (ICRS).
    double ri2v[4][4];  // Equatorial J2000 (ICRS) to view.
    double ri2e[4][4];  // Equatorial J2000 (ICRS) to ecliptic.
    double re2i[4][4];  // Eclipic to Equatorial J2000 (ICRS).
    double re2h[4][4];  // Ecliptic to horizontal.
    double re2v[4][4];  // Ecliptic to view.
};

void observer_recompute_hash(observer_t *obs);
void observer_update(observer_t *obs, bool fast);

#endif // OBSERVER_H
