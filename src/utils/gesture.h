/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: gesture.h
 *
 * Mouse/touch gestures manager.
 */

/*
 * Enum: GESTURE_TYPE
 * The different type of gesture we can create.
 */
enum {
    GESTURE_CLICK,
    GESTURE_PAN,
    GESTURE_HOVER,
    GESTURE_PINCH,
};

/*
 * Enum: GESTURE_STATES
 * Define the states a gesture can be in.
 *
 * GESTURE_POSSIBLE     - The gesture is not recognised yet (default state).
 * GESTURE_BEGIN        - The gesture has begun.
 * GESTURE_UPDATE       - The gesture is in progress.
 */
enum {
    GESTURE_POSSIBLE = 0,
    GESTURE_BEGIN,
    GESTURE_UPDATE,
};

typedef struct gesture gesture_t;
struct gesture
{
    int     type;
    int     state;
    double  pos[2];
    double  start_pos[2][2];
    double  pinch;
    int     (*callback)(const gesture_t *gest, void *user);
};

// Pass a mouse event to a list of gestures.
int gesture_on_mouse(int n, gesture_t **gs, int id, int state,
                     double x, double y);

