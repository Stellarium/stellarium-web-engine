/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

typedef struct observer observer_t;

/*
 * File: calendar.h
 * Utilities to compute astronomy events calendars.
 */

/*
 * Enum: CALENDAR flags
 * Modify the behavior of the <calendar_get> function.
 */
enum {
    CALENDAR_HIDDEN = 1 << 0,
};

/* Function: calendar_get
 * Compute calendar events.
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
                        const char *desc, int flags, void *user));

// Mostly for testing.
void calendar_print(void);

