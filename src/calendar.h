/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: calendar.h
 * Utilities to compute astronomy events calendars.
 */

#ifndef CALENDAR_H
#define CALENDAR_H

typedef struct observer observer_t;
typedef struct calendar calendar_t;
typedef struct obj obj_t;

/*
 * Enum: CALENDAR_FLAG
 * Modify the behavior of the <calendar_get> function.
 */
enum {
    CALENDAR_HIDDEN = 1 << 0,
};

// Mostly for testing.
void calendar_print(void);

/*
 * Function: calendar_create
 * Create a new calendar object.
 *
 * This return a new calendar, wich has not been computed yet.
 *
 * Parameters:
 *   obs    - an observer.
 *   start  - start time for the calendar computation (UT1, MJD)
 *   end    - end time for the calendar computation (UT1, MJD)
 *   flags  - <CALENDAR_FLAG> union.
 */
calendar_t *calendar_create(const observer_t *obs,
                            double start, double end, int flags);

/*
 * Function: calendar_compute
 * Compute the calendar events.
 *
 * To compute all the events we have to call this function several times
 * until it returns 0.  This allow to use it in a loop without blocking the
 * thread.
 *
 * Return:
 *   0 if the computation has finished, 1 otherwise.
 */
int calendar_compute(calendar_t *cal);

/*
 * Function: calendar_get_results
 * Get all the events from a calendar.
 *
 * This should be called only after calendar_compute has finished.
 */
int calendar_get_results(calendar_t *cal, void *user,
                         int (*callback)(double ut1, const char *type,
                         const char *desc, int flags,
                         obj_t *o1, obj_t *o2,
                         void *user));

/*
 * Function: calendar_delete
 * Delete a calendar.
 */
void calendar_delete(calendar_t *cal);


/* Function: calendar_get
 * Compute calendar events.
 *
 * This is just a conveniance function to create a calendar, compute it,
 * and get the events in a single call.
 *
 * Properties:
 *  obs         - An observer instance.
 *  start_ut1   - Start time (MJD, UT1).
 *  end_ut1     - End time (MJD, UT1).
 *  flags       - Union of <CALENDAR flags> values.
 *  user        - User data passed to the callback function.
 *  callback    - Function called once per event in the given time range
 *                for the given observer.
 */
int calendar_get(
        const observer_t *obs,
        double start_ut1, double end_ut1, int flags, void *user,
        int (*callback)(double ut1, const char *type,
                        const char *desc, int flags,
                        obj_t *o1, obj_t *o2,
                        void *user));

#endif // CALENDAR_H
