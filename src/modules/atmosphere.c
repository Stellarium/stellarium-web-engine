/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "skybrightness.h"

/*
 * This is all based on the paper: "A Practical Analytic Model for Daylight" by
 * A. J. Preetham, Peter Shirley and Brian Smits.
 *
 */

/*
 * Type: atmosphere_t
 * Atmosphere module struct.
 */
typedef struct atmosphere {
    obj_t           obj;
    // The twelves tile textures of healpix at order 0 and construction bufs.
    struct {
        texture_t       *tex;
        float           (*buf)[3];  // color buffer (in xyY).
        int             border;
        bool            visible;
    } tiles[12];
    fader_t         visible;
} atmosphere_t;

// All the precomputed data
typedef struct {
    double sun_pos[3];
    double moon_pos[3];

    // Precomputed factors for A. J. Preetham model.
    double Px[5];
    double Py[5];
    double PY[5];
    double kx, ky, kY;

    // Skybrightness model.
    skybrightness_t skybrightness;

    // Updated during rendering.
    double sum_lum;
    int    nb_lum;
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
    render_data_t data = {};
    double thetaS;
    double X;
    double zx, zy, zY;

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
    data.kY = zY / F(data.PY, 0, thetaS);

    vec3_copy(sun_pos, data.sun_pos);
    vec3_copy(moon_pos, data.moon_pos);
    return data;
}

static void prepare_skybrightness(
        skybrightness_t *sb, const painter_t *painter,
        const double sun_pos[3], const double moon_pos[3], double moon_vmag,
        double moon_phase)
{
    int year, month, day, ihmsf[4];
    const observer_t *obs = painter->obs;
    const double zenith[3] = {0, 0, 1};
    // Convert moon phase to sun/moon/earth separation.
    moon_phase = acos(2 * (moon_phase - 0.5));
    eraD2dtf("UTC", 0, DJM0, obs->utc, &year, &month, &day, ihmsf);
    skybrightness_prepare(sb, year, month,
                          moon_phase,
                          obs->phi, obs->hm,
                          15, 40,
                          eraSepp(moon_pos, zenith),
                          eraSepp(sun_pos, zenith),
                          0.01);
}

static double compute_lum(void *user, const double pos[3])
{
    render_data_t *d = user;
    double p[3] = {pos[0], pos[1], pos[2]}, lum;
    const double zenith[3] = {0, 0, 1};
    // Our formula does not work below the horizon.
    p[2] = fabs(p[2]);
    lum = skybrightness_get_luminance(&d->skybrightness,
                eraSepp(p, d->moon_pos),
                eraSepp(p, d->sun_pos),
                eraSepp(p, zenith));
    // Clamp to prevent too much adaptation.
    lum = min(lum, 50000);

    d->sum_lum += lum;
    d->nb_lum++;
    return lum;
}

static int atmosphere_update(obj_t *obj, const observer_t *obs, double dt)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    return fader_update(&atm->visible, dt);
}

static void render_tile2(atmosphere_t *atm, const painter_t *painter,
                         int order, int pix)
{
    int split;
    double uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    if (painter_is_tile_clipped(painter, FRAME_OBSERVED, order, pix, true))
        return;
    split = 8; // Adhoc split computation.
    projection_t proj;
    projection_init_healpix(&proj, 1 << order, pix, true, true);
    paint_quad(painter, FRAME_OBSERVED, NULL, NULL, uv, &proj, split);
}

static int atmosphere_render(const obj_t *obj, const painter_t *painter_)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    obj_t *sun, *moon;
    double sun_pos[4], moon_pos[4], avg_lum, moon_phase;
    render_data_t data;
    const double T = 5.0;
    int i;
    painter_t painter = *painter_;

    if (atm->visible.value == 0.0) return 0;
    sun = obj_get_by_oid(&core->obj, oid_create("HORI", 10), 0);
    moon = obj_get_by_oid(&core->obj, oid_create("HORI", 301), 0);
    assert(sun);
    assert(moon);
    obj_get_pos_observed(sun, painter.obs, sun_pos);
    obj_get_pos_observed(moon, painter.obs, moon_pos);
    vec3_normalize(sun_pos, sun_pos);
    vec3_normalize(moon_pos, moon_pos);

    // XXX: this could be cached!
    data = prepare_render_data(sun_pos, sun->vmag, moon_pos, moon->vmag, T);
    obj_get_attr(moon, "phase", "f", &moon_phase);
    prepare_skybrightness(&data.skybrightness,
            &painter, sun_pos, moon_pos, moon->vmag, moon_phase);

    // Set the shader attributes.
    painter.atm.p[0]  = data.Px[0];
    painter.atm.p[1]  = data.Px[1];
    painter.atm.p[2]  = data.Px[2];
    painter.atm.p[3]  = data.Px[3];
    painter.atm.p[4]  = data.Px[4];
    painter.atm.p[5]  = data.kx;

    painter.atm.p[6]  = data.Py[0];
    painter.atm.p[7]  = data.Py[1];
    painter.atm.p[8]  = data.Py[2];
    painter.atm.p[9]  = data.Py[3];
    painter.atm.p[10] = data.Py[4];
    painter.atm.p[11] = data.ky;

    vec3_to_float(sun_pos, painter.atm.sun);
    painter.atm.compute_lum = compute_lum;
    painter.atm.user = &data;
    painter.flags |= PAINTER_ADD | PAINTER_ATMOSPHERE_SHADER;
    painter.color[3] = atm->visible.value;

    for (i = 0; i < 12; i++) {
        render_tile2(atm, &painter, 0, i);
    }

    avg_lum = 0;
    if (data.nb_lum) avg_lum = data.sum_lum / data.nb_lum;
    // Clamp the average luminance to prevent too much adaptation.
    // 8000 cd/m² represents a bright sky.
    avg_lum = min(avg_lum, 8000);
    core_report_luminance_in_fov(avg_lum + 0.001, true);
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
