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
void proj_hammer_init(projection_t *p, double fov, double aspect);
void proj_mollweide_init(projection_t *p, double fov, double aspect);
void proj_mollweide_adaptive_init(projection_t *p, double fovx, double aspect);

void proj_stereographic_compute_fov(double fov, double aspect,
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
    case PROJ_MOLLWEIDE:
    case PROJ_MOLLWEIDE_ADAPTIVE:
        proj_mollweide_compute_fov(fov, aspect, fovx, fovy);
        break;
    case PROJ_PERSPECTIVE:
        if (aspect < 1) {
            *fovx = fov;
            *fovy = 2 * atan(tan(fov / 2) / aspect);
        } else {
            *fovy = fov;
            *fovx = 2 * atan(tan(fov / 2) * aspect);
        }
        break;
    default:
        // To remove?
        if (aspect < 1) {
            *fovx = fov;
            *fovy = fov / aspect;
        } else {
            *fovy = fov;
            *fovx = fov * aspect;
        }
    }
}

void projection_init(projection_t *p, int type, double fov,
                     double w, double h)
{
    double aspect = w / h;
    memset(p, 0, sizeof(*p));
    p->window_size[0] = w;
    p->window_size[1] = h;
    p->fovx = fov;
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
        case PROJ_MOLLWEIDE_ADAPTIVE:
            proj_mollweide_adaptive_init(p, fov, aspect);
            break;
        default:
            assert(false);
    }
}

bool project(const projection_t *proj, int flags,
             const double v[static 4],
             double out[static 4])
{
    double p[4] = {0, 0, 0, 1};
    bool visible;

    if (flags & PROJ_BACKWARD) {
        vec2_copy(v, p);
        if (flags & PROJ_FROM_WINDOW_SPACE) {
            p[0] = p[0] / proj->window_size[0] * 2 - 1;
            p[1] = 1 - p[1] / proj->window_size[1] * 2;
        }
        if (proj->flags & PROJ_FLIP_HORIZONTAL) p[0] = -p[0];
        if (proj->flags & PROJ_FLIP_VERTICAL)   p[1] = -p[1];
        assert(proj->backward);
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
        memcpy(out, p, 4 * sizeof(double));
        return true;
    }
    assert(!(proj->flags & PROJ_NO_CLIP));
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
