/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#define DHOUR (1.0 / 24.0)
#define DMIN  (DHOUR / 60.0)

typedef struct event event_t;
typedef struct event_type event_type_t;

static inline uint32_t s4toi(const char s[4])
{
    return ((uint32_t)s[0] << 0) +
           ((uint32_t)s[1] << 8) +
           ((uint32_t)s[2] << 16) +
           ((uint32_t)s[2] << 24);
}

struct event_type
{
    const char *name;
    int nb_objs;
    int flags;
    double precision;
    // Compute the value for the event.
    double (*func)(const event_type_t *type,
                   const observer_t *obs,
                   const obj_t *o1, const obj_t *o2);
    // Can be used by the function.
    char   obj_type[4];
    double target;

    // Generate the event description string.
    int (*format)(const event_t *ev, char *out, int len);
};

// Status of events.
enum {
    EV_STATE_NULL,
    EV_STATE_MAYBE,
    EV_STATE_FOUND,
};

struct event
{
    event_t *next, *prev;
    const event_type_t *type;
    const obj_t *o1;
    const obj_t *o2;
    double time;
    // Range into wich the event occured, and the test function returns a
    // value != NAN.
    double time_range[2];
    int flags;

    double v;
    int status;
};

// Newton algo.
// XXX: should I move this into algos ?
static double newton(double (*f)(double x, void *user),
                     double x0, double x1, double precision, void *user)
{
    double f0, f1, tmp;
    f0 = f(x0, user);
    f1 = f(x1, user);
    while (f1 && fabs(x1 - x0) > precision && f1 != f0) {
        tmp = x1;
        x1 = x1 + (x1 - x0) / (f0 / f1 - 1.0);
        x0 = tmp;
        f0 = f1;
        f1 = f(x1, user);
    }
    return x1;
}

static double conjunction_func(const event_type_t *type,
                               const observer_t *obs,
                               const obj_t *o1, const obj_t *sun)
{
    double ohpos[3], shpos[3];
    double olon, slon, lat;
    double v;
    if (s4toi(o1->type) != s4toi(type->obj_type) ||
        s4toi(sun->type) != s4toi("SUN ")) return NAN;

    // Compute obj and sun geocentric ecliptic longitudes.
    mat4_mul_vec3(obs->ri2e, o1->pos.pvg[0], ohpos);
    mat4_mul_vec3(obs->ri2e, sun->pos.pvg[0], shpos);
    eraC2s(ohpos, &olon, &lat);
    eraC2s(shpos, &slon, &lat);

    v = eraAnpm(olon - slon - type->target);
    if (fabs(v) > 15.0 * DD2R) return NAN;
    return v;
}

static double vertical_align_event_func(const event_type_t *type,
                                        const observer_t *obs,
                                        const obj_t *o1, const obj_t *o2)
{
    const char types[4][2][4] = {
        {"MOO ", "PLA "},
        {"MOO ", "*   "},
        {"PLA ", "PLA "},
        {"PLA ", "*   "},
    };
    double sep;
    int i;

    // Make sure the objects are of the right types.
    if ((s4toi(o1->type) == s4toi(o2->type)) && o1 > o2) return NAN;
    for (i = 0; i < ARRAY_SIZE(types); i++) {
        if (s4toi(o1->type) == s4toi(types[i][0]) &&
            s4toi(o2->type) == s4toi(types[i][1])) break;
    }
    if (i == ARRAY_SIZE(types)) return NAN;

    sep = eraSepp(o1->pos.pvg[0], o2->pos.pvg[0]);
    if (sep > 5 * DD2R) return NAN;
    return eraAnpm(o1->pos.ra - o2->pos.ra);
}

static int vertical_align_format(const event_t *ev, char *out, int len)
{
    char buf[64];
    double v;
    int prec;
    v = fabs(eraAnpm(ev->o1->pos.dec - ev->o2->pos.dec));
    prec = (v < 2 * DD2R) ? 1 : 0;
    if (ev->o1->pos.dec < ev->o2->pos.dec)
        sprintf(buf, "%.*f° south", prec, v * DR2D);
    else
        sprintf(buf, "%.*f° north", prec, v * DR2D);

    snprintf(out, len, "%s passes %s of %s",
             obj_get_name(ev->o1), buf, obj_get_name(ev->o2));
    return 0;
}


