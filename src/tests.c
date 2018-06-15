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
        LOG_I("Run %-20s OK (%s)", test->name, test->file);
    }
}

#endif
