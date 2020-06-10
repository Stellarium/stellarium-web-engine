/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * This file is only used by the javascript at the moment.  We could
 * probably try to remove it totally at some point
 */

#include "swe.h"

// Events types
enum {
    EVENT_RISE      = 1 << 0,
    EVENT_SET       = 1 << 1,
};

// Newton algo.
#define NEWTON_MAX_STEPS 20
static double newton(double (*f)(double x, void *user),
                     double x0, double x1, double precision, void *user)
{
    int nbiter = 0;
    double f0, f1, tmp;
    double startDelta = fabs(x1 - x0);
    f0 = f(x0, user);
    f1 = f(x1, user);
    while (f1 && fabs(x1 - x0) > precision && f1 != f0) {
        tmp = x1;
        x1 = x1 + (x1 - x0) / (f0 / f1 - 1.0);
        x0 = tmp;
        // Check for diverging
        if (fabs(x1 - x0) > startDelta * 10)
            return NAN;
        f0 = f1;
        f1 = f(x1, user);
        if (nbiter++ > NEWTON_MAX_STEPS)
            return x1;
    }
    return x1;
}

static int sign(double x)
{
    return x < 0 ? -1 : 1;
}

static double find_zero(double (*f)(double x, void *user),
                        double x0, double x1, double step, double precision,
                        int rising, void *user)
{
    int last_sign = 0;
    double x, fx;
    bool found = false;
    // First find an approximate answer simply by stepping.  Not very clever.
    // Make sure the last iteration is exactly at x1.
    for (x = x0;; x += step) {
        if (x > x1) x = x1; // Clamp to x1.
        fx = f(x, user);
        if (sign(fx) * last_sign == -1 && sign(fx) == rising) {
            found = true;
            break;
        }
        last_sign = sign(fx);
        if (x == x1) break;
    }
    if (!found) return NAN;
    // Once we are near the value, use newton algorithm.
    return newton(f, x - step, x, precision, user);
}

static double rise_dist(double time, void *user)
{
    struct {
        observer_t *obs;
        obj_t *obj;
    } *data = user;
    double radius = 0, pvo[2][4], observed[4], az, alt;

    data->obs->tt = time;
    observer_update(data->obs, false);
    obj_get_pvo(data->obj, data->obs, pvo);
    convert_framev4(data->obs, FRAME_ICRF, FRAME_OBSERVED, pvo[0], observed);
    eraC2s(observed, &az, &alt);
    az = eraAnp(az);
    obj_get_info(data->obj, data->obs, INFO_RADIUS, &radius);
    return alt + radius - data->obs->horizon;
}

EMSCRIPTEN_KEEPALIVE
double compute_event(observer_t *obs,
                     obj_t *obj,
                     int event,
                     double start_time,
                     double end_time,
                     double precision)
{
    observer_t obs2 = *obs;
    double ret;
    int rising;
    struct {
        observer_t *obs;
        obj_t *obj;
    } data = {&obs2, obj};
    rising = event == EVENT_RISE ? +1 : -1;
    ret = find_zero(rise_dist, start_time, end_time,
                    (end_time - start_time) / 24, precision,
                    rising, &data);
    return ret;
}
