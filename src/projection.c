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

void projection_init(projection_t *p, int type, double fovy,
                     double w, double h)
{
    double aspect = w / h;
    memset(p, 0, sizeof(*p));
    p->window_size[0] = w;
    p->window_size[1] = h;
    p->fovy = fovy;
    p->klass = g_klasses[type];
    p->klass->init(p, fovy, aspect);
}

bool project_to_clip(const projection_t *proj, const double input[3],
                     double out[4])
{
    double p[4];
    bool ret;
    ret = proj->klass->project(proj, input, p);
    p[3] = 1.0;
    mat4_mul_vec4(proj->mat, p, out);
    return ret;
}

bool project_to_win(const projection_t *proj, const double input[3],
                    double out[3])
{
    double p[4];
    vec3_copy(input, p);
    proj->klass->project(proj, p, p);
    p[3] = 1.0;
    mat4_mul_vec4(proj->mat, p, p);
    if (!p[3]) return false;
    vec3_mul(1.0 / p[3], p, p);
    out[0] = (+p[0] + 1) / 2 * proj->window_size[0];
    out[1] = (-p[1] + 1) / 2 * proj->window_size[1];
    out[2] = (p[2] + 1) / 2;
    return true;
}

bool project_to_win_xy(const projection_t *proj, const double input[3],
                       double out[2])
{
    double win[3];
    if (!project_to_win(proj, input, win)) return false;
    out[0] = win[0];
    out[1] = win[1];
    return true;
}

bool unproject(const projection_t *proj,
               const double v[3], double out[3])
{
    double p[4], inv[4][4];
    assert(proj->klass->backward);
    p[0] = v[0] / proj->window_size[0] * 2 - 1;
    p[1] = 1 - v[1] / proj->window_size[1] * 2;
    p[2] = 2 * v[2] - 1;
    p[3] = 1;
    if (!mat4_invert(proj->mat, inv)) assert(false);
    mat4_mul_vec4(inv, p, p);
    return proj->klass->backward(proj, p, out);
}
