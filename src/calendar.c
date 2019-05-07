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

typedef struct cobj cobj_t;
typedef struct event event_t;
typedef struct event_type event_type_t;

// Keep reference to an object plus some extra info for fast event computations
struct cobj {
    obj_t *obj;
    double obs_z;  // Z value of observed position (if < 0 below horizon).
    double ra, de;
};

struct calendar
{
    observer_t obs;
    cobj_t *objs;
    int nb_objs;
    double start;
    double end;
    double time;
    event_t *events;
    int flags;
};

struct event_type
{
    const char *name;
    int nb_objs;
    int flags;
    double precision;
    // Compute the value for the event.
    double (*func)(const event_type_t *type,
                   const observer_t *obs,
                   const cobj_t *o1, const cobj_t *o2);
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
    const cobj_t *o1;
    const cobj_t *o2;
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
                               const cobj_t *o, const cobj_t *_)
{
    double ohpos[3], shpos[3];
    double pvo[2][4], olon, slon, lat;
    double v;
    if (memcmp(o->obj->type, type->obj_type, 4) != 0) return NAN;

    // Compute obj and sun geocentric ecliptic longitudes.
    obj_get_pvo(o->obj, obs, pvo);
    mat3_mul_vec3(obs->ri2e, pvo[0], ohpos);
    mat3_mul_vec3(obs->ri2e, obs->sun_pvo[0], shpos);
    eraC2s(ohpos, &olon, &lat);
    eraC2s(shpos, &slon, &lat);

    v = eraAnpm(olon - slon - type->target);
    if (fabs(v) > 15.0 * DD2R) return NAN;
    return v;
}

static double vertical_align_event_func(const event_type_t *type,
                                        const observer_t *obs,
                                        const cobj_t *o1, const cobj_t *o2)
{
    const char types[4][2][4] = {
        {"Moo", "Pla"},
        {"Moo", "*"},
        {"Pla", "Pla"},
        {"Pla", "*"},
    };
    double sep, o1_pvo[2][4], o2_pvo[2][4];
    int i;

    // Make sure the objects are of the right types.
    if (memcmp(o1->obj->type, o2->obj->type, 4) == 0 && (o1 > o2))
        return NAN;
    for (i = 0; i < ARRAY_SIZE(types); i++) {
        if (memcmp(o1->obj->type, types[i][0], 4) == 0 &&
            memcmp(o2->obj->type, types[i][1], 4) == 0) break;
    }
    if (i == ARRAY_SIZE(types)) return NAN;

    obj_get_pvo(o1->obj, obs, o1_pvo);
    obj_get_pvo(o2->obj, obs, o2_pvo);
    sep = eraSepp(o1_pvo[0], o2_pvo[0]);
    if (sep > 5 * DD2R) return NAN;
    return eraAnpm(o1->ra - o2->ra);
}

static int vertical_align_format(const event_t *ev, char *out, int len)
{
    char buf[64], buf1[128], buf2[128];
    double v, de1, de2;
    int prec;

    de1 = ev->o1->de;
    de2 = ev->o2->de;
    v = fabs(eraAnpm(de2 - de1));
    prec = (v < 2 * DD2R) ? 1 : 0;
    if (de1 < de2)
        sprintf(buf, "%.*f° south", prec, v * DR2D);
    else
        sprintf(buf, "%.*f° north", prec, v * DR2D);

    snprintf(out, len, "%s passes %s of %s",
             obj_get_name(ev->o1->obj, buf1), buf,
             obj_get_name(ev->o2->obj, buf2));
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
    char name[128];
    obj_get_name(ev->o1->obj, name);
    if (strcmp(ev->type->name, "conjunction") == 0) {
        snprintf(out, len, "%s is in conjunction with the Sun", name);
    }
    if (strcmp(ev->type->name, "opposition") == 0) {
        snprintf(out, len, "%s is in opposition", name);
    }
    return 0;
}


