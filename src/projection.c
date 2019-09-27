/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "projection.h"
#include "profiler.h"

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
void proj_hammer_init(projection_t *p, double fov, double aspect);
void proj_mollweide_init(projection_t *p, double fov, double aspect);

void proj_stereographic_compute_fov(double fov, double aspect,
                                    double *fovx, double *fovy);
void proj_mercator_compute_fov(double fov, double aspect,
                               double *fovx, double *fovy);
void proj_hammer_compute_fov(double fov, double aspect,
                             double *fovx, double *fovy);
void proj_mollweide_compute_fov(double fov, double aspect,
                                double *fovx, double *fovy);

void projection_compute_fovs(int type, double fov, double aspect,
                             double *fovx, double *fovy)
{
    switch (type) {
        case PROJ_STEREOGRAPHIC:
            proj_stereographic_compute_fov(fov, aspect, fovx, fovy);
            break;
        case PROJ_MERCATOR:
            proj_mercator_compute_fov(fov, aspect, fovx, fovy);
            break;
        case PROJ_HAMMER:
            proj_hammer_compute_fov(fov, aspect, fovx, fovy);
            break;
        case PROJ_MOLLWEIDE:
            proj_hammer_compute_fov(fov, aspect, fovx, fovy);
            break;
        default:
            assert(false);
    }
}

void projection_init(projection_t *p, int type, double fov,
                     double w, double h)
{
    double aspect = w / h;
    memset(p, 0, sizeof(*p));
    p->window_size[0] = w;
    p->window_size[1] = h;
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
        case PROJ_HAMMER:
            proj_hammer_init(p, fov, aspect);
            break;
        case PROJ_MOLLWEIDE:
            proj_mollweide_init(p, fov, aspect);
            break;
        default:
            assert(false);
    }
}

bool project(const projection_t *proj, int flags, int out_dim,
             const double *v, double *out)
{
    PROFILE(project, PROFILE_AGGREGATE);
    double p[4] = {0, 0, 0, 1};
    bool visible;

    if (flags & PROJ_BACKWARD) {
        vec2_copy(v, p);
        if (proj->flags & PROJ_FLIP_HORIZONTAL) p[0] = -p[0];
        if (proj->flags & PROJ_FLIP_VERTICAL)   p[1] = -p[1];
        assert(proj->backward);
        assert(out_dim == 4);
        return proj->backward(proj, flags, p, out);
    }
    assert(proj->project);
    vec3_copy(v, p);
    if (flags & PROJ_ALREADY_NORMALIZED)
        assert(vec3_is_normalized(p));
    proj->project(proj, flags, v, p);
    if (proj->flags & PROJ_FLIP_HORIZONTAL) p[0] = -p[0];
    if (proj->flags & PROJ_FLIP_VERTICAL)   p[1] = -p[1];

    if (!(flags & (PROJ_TO_NDC_SPACE | PROJ_TO_WINDOW_SPACE))) {
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
    if (flags & PROJ_TO_WINDOW_SPACE) {
        p[0] = (+p[0] + 1) / 2 * proj->window_size[0];
        p[1] = (-p[1] + 1) / 2 * proj->window_size[1];
    }
    memcpy(out, p, out_dim * sizeof(double));
    return visible;
}

/******* TESTS **********************************************************/
#ifdef COMPILE_TESTS

static void test_projs(void)
{
    double a[3] = {1, 0, -1};
    double b[3], c[4];
    projection_t proj;
    projection_init(&proj, PROJ_PERSPECTIVE, 90 * DD2R, 1, 1);
    project(&proj, 0, 3, a, b);
    assert(vec3_dist(b, VEC(1, 0, 1)) < 0.0001);
    project(&proj, PROJ_BACKWARD, 4, b, c);
    vec3_normalize(a, b);
    assert(vec2_dist(c, b) < 0.0001);
}

TEST_REGISTER(NULL, test_projs, TEST_AUTO);

#endif
