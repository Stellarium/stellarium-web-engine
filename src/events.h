/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// XXX: we could probably merge this with calendar, since this is related.

typedef struct observer observer_t;
typedef struct obj obj_t;

// Events types
enum {
    EVENT_RISE      = 1 << 0,
    EVENT_SET       = 1 << 1,
};

// Compute time for a given event.
//  start_time: time in MJD (UT).
//  end_time  : time in MJD (UT).
//  precision : precision of the result (JD).
//  Return the time in MJD (UT)
//  XXX: probably need to redo this part.
double compute_event(observer_t *obs, obj_t *obj, int event,
                     double start_time, double end_time,
                     double precision);

