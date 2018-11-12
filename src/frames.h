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
 * FRAME_ICRS     - ICRS frame.
 * FRAME_CIRS     - CIRS frame.
 * FRAME_OBSERVED - Observed frame (the frame of alt/az).
 * FRAME_VIEW     - Observed frame rotated in the observer view direction.
 * FRAME_NDC      - Normalized device coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 * FRAME_WINDOW   - Window coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 */
enum {
    FRAME_ICRS                = 1,
    FRAME_CIRS                = 2,
    FRAME_OBSERVED            = 3,
    FRAME_VIEW                = 4,
    FRAME_NDC                 = 5,
    FRAME_WINDOW              = 6,
};

/* Function: convert_coordinates
 * Convert Cartesian coordinates from a referential to an other.
 *
 * The coordinates are passed in homogeneous coordinates (xyzw) where
 * w can be either 1 or 0.  If w is 1, then the xyz vector represents the
 * position with AU unit.  If w is 0, then the xyz vector represents a point
 * at infinity.
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Origin coordinates.
 *            One of the <FRAME> enum values.
 *  dest    - Destination coordinates.
 *            One of the <FRAME> enum values.
 *  flags   - Optional flags.  Not used yet.
 *  v       - The input coordinates (4d AU).
 *  out     - The output coordinates (4d AU).
 *
 * Return:
 *  0 for success.
 */
int convert_coordinates(const observer_t *obs,
                        int origin, int dest, int flags,
                        const double v[4], double out[4]);
