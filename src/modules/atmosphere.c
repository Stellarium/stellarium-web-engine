/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

/*
 * This is all based on the paper: "A Practical Analytic Model for Daylight" by
 * A. J. Preetham, Peter Shirley and Brian Smits.
 *
 */

static const int TEX_SIZE = 64;

/*
 * Type: atmosphere_t
 * Atmosphere module struct.
 */
typedef struct atmosphere {
    obj_t           obj;
    // The twelves tile textures of healpix at order 0.
    texture_t       *tiles[12];
    fader_t         visible;

    // Used to prevent recomputing if no change.
    struct {
        double sun_pos[3];
    } last_state;
} atmosphere_t;

// All the precomputed data
typedef struct {
    double Px[5];
    double Py[5];
    double PY[5];
    double kx, ky, kY;
    double ambient_lum; // Constant added to the color lum.
    double lum_factor;  // For solar eclipse adjustement.
} render_data_t;

static double F2(const double *lam, double cos_theta,
                 double gamma, double cos_gamma)
{
    return (1 + lam[0] * exp(lam[1] / cos_theta)) *
           (1 + lam[2] * exp(lam[3] * gamma) + lam[4] * cos_gamma * cos_gamma);
}

static double F(const double *lam , double theta, double gamma)
{
    return F2(lam, cos(theta), gamma, cos(gamma));
}

static render_data_t prepare_render_data(
        const double sun_pos[3], double sun_vmag,
        const double moon_pos[3], double moon_vmag,
        double T)
{
    render_data_t data;
    double thetaS;
    double X;
    double zx, zy, zY;
    const double base_sun_vmag = -26.74;
    const double base_moon_vmag = -12.90;

    thetaS = acos(sun_pos[2]); // Zenith angle
    X = (4.0 / 9.0 - T / 120) * (M_PI - 2 * thetaS);
    zY = (4.0453 * T - 4.9710) * tan(X) - 0.2155 * T + 2.4192;

    double t2 = thetaS * thetaS;
    double t3 = thetaS * thetaS * thetaS;
    double T2 = T * T;

    zx =
        ( 0.00216 * t3 - 0.00375 * t2 + 0.00209 * thetaS) * T2 +
        (-0.02903 * t3 + 0.06377 * t2 - 0.03202 * thetaS + 0.00394) * T +
        ( 0.10169 * t3 - 0.21196 * t2 + 0.06052 * thetaS + 0.25886);

    zy =
        ( 0.00275 * t3 - 0.00610 * t2 + 0.00317 * thetaS) * T2 +
        (-0.04214 * t3 + 0.08970 * t2 - 0.04153 * thetaS + 0.00516) * T +
        ( 0.14535 * t3 - 0.26756 * t2 + 0.06670 * thetaS + 0.26688);

    data.PY[0] =    0.17872 *T  - 1.46303;
    data.PY[1] =   -0.35540 *T  + 0.42749;
    data.PY[2] =   -0.02266 *T  + 5.32505;
    data.PY[3] =    0.12064 *T  - 2.57705;
    data.PY[4] =   -0.06696 *T  + 0.37027;

    data.Px[0] =   -0.01925 *T  - 0.25922;
    data.Px[1] =   -0.06651 *T  + 0.00081;
    data.Px[2] =   -0.00041 *T  + 0.21247;
    data.Px[3] =   -0.06409 *T  - 0.89887;
    data.Px[4] =   -0.00325 *T  + 0.04517;

    data.Py[0] =   -0.01669 *T  - 0.26078;
    data.Py[1] =   -0.09495 *T  + 0.00921;
    data.Py[2] =   -0.00792 *T  + 0.21023;
    data.Py[3] =   -0.04405 *T  - 1.65369;
    data.Py[4] =   -0.01092 *T  + 0.05291;

    data.kx = zx / F(data.Px, 0, thetaS);
    data.ky = zy / F(data.Py, 0, thetaS);
    // XXX: I added the /30 to make the sky look OK, I don't understand why
    // I need that.
    data.kY = zY / F(data.PY, 0, thetaS) / 30;
    data.ambient_lum = 0.005;
    // Compute factor due to solar eclipse.
    // I am using an ad-hoc formula to make it look OK here.
    data.lum_factor = pow(2.512 * 0.48, base_sun_vmag - sun_vmag);

    // Add ambient light from the moon.  Ad-hoc formula!
    data.ambient_lum += 0.02 * smoothstep(0, 0.5, moon_pos[2]) *
                                pow(4.0, base_moon_vmag - moon_vmag);

    return data;
}

