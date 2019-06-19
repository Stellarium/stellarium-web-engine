/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "vec.h"
#include "tests.h"

#if !VEC_INLINE
    #define VEC_IMPLEMENTATION
    #include "vec.h"
#endif

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

static void test_caps(void)
{
    const double p0[3] = {1, 0, 0};
    const double p1[3] = {-1, 0, 0};
    double p2[3] = {1, 1, 1};
    vec3_normalize(p2, p2);
    const double p3[3] = {0, 1, 0};

    double h0[4] = {VEC3_SPLIT(p0), 0};
    double h1[4] = {VEC3_SPLIT(p0), 0.8};
    double h2[4] = {VEC3_SPLIT(p0), -0.5};
    double h3[4] = {VEC3_SPLIT(p1), 0.5};
    double h4[4] = {VEC3_SPLIT(p2), 0.8};
    double h5[4] = {VEC3_SPLIT(p2), 1};
    double h6[4] = {VEC3_SPLIT(p1), 0};

    assert(cap_contains_vec3(h0, p0));
    assert(cap_contains_vec3(h1, p0));
    assert(cap_contains_vec3(h0, p3));
    assert(cap_contains_vec3(h6, p3));

    assert(cap_intersects_cap(h0, h1));
    assert(cap_intersects_cap(h0, h2));
    assert(cap_intersects_cap(h1, h2));
    assert(cap_intersects_cap(h4, h1));
    assert(!cap_intersects_cap(h0, h3));
    assert(!cap_intersects_cap(h1, h3));
    assert(cap_intersects_cap(h2, h3));
    assert(cap_intersects_cap(h0, h5));

    assert(cap_intersects_cap(h0, h0));
    assert(cap_intersects_cap(h1, h1));
    assert(cap_intersects_cap(h2, h2));
    assert(cap_intersects_cap(h3, h3));
    assert(cap_intersects_cap(h4, h4));
    assert(cap_intersects_cap(h5, h5));
    assert(cap_intersects_cap(h6, h0));
    assert(cap_intersects_cap(h0, h6));

    assert(cap_contains_cap(h0, h1));
    assert(!cap_contains_cap(h1, h0));
    assert(cap_contains_cap(h2, h0));
    assert(!cap_contains_cap(h0, h2));
    assert(!cap_contains_cap(h6, h0));
    assert(!cap_contains_cap(h0, h6));
    assert(cap_contains_cap(h2, h1));
    assert(!cap_contains_cap(h1, h2));
    assert(!cap_contains_cap(h0, h3));
    assert(!cap_contains_cap(h1, h3));
    assert(cap_contains_cap(h0, h5));
    assert(cap_contains_cap(h2, h5));
    assert(!cap_contains_cap(h5, h0));
    assert(!cap_contains_cap(h5, h1));
    assert(!cap_contains_cap(h5, h2));
    assert(!cap_contains_cap(h5, h3));
    assert(!cap_contains_cap(h5, h4));
    assert(cap_contains_cap(h0, h0));
    assert(cap_contains_cap(h1, h1));
    assert(cap_contains_cap(h2, h2));
    assert(cap_contains_cap(h3, h3));
    assert(cap_contains_cap(h4, h4));
    assert(cap_contains_cap(h5, h5));
}

TEST_REGISTER(NULL, test_caps, TEST_AUTO);

#endif
