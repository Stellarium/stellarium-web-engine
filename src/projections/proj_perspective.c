/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "projection.h"
#include "utils/vec.h"

/* Degrees to radians */
#define DD2R (1.745329251994329576923691e-2)
/* Radians to degrees */
#define DR2D (57.29577951308232087679815)
#define DAU (149597870.7e3)
#define DM2AU  (1. / DAU)

static bool proj_perspective_project(
        const projection_t *proj, const double v[3], double out[3])
{
    vec3_copy(v, out);
    return true;
}

static bool proj_perspective_backward(const projection_t *proj,
        const double v[3], double out[3])
{
    vec3_copy(v, out);
    return true;
}

static void proj_perspective_compute_fov(int id, double fov, double aspect,
                                         double *fovx, double *fovy)
{
    if (aspect < 1) {
        *fovx = fov;
        *fovy = 2 * atan(tan(fov / 2) / aspect);
    } else {
        *fovy = fov;
        *fovx = 2 * atan(tan(fov / 2) * aspect);
    }
}

void proj_perspective_init(projection_t *p, double fovy, double aspect)
{
    const double clip_near = 5 * DM2AU;
    mat4_inf_perspective(p->mat, fovy * DR2D, aspect, clip_near);
}

static const projection_klass_t proj_perspective_klass = {
    .name           = "perspective",
    .id             = PROJ_PERSPECTIVE,
    .max_fov        = 180. * DD2R,
    .max_ui_fov     = 120. * DD2R,
    .init           = proj_perspective_init,
    .project        = proj_perspective_project,
    .backward       = proj_perspective_backward,
    .compute_fovs   = proj_perspective_compute_fov,
};
PROJECTION_REGISTER(proj_perspective_klass);