static void compute_point_color(const render_data_t *d,
                                const double pos[3],
                                const double sun_pos[3],
                                double T,
                                double color[3])
{
    double cos_theta, gamma, cos_gamma, s;
    double p[3] = {pos[0], pos[1], pos[2]};

    p[2] = max(p[2], 0); // Our formula does not work below the horizon.
    cos_theta  = p[2];
    cos_gamma = vec3_dot(p, sun_pos);
    gamma = acos(cos_gamma);

    color[0] = F2(d->Px, cos_theta, gamma, cos_gamma) * d->kx;
    color[1] = F2(d->Py, cos_theta, gamma, cos_gamma) * d->ky;
    color[2] = F2(d->PY, cos_theta, gamma, cos_gamma) * d->kY;
    color[2] = max(color[2], 0.0) * d->lum_factor + d->ambient_lum;
    // Scotopic vision adjustment with blue shift (xy = 0.25, 0.25)
    // Algo inspired from Stellarium.
    if (color[2] < 3.9) {
        // s: ratio between scotopic and photopic vision.
        s = smoothstep(0, 1, (log(color[2]) / log(10) + 2.) / 2.6);
        color[0] = mix(0.25, color[0], s);
        color[1] = mix(0.25, color[1], s);
    }
    xyY_to_rgb(color, color);
}

static int atmosphere_update(obj_t *obj, const observer_t *obs, double dt)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    return fader_update(&atm->visible, dt);
}

static int render_visitor(int order, int pix, void *user)
{
    assert(order == 0); // We only do level 0.
    atmosphere_t *atm = USER_GET(user, 0);
    painter_t painter = *(painter_t*)USER_GET(user, 1);
    const double *sun_pos = USER_GET(user, 2);
    const render_data_t *data = USER_GET(user, 3);
    uint8_t *tex_data;
    double pos[4], color[3];
    projection_t proj;
    double uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    int i, j, split;
    const double T = 5.0;

    painter.color[3] = atm->visible.value;
    projection_init_healpix(&proj, 1 << order, pix, true, true);

    if (vec3_equal(sun_pos, atm->last_state.sun_pos)) goto no_change;

    // Compute the atmosphere texture.
    tex_data = malloc(TEX_SIZE * TEX_SIZE * 3);
    for (i = 0; i < TEX_SIZE; i++)
    for (j = 0; j < TEX_SIZE; j++) {
        vec2_set(pos, j / (TEX_SIZE - 1.0), i / (TEX_SIZE - 1.0));
        project(&proj, PROJ_BACKWARD, 4, pos, pos);
        compute_point_color(data, pos, sun_pos, T, color);
        tex_data[(i * TEX_SIZE + j) * 3 + 0] = clamp(color[0], 0, 1) * 255;
        tex_data[(i * TEX_SIZE + j) * 3 + 1] = clamp(color[1], 0, 1) * 255;
        tex_data[(i * TEX_SIZE + j) * 3 + 2] = clamp(color[2], 0, 1) * 255;
    }
    if (!atm->tiles[pix]) {
        atm->tiles[pix] = texture_create(TEX_SIZE, TEX_SIZE, 3);
        // Add one pixel border to prevent error in the interpolation on the
        // side.
        atm->tiles[pix]->border = 1;
    }
    texture_set_data(atm->tiles[pix], tex_data, TEX_SIZE, TEX_SIZE, 3);
    free(tex_data);

no_change:
    painter.flags |= PAINTER_ADD;
    split = max(4, 12 >> order); // Adhoc split computation.
    paint_quad(&painter, FRAME_OBSERVED, atm->tiles[pix], NULL,
               uv, &proj, split);
    return 0;
}

static int atmosphere_render(const obj_t *obj, const painter_t *painter)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    obj_t *sun, *moon;
    double sun_pos[4], moon_pos[4], vmag, theta;
    render_data_t data;
    const double T = 5.0;
    const double base_sun_vmag = -26.74;

    if (atm->visible.value == 0.0) return 0;
    sun = obj_get_by_oid(&core->obj, oid_create("HORI", 10), 0);
    moon = obj_get_by_oid(&core->obj, oid_create("HORI", 301), 0);
    assert(sun);
    assert(moon);
    obj_get_pos_observed(sun, painter->obs, sun_pos);
    obj_get_pos_observed(moon, painter->obs, moon_pos);
    vec3_normalize(sun_pos, sun_pos);
    vec3_normalize(moon_pos, moon_pos);
    // XXX: this could be cached!
    data = prepare_render_data(sun_pos, sun->vmag, moon_pos, moon->vmag, T);
    hips_traverse(USER_PASS(atm, painter, sun_pos, &data), render_visitor);
    vec3_copy(sun_pos, atm->last_state.sun_pos);

    // XXX: ad-hoc formula to get the visible vmag.  Need to implement
    // something better.
    theta = acos(sun_pos[2]) - 0.83 * DD2R; // Zenith angle of top of Sun
    vmag = mix(-13 + (sun->vmag - base_sun_vmag) * 0.49, 0,
               smoothstep(85 * DD2R, 95 * DD2R, theta));
    core_report_vmag_in_fov(vmag, 360 * DD2R, 0);

    return 0;
}

static int atmosphere_init(obj_t *obj, json_value *args)
{
    atmosphere_t *atm = (void*)obj;
    fader_init(&atm->visible, true);
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t atmosphere_klass = {
    .id     = "atmosphere",
    .size   = sizeof(atmosphere_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = atmosphere_init,
    .render = atmosphere_render,
    .update = atmosphere_update,
    .render_order = 35,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(atmosphere_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(atmosphere_klass)
