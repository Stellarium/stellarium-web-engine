/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "projection.h"

#include "tests.h"
#include "utils/vec.h"

#include <assert.h>
#include <math.h>
#include <string.h>


/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)

void proj_perspective_init(projection_t *p, double fov, double aspect);
void proj_stereographic_init(projection_t *p, double fov, double aspect);
void proj_mercator_init(projection_t *p, double fov, double aspect);

void projection_init(projection_t *p, int type, double fov, double aspect)
{
    memset(p, 0, sizeof(*p));
    switch (type) {
        case PROJ_PERSPECTIVE:
            proj_perspective_init(p, fov, aspect);
            break;
        case PROJ_STEREOGRAPHIC:
            proj_stereographic_init(p, fov, aspect);
            break;
        case PROJ_MERCATOR:
            proj_mercator_init(p, fov, aspect);
            break;
        default:
            assert(false);
    }
}

bool project(const projection_t *proj, int flags, int out_dim,
             const double *v, double *out)
{
    double p[4] = {0, 0, 0, 1};
    bool visible;

    // In case we forgot to init the proj to 0.
    assert(!isnan(proj->offset[0]));

    if (flags & PROJ_BACKWARD) {
        vec2_sub(v, proj->offset, p);
        assert(proj->backward);
        assert(out_dim == 4);
        proj->backward(proj, flags, p, out);
        return true;
    }
    assert(proj->project);
    vec3_copy(v, p);
    proj->project(proj, flags, v, p);

    vec2_add(p, proj->offset, p);
    if (!(flags & PROJ_TO_NDC_SPACE)) {
        memcpy(out, p, out_dim * sizeof(double));
        return true;
    }
    assert(!(proj->flags & PROJ_NO_CLIP));
    visible = (p[0] >= -p[3] && p[0] <= +p[3] &&
               p[1] >= -p[3] && p[1] <= +p[3] &&
               p[2] >= -p[3] && p[2] <= +p[3]);
    if (p[3])
        vec3_mul(1.0 / p[3], p, p);
    p[3] = visible ? 1.0 : 0.0; // Not sure this is proper...
    memcpy(out, p, out_dim * sizeof(double));
    return visible;
}

// n can be 2 (for a line) or 4 (for a quad).
int projection_intersect_discontinuity(const projection_t *proj,
                                        double (*pos)[4], int n)
{
    int i, r, ret = 0;
    const int NEXT[4] = {1, 3, 0, 2};
    if (!proj->intersect_discontinuity) return 0;
    assert(n == 2 || n == 4);
    if (n == 2)
        return proj->intersect_discontinuity(proj, pos[0], pos[1]);
    for (i = 0; i < 4; i++) {
        r = proj->intersect_discontinuity(proj, pos[i], pos[NEXT[i]]);
        ret |= r;
        if (ret == (PROJ_INTERSECT_DISCONTINUITY | PROJ_CANNOT_SPLIT))
            break;
    }
    return ret;
}

/******* TESTS **********************************************************/
#ifdef COMPILE_TESTS

static void test_projs(void)
{
    double a[3] = {1, 0, -1};
    double b[3], c[4];
    projection_t proj;
    projection_init(&proj, PROJ_PERSPECTIVE, 90 * DD2R, 1);
    project(&proj, 0, 3, a, b);
    assert(vec3_dist(b, VEC(1, 0, 1)) < 0.0001);
    project(&proj, PROJ_BACKWARD, 4, b, c);
    vec3_normalize(a, b);
    assert(vec2_dist(c, b) < 0.0001);
}

TEST_REGISTER(NULL, test_projs, TEST_AUTO);

#endif
