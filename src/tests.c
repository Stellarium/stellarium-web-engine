/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#if COMPILE_TESTS

typedef struct test {
    struct test *next;
    const char *name;
    const char *file;
    void (*setup)(void);
    void (*func)(void);
    int flags;
} test_t;

static test_t *g_tests = NULL;

void tests_register(const char *name, const char *file,
                    void (*setup)(void),
                    void (*func)(void),
                    int flags)
{
    test_t *test;
    test = calloc(1, sizeof(*test));
    test->name = name;
    test->file = file;
    test->setup = setup;
    test->func = func;
    test->flags = flags;
    LL_APPEND(g_tests, test);
}

static bool filter_test(const char *filter, const test_t *test)
{
    if (!filter) return true;
    if (strcmp(filter, "auto") == 0) return test->flags & TEST_AUTO;
    return strstr(test->file, filter);
}

EMSCRIPTEN_KEEPALIVE
void tests_run(const char *filter)
{
    test_t *test;
    LOG_I("Run tests: %s", filter);
    LL_FOREACH(g_tests, test) {
        if (!filter_test(filter, test)) continue;
        if (test->setup) test->setup();
        test->func();
        // LOG_I("Run %-20s OK (%s)", test->name, test->file);
    }
}

bool tests_compare_time(double t, double ref, double max_delta_ms)
{
    double err = fabs(t - ref) * ERFA_DAYSEC * 1000;
    if (err > max_delta_ms) {
        LOG_E("Time delta: %.15f ms > %f ms", err, max_delta_ms);
        return false;
    }
    return true;
}

bool tests_compare_pv(const double pv[2][3], const double ref[2][3],
                       double max_delta_position,
                       double max_delta_velocity)
{
    double err[2][3], dp, dv;
    eraPvmpv(pv, ref, err);
    vec3_mul(ERFA_DAU / 1000, err[0], err[0]);
    vec3_mul(ERFA_DAU * 1000 / ERFA_DAYSEC, err[1], err[1]);
    dp = vec3_norm(err[0]);
    dv = vec3_norm(err[1]);
    if (dp > max_delta_position || dv > max_delta_velocity) {
        LOG_E("Position/Velocity delta: %.10f km, %.10f mm/s", dp, dv);
        return false;
    }
    return true;
}

#endif