static int moon_format(const event_t *ev, char *out, int len)
{
    if (strcmp(ev->type->name, "moon-new") == 0)
        snprintf(out, len, "New Moon");
    if (strcmp(ev->type->name, "moon-full") == 0)
        snprintf(out, len, "Full Moon");
    if (strcmp(ev->type->name,  "moon-first-quarter") == 0)
        snprintf(out, len, "First Quarter Moon");
    if (strcmp(ev->type->name,  "moon-last-quarter") == 0)
        snprintf(out, len, "Last Quarter Moon");
    return 0;
}

static int conjunction_format(const event_t *ev, char *out, int len)
{
    if (strcmp(ev->type->name, "conjunction") == 0)
        snprintf(out, len, "Conjunction %s %s",
                 obj_get_name(ev->o1), obj_get_name(ev->o2));
    if (strcmp(ev->type->name, "opposition") == 0)
        snprintf(out, len, "%s is in opposition", obj_get_name(ev->o1));
    return 0;
}


static const event_type_t event_types[] = {
    {
        .name = "moon-new",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "MOO ",
        .target = 0,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-full",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "MOO ",
        .target = 180 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-first-quarter",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "MOO ",
        .target = 90 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-last-quarter",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "MOO ",
        .target = -90 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "conjunction",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "PLA ",
        .target = 0,
        .precision = DMIN,
        .format = conjunction_format,
    },
    {
        .name = "opposition",
        .nb_objs = 2,
        .func = conjunction_func,
        .obj_type = "PLA ",
        .target = 180 * DD2R,
        .precision = DMIN,
        .format = conjunction_format,
    },
    {
        .name = "valign",
        .nb_objs = 2,
        .func = vertical_align_event_func,
        .precision = DHOUR,
        .format = vertical_align_format,
    },
    {},
};

static int print_callback(double time, const char *type,
                          const char *desc, int flags, void *user)
{
    char b1[64], b2[64];
    double *utcofs = USER_GET(user, 0);
    printf("%s %s: %s\n", format_time(b1, time, *utcofs, "YYYY-MM-DD"),
                          format_time(b2, time, *utcofs, "HH:mm"),
                          desc);
    return 0;
}

void calendar_print(void)
{
    double start, end;
    double utcoffset;
    char b1[64], b2[64];
    core_init();
    utcoffset = core->utc_offset / 60.0 / 24;
    start = time_set_dtf(core->observer->utc, utcoffset, -1, -1, 1, 0, 0, 0);
    end   = time_add_dtf(start, utcoffset, 0, 1, 0, 0, 0, 0);
    printf("from %s to %s\n", format_time(b1, start, utcoffset, NULL),
                              format_time(b2, end,   utcoffset, NULL));
    calendar_get(core->observer, start, end, CALENDAR_HIDDEN,
                 USER_PASS(&utcoffset), print_callback);
}

static int check_event(const event_type_t *ev_type,
                       const observer_t *obs,
                       const obj_t *o1, const obj_t *o2,
                       int flags,
                       event_t **events)
{
    double v, time = obs->tt;
    event_t *ev;
    bool hidden = (o1->pos.alt < 0) && (!o2 || o2->pos.alt < 0);
    if (!(flags & CALENDAR_HIDDEN) && hidden)
        v = NAN;
    else
        v = ev_type->func(ev_type, obs, o1, o2);
    if (isnan(v)) return 0;
    DL_FOREACH(*events, ev) {
        if (!ev->type) break;
        if (ev->status != EV_STATE_MAYBE) continue;
        if (ev->type == ev_type && ev->o1 == o1 && ev->o2 == o2)
            break;
    }
    if (!ev) {
        ev = calloc(1, sizeof(*ev));
        DL_APPEND(*events, ev);
        ev->type = ev_type;
        ev->v = v;
        ev->o1 = o1;
        ev->o2 = o2;
        ev->time = time;
        ev->time_range[0] = ev->time_range[1] = time;
        ev->status = EV_STATE_MAYBE;
        ev->flags = hidden ? CALENDAR_HIDDEN : 0;
    } else {
        if (!hidden) ev->flags &= !CALENDAR_HIDDEN;
        if (v * ev->v <= 0.0) {
            ev->time = time;
            ev->time_range[1] = time;
            ev->time_range[0] = time - DHOUR;
            ev->status = EV_STATE_FOUND;
        }
        ev->v = v;
    }
    return 0;
}

