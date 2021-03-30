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

static projection_klass_t *g_klasses[PROJ_COUNT];

void proj_register_(projection_klass_t *klass)
{
    g_klasses[klass->id] = klass;
}

void projection_compute_fovs(int type, double fov, double aspect,
                             double *fovx, double *fovy)
{
    assert(g_klasses[type]->compute_fovs);
    g_klasses[type]->compute_fovs(type, fov, aspect, fovx, fovy);
}

void projection_init(projection_t *p, int type, double fov,
                     double w, double h)
{
    double aspect = w / h;
    memset(p, 0, sizeof(*p));
    p->window_size[0] = w;
    p->window_size[1] = h;
    p->fovx = fov;
    p->klass = g_klasses[type];
    p->klass->init(p, fov, aspect);
}

bool project(const projection_t *proj, int flags,
             const double v[static 4],
             double out[static 4])
{
    double p[4] = {0, 0, 0, 1};
    bool visible;

    assert(proj->klass->project);
    vec3_copy(v, p);
    if (flags & PROJ_ALREADY_NORMALIZED)
        assert(vec3_is_normalized(p));
    proj->klass->project(proj, flags, v, p);
    if (proj->flags & PROJ_FLIP_HORIZONTAL) p[0] = -p[0];
    if (proj->flags & PROJ_FLIP_VERTICAL)   p[1] = -p[1];

    if (!(flags & PROJ_TO_WINDOW_SPACE)) {
        memcpy(out, p, 4 * sizeof(double));
        return true;
    }
    visible = (p[0] >= -p[3] && p[0] < +p[3] &&
               p[1] >= -p[3] && p[1] < +p[3] &&
               p[2] >= -p[3] && p[2] < +p[3]);
    if (p[3])
        vec3_mul(1.0 / p[3], p, p);
    p[3] = visible ? 1.0 : 0.0; // Not sure this is proper...
    if (flags & PROJ_TO_WINDOW_SPACE) {
        p[0] = (+p[0] + 1) / 2 * proj->window_size[0];
        p[1] = (-p[1] + 1) / 2 * proj->window_size[1];
        p[2] = -v[2]; // Z get the distance in view space.
    }
    memcpy(out, p, 4 * sizeof(double));
    return visible;
}

bool unproject(const projection_t *proj, int flags,
               const double v[4], double out[4])
{
    double p[4] = {0, 0, 0, 1};
    vec2_copy(v, p);
    if (flags & PROJ_FROM_WINDOW_SPACE) {
        p[0] = p[0] / proj->window_size[0] * 2 - 1;
        p[1] = 1 - p[1] / proj->window_size[1] * 2;
    }
    if (proj->flags & PROJ_FLIP_HORIZONTAL) p[0] = -p[0];
    if (proj->flags & PROJ_FLIP_VERTICAL)   p[1] = -p[1];
    assert(proj->klass->backward);
    return proj->klass->backward(proj, flags, p, out);
}
