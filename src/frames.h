/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: frames.h
 * Referential conversion functions
 *
 */

typedef struct observer observer_t;

/* Enum: FRAME
 * Represent a frame of reference.
 *
 * FRAME_ICRS     - ICRS frame. Centered on Solar System Barycenter (SSB) with
 *                  axes (almost) aligned to equatorial J2000.0.
 *                  Use this frame for ephemerides of solar system objects and
 *                  astrometric reference data on galactic and extragalactic
 *                  objects, i.e., the data in astrometric star catalogs.
 * FRAME_GCRS     - GCRS frame. Like ICRS, but centered on earth geocenter.
 *                  Use this frame to describe the rotation of the Earth, the
 *                  orbits of Earth satellites, and geodetic quantities such as
 *                  instrument locations and baselines.
 * FRAME_JNOW     - Equatorial of date frame (=JNow or Geocentric Apparent).
 *                  Centered on earth geocenter. It is the true equator and
 *                  equinox of date equatorial frame.
 *                  Use this frame to describe apparent directions of
 *                  astronomical objects as seen from the Earth.
 * FRAME_CIRS     - CIRS frame. Like Equatorial of date but with the origin of
 *                  right ascension being the Celestial Intermediate Origin
 *                  (CIO) instead of the true equinox.
 *                  Also use this frame to describe apparent directions of
 *                  astronomical objects as seen from the Earth.
 * FRAME_OBSERVED - Observed frame (the frame of alt/az). Centered on observer.
 *                  Includes atmospheric refraction.
 * FRAME_VIEW     - Observed frame rotated in the observer view direction.
 * FRAME_NDC      - Normalized device coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 * FRAME_WINDOW   - Window coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 */
enum {
    FRAME_ICRS                = 0,
    FRAME_CIRS                = 1,
    FRAME_JNOW                = 2,
    FRAME_OBSERVED            = 3,
    FRAME_VIEW                = 4,
    FRAME_NDC                 = 5,
    FRAME_WINDOW              = 6,
};

/* Function: convert_direction
 * Rotate the passed 3D apparent direction vector from a Reference Frame to
 * another.
 *
 * The vector represents the apparent direction of the source as seen by the
 * observer in his reference system (usually GCRS for earth observation).
 * This means that effects such as space motion, light deflection or annual
 * aberration must already be taken into account before calling this function.
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Origin coordinates.
 *            One of the <FRAME> enum values.
 *  dest    - Destination coordinates.
 *            One of the <FRAME> enum values.
 *  flags   - Optional flags.  Not used yet.
 *  in      - The input coordinates (3d AU).
 *  out     - The output coordinates (3d AU).
 *
 * Return:
 *  0 for success.
 */
int convert_direction(const observer_t *obs,
                        int origin, int dest, int flags,
                        const double in[3], double out[3]);

/* Enum: ORIGIN
 * Represent a frame origin
 *
 */
enum {
    ORIGIN_BARYCENTRIC        = 0,  // Centered on Solar System Barycenter (SSB)
    ORIGIN_HELIOCENTRIC       = 1,  // Centered on the Sun
    ORIGIN_GEOCENTRIC         = 2,  // Centered on earth center
    ORIGIN_OBSERVERCENTRIC    = 3   // Centered on observer (=topocentric when
                                    // on earth)
};

/* Function: position_to_apparent
 * Convert 3D positions/velocity to apparent direction as seen from observer.
 *
 * This function performs basic 3D vectors addition/subtraction and change the
 * inertial frame to match the one of the observer. This convertion takes into
 * account the following effects:
 * - relative position of observer/object
 * - space motion of the observed object (compensate light time)
 * - annual abberation (space motion of the observer)
 * - diurnal abberation (and parallax)
 * - light deflection by the sun
 *
 * Input position/velocity and output direction are 3D vectors in the ICRF
 * reference frame.
 *
 * The output of this function must not be added/subtracted to other
 * positions/velocity from different intertial frame.
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Source origin.
 *            One of the <ORIGIN> enum values.
 *  at_inf  - true for fixed objects (far away from the solar system).
 *            For such objects, velocity is assumed to be 0 and the position
 *            is assumed to be normalized.
 *  in      - The input  ICRF position/velocity in AU and AU/day as seen from
 *            the given origin (in term of position and intertial frame).
 *  out     - The output ICRF position/velocity in AU and AU/day in the
 *            inertial frame of the observer.
 */
void position_to_apparent(const observer_t *obs, int origin, bool at_inf,
                          const double in[2][3], double out[2][3]);

/* Function: position_to_astrometric
 * Convert 3D positions/velocity to astrometric direction as seen from earth
 * center (GCRS).
 *
 * This function performs basic 3D vectors addition/subtraction and change the
 * inertial frame to match the one of the geocenter. This convertion takes into
 * account the following effects:
 * - relative position of earth/object
 * - space motion of the observed object (compensate light time)
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Source origin.
 *            One of the <ORIGIN> enum values.
 *  in      - The input  ICRF barycentric position/velocity in AU and AU/day as
 *            seen from the geocenter.
 *  out     - The output ICRF astrometric position/velocity in AU and AU/day as
 *            seen from the geocenter.
 */
void position_to_astrometric(const observer_t *obs, int origin,
                                const double in[2][3], double out[2][3]);

/* Function: astrometric_to_apparent
 * Convert astrometric direction to apparent direction. Input direction is
 * assumed to be seen from the earth center, while ouput direction is seen
 * from observer.
 *
 * This function change the inertial frame to match the one of the observer.
 * This convertion takes into account the following effects:
 * - position of observer on earth
 * - annual abberation (space motion of the observer)
 * - diurnal abberation (daily space motion of the observer)
 * - light deflection by the sun
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  in      - The input  ICRF barycentric position/velocity in AU and AU/day as
 *            seen from the observer.
 *  at_inf  - true for fixed objects (far away from the solar system)
 *  out     - The output ICRF astrometric position/velocity in AU and AU/day as
 *            seen from the observer.
 */
void astrometric_to_apparent(const observer_t *obs, const double in[3],
                             bool at_inf, double out[3]);