static int clean_events(event_t **events, const observer_t *obs) {
    double v;
    event_t *ev, *tmp;
    DL_FOREACH_SAFE(*events, ev, tmp) {
        if (ev->status == EV_STATE_MAYBE) {
            v = ev->type->func(ev->type, obs, ev->o1, ev->o2);
            if (isnan(v)) {
                DL_DELETE(*events, ev);
                free(ev);
            }
        }
    }
    return 0;
}

// Function that can be used in the newton algo (slow).
static double newton_fn_(double time, void *user)
{
    double ret;
    observer_t *obs = USER_GET(user, 0);
    event_t *ev = USER_GET(user, 1);
    obs->tt = time;
    observer_update(obs, true);
    if (ev->o1) obj_update((obj_t*)ev->o1, obs, 0);
    if (ev->o2) obj_update((obj_t*)ev->o2, obs, 0);
    ret = ev->type->func(ev->type, obs, ev->o1, ev->o2);
    assert(!isnan(ret));
    return ret;
}

static int event_cmp(const void *e1, const void *e2)
{
    return cmp(((const event_t*)e1)->time, ((const event_t*)e2)->time);
}

static int obj_add_f(const char *id, void *user)
{
    obj_t **objs = USER_GET(user, 0);
    int *i = USER_GET(user, 1);
    if (strcmp(id, "EARTH") == 0) return 0;
    assert(objs[*i] == NULL);
    objs[*i] = obj_get(NULL, id, 0);
    (*i)++;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int calendar_get(
        const observer_t *obs_,
        double start, double end, int flags, void *user,
        int (*callback)(double tt, const char *type,
                        const char *desc, int flags, void *user))
{
    int i, j, nb;
    double step = DHOUR, time;
    observer_t obs = *obs_;
    obj_t **objs, *o1, *o2;
    const event_type_t *ev_type;
    event_t *events = NULL, *ev, *ev_tmp;
    char buf[128];
    assert(callback);

    // Create all the objects.
    // -1 because we skip the earth.
    nb = obj_list(&core->obj, 2.0, NULL, NULL) - 1;
    objs = calloc(nb, sizeof(*objs));
    i = 0;
    obj_list(&core->obj, 2.0, USER_PASS(objs, &i), obj_add_f);

    // Slow update at mid time, so that we can do fast update after that
    // while still keeping a good precision.
    obs.tt = (start + end) / 2;
    observer_update(&obs, false);

    for (time = start; time < end; time += step) {
        obs.tt = time;
        observer_update(&obs, true);

        for (i = 0; i < nb; i++)
            obj_update(objs[i], &obs, 0);

        // Check two bodies events.
        for (i = 0; i < nb; i++)
        for (j = 0; j < nb; j++) {
            o1 = objs[i];
            o2 = objs[j];
            if (o1 == o2) continue;
            for (ev_type = &event_types[0]; ev_type->func; ev_type++) {
                if (ev_type->nb_objs == 2)
                    check_event(ev_type, &obs, o1, o2, flags, &events);
            }
        }
        clean_events(&events, &obs);
    }

    // Remove all the tmp events
    DL_FOREACH_SAFE(events, ev, ev_tmp) {
        if (ev->status != EV_STATE_FOUND) {
            DL_DELETE(events, ev);
            free(ev);
        }
    }

    // Compute fine value using newton algo.
    DL_FOREACH(events, ev) {
        if (ev->type->precision >= step) continue;
        ev->time = newton(newton_fn_,
                          ev->time_range[0], ev->time_range[1],
                          ev->type->precision, USER_PASS(&obs, ev));
    }

    // Return all the found events.
    DL_SORT(events, event_cmp);
    DL_FOREACH(events, ev) {
        obs.tt = ev->time;
        observer_update(&obs, true);
        if (ev->o1) obj_update((obj_t*)ev->o1, &obs, 0);
        if (ev->o2) obj_update((obj_t*)ev->o2, &obs, 0);
        ev->type->format(ev, buf, ARRAY_SIZE(buf));
        callback(ev->time, ev->type->name, buf, ev->flags, user);
    }

    DL_FOREACH_SAFE(events, ev, ev_tmp) free(ev);
    for (i = 0; i < nb; i++) {
        obj_delete(objs[i]);
    }
    free(objs);
    return 0;
}
