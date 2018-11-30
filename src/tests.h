/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#if COMPILE_TESTS

enum {
    TEST_AUTO = 1 << 0,
};

void tests_register(const char *name, const char *file,
                    void (*setup)(void),
                    void (*func)(void),
                    int flags);

void tests_run(const char *filter);

bool tests_compare_time(double t, double ref, double max_delta_ms);
bool tests_compare_pv(const double pv[2][3], const double ref[2][3],
                       double max_delta_position,
                       double max_delta_velocity);

#define test_str(v, expected) do { \
    if (strcmp(v, expected) != 0) { \
        LOG_E("Expected '%s', got '%s'", expected, v); \
        assert(false); } \
} while (0)

#define test_float(v, expected, e) do { \
    if (fabs((v) - (expected)) > (e)) { \
        LOG_E("Expected '%f', got '%f'", (expected), (v)); \
        assert(false); } \
} while (0)

#define TEST_REGISTER(setup_, func_, flags_) \
    static void register_test_##func_() __attribute__((constructor)); \
    static void register_test_##func_() { \
        tests_register(#func_, __FILE__, setup_, func_, flags_); }

#else // COMPILE_TEST

#define TEST_REGISTER(...)
static inline void tests_run(const char *filter) {}

#endif