static const event_type_t event_types[] = {
    {
        .name = "moon-new",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Moo",
        .target = 0,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-full",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Moo",
        .target = 180 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-first-quarter",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Moo",
        .target = 90 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "moon-last-quarter",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Moo",
        .target = -90 * DD2R,
        .precision = DMIN,
        .format = moon_format,
    },
    {
        .name = "conjunction",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Pla",
        .target = 0,
        .precision = DMIN,
        .format = conjunction_format,
    },
    {
        .name = "opposition",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "Pla",
        .target = 180 * DD2R,
        .precision = DMIN,
        .format = conjunction_format,
    },
    {
        .name = "opposition",
        .nb_objs = 1,
        .func = conjunction_func,
        .obj_type = "MPl",
        .target = 180 * DD2R,
        .precision = DHOUR,
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
                          const char *desc, int flags,
                          obj_t *o1, obj_t *o2,
                          void *user)
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
    core_init(100, 100, 1);
    utcoffset = core->utc_offset / 60.0 / 24;
    start = time_set_dtf(core->observer->utc, utcoffset, -1, -1, 1, 0, 0, 0);
    end   = time_add_dtf(start, utcoffset, 0, 1, 0, 0, 0, 0);
    printf("from %s to %s\n", format_time(b1, start, utcoffset, NULL),
                              format_time(b2, end,   utcoffset, NULL));
    calendar_get(core->observer, start, end, CALENDAR_HIDDEN,
                 USER_PASS(&utcoffset), print_callback);
}

static bool is_obj_hidden(cobj_t *obj, const observer_t *obs)
{
    if (!obj) return true;
    return obj->obs_z < 0;
}

static int check_event(const event_type_t *ev_type,
                       const observer_t *obs,
                       const cobj_t *o1, const cobj_t *o2,
                       int flags,
                       event_t **events)
{
    double v, time = obs->tt;
    event_t *ev;
    bool hidden = false;

    hidden = is_obj_hidden(o1, obs) && is_obj_hidden(o2, obs);
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

static void cobj_update(cobj_t *o, const observer_t *obs)
{
    double pvo[2][4], p[3];
    if (!o) return;
    obj_get_pvo(o->obj, obs, pvo);
    eraC2s(pvo[0], &o->ra, &o->de);
    o->ra = eraAnp(o->ra);
    o->de = eraAnp(o->de);
    convert_framev4(obs, FRAME_ICRF, FRAME_OBSERVED, pvo[0], p);
    o->obs_z = p[2];
}


// Function that can be used in the newton algo (slow).
static double newton_fn_(double time, void *user)
{
    double ret;
    observer_t *obs = USER_GET(user, 0);
    event_t *ev = USER_GET(user, 1);
    obs->tt = time;
    observer_update(obs, true);
    if (ev->o1) cobj_update(ev->o1, obs);
    if (ev->o2) cobj_update(ev->o2, obs);
    ret = ev->type->func(ev->type, obs, ev->o1, ev->o2);
    assert(!isnan(ret));
    return ret;
}

static int event_cmp(const void *e1, const void *e2)
{
    return cmp(((const event_t*)e1)->time, ((const event_t*)e2)->time);
}

static int obj_add_f(void *user, obj_t *obj)
{
    cobj_t *objs = USER_GET(user, 0);
    int *i = USER_GET(user, 1);
    if (obj->oid == oid_create("HORI", 399)) return 0; // Skip Earth.
    if (objs) {
        assert(objs[*i].obj == NULL);
        objs[*i].obj = obj;
        obj->ref++;
    }
    (*i)++;
    return 0;
}

// Function that list all the objects we consider in the calendar computation.
static void list_objs(observer_t *obs, void *user,
                      int (*f)(void *user, obj_t *obj))
{
    obj_t *stars, *planets, *asteroids;

    // All stars and planets up to mag 2.
    stars = core_get_module("stars");
    module_list_objs(stars, obs, 2.0, 0, user, f);
    planets = core_get_module("planets");
    module_list_objs(planets, obs, 2.0, 0, user, f);
    // All asteroids up to mag 10.
    asteroids = core_get_module("minor_planets");
    module_list_objs(asteroids, obs, 10.0, 0, user, f);
}

EMSCRIPTEN_KEEPALIVE
calendar_t *calendar_create(const observer_t *obs,
                            double start, double end, int flags)
{
    calendar_t *cal;
    cal = calloc(1, sizeof(*cal));
    int i;

    cal->obs = *obs;
    // Make a full update at mid time, so that we can do fast update after that
    // while still keeping a good precision.
    cal->obs.tt = (start + end) / 2;
    observer_update(&cal->obs, false);

    // Create all the objects.
    i = 0;
    list_objs(&cal->obs, USER_PASS(NULL, &i), obj_add_f);
    cal->nb_objs = i;
    cal->objs = calloc(cal->nb_objs, sizeof(*cal->objs));
    i = 0;
    list_objs(&cal->obs, USER_PASS(cal->objs, &i), obj_add_f);

    cal->flags = flags;
    cal->start = start;
    cal->end = end;
    cal->time = start;

    return cal;
}

EMSCRIPTEN_KEEPALIVE
void calendar_delete(calendar_t *cal)
{
    int i;
    event_t *ev, *ev_tmp;

    // Release all objects.
    for (i = 0; i < cal->nb_objs; i++) {
        obj_release(cal->objs[i].obj);
    }
    free(cal->objs);
    // Delete events.
    DL_FOREACH_SAFE(cal->events, ev, ev_tmp) free(ev);
    free(cal);
}

EMSCRIPTEN_KEEPALIVE
int calendar_compute(calendar_t *cal)
{
    double step = DHOUR;
    int i, j;
    cobj_t *o1, *o2;
    const event_type_t *ev_type;
    event_t *ev, *ev_tmp;

    if (cal->time >= cal->end) goto end;

    // Only compute event for one time iteration.
    cal->obs.tt = cal->time;
    observer_update(&cal->obs, true);
    for (i = 0; i < cal->nb_objs; i++) {
        cobj_update(&cal->objs[i], &cal->obs);
    }
    // Check one body events.
    for (i = 0; i < cal->nb_objs; i++) {
        o1 = &cal->objs[i];
        for (ev_type = &event_types[0]; ev_type->func; ev_type++) {
            if (ev_type->nb_objs == 1) {
                check_event(ev_type, &cal->obs, o1, NULL, cal->flags,
                            &cal->events);
            }
        }
    }
    // Check two bodies events.
    for (i = 0; i < cal->nb_objs; i++)
    for (j = 0; j < cal->nb_objs; j++) {
        o1 = &cal->objs[i];
        o2 = &cal->objs[j];
        if (o1 == o2) continue;
        for (ev_type = &event_types[0]; ev_type->func; ev_type++) {
            if (ev_type->nb_objs == 2) {
                check_event(ev_type, &cal->obs, o1, o2, cal->flags,
                            &cal->events);
            }
        }
    }
    clean_events(&cal->events, &cal->obs);
    cal->time += step;
    return 1;

end:
    // Remove all the tmp events
    DL_FOREACH_SAFE(cal->events, ev, ev_tmp) {
        if (ev->status != EV_STATE_FOUND) {
            DL_DELETE(cal->events, ev);
            free(ev);
        }
    }
    // Compute fine value using newton algo.
    DL_FOREACH(cal->events, ev) {
        if (ev->type->precision >= step) continue;
        ev->time = newton(newton_fn_,
                          ev->time_range[0], ev->time_range[1],
                          ev->type->precision, USER_PASS(&cal->obs, ev));
    }
    DL_SORT(cal->events, event_cmp);
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int calendar_get_results(calendar_t *cal, void *user,
                         int (*callback)(double ut1, const char *type,
                         const char *desc, int flags,
                         obj_t *o1, obj_t *o2,
                         void *user))
{
    int n = 0;
    char buf[128];
    event_t *ev;
    DL_FOREACH(cal->events, ev) {
        cal->obs.tt = ev->time;
        observer_update(&cal->obs, true);
        cobj_update(ev->o1, &cal->obs);
        cobj_update(ev->o2, &cal->obs);
        ev->type->format(ev, buf, ARRAY_SIZE(buf));
        callback(ev->time, ev->type->name, buf, ev->flags,
                 ev->o1->obj, ev->o2 ? ev->o2->obj : NULL, user);
        n++;
    }
    return n;
}

EMSCRIPTEN_KEEPALIVE
int calendar_get(
        const observer_t *obs,
        double start, double end, int flags, void *user,
        int (*callback)(double tt, const char *type,
                        const char *desc, int flags,
                        obj_t *o1, obj_t *o2,
                        void *user))
{
    calendar_t *cal;
    cal = calendar_create(obs, start, end, flags);
    while (calendar_compute(cal)) {}
    calendar_get_results(cal, user, callback);
    calendar_delete(cal);
    return 0;
}
