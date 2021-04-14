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
        bool            visible;
    } tiles[12];
    fader_t         visible;
    double          turbidity;
} atmosphere_t;

// All the precomputed data
typedef struct {
    double sun_pos[3];
    double moon_pos[3];

    // Precomputed factors for A. J. Preetham model.
    double Px[5];
    double Py[5];
    double kx, ky;

    // Skybrightness model.
    skybrightness_t skybrightness;
    double eclipse_factor; // Solar eclipse adjustment.
    double landscape_lum; // Average luminance of the landscape.

    double light_pollution_lum;

    // Updated during rendering.
    double sum_lum;
    double max_lum;
    int    nb_lum;

    // Cos Maximum distance between 2 points of the grid on which the atmosphere
    // is rendered. It is used to avoid aliasing in fast varying regions of the
    // atmosphere, like near moon border.
    float cos_grid_angular_step;
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
        double T, double bortle_index)
{
    render_data_t data = {};
    double thetaS;
    double zx, zy;
    const double base_sun_vmag = -26.74;

    assert(vec3_is_normalized(sun_pos));
    assert(vec3_is_normalized(moon_pos));
    thetaS = acos(sun_pos[2]); // Zenith angle

    double t2 = thetaS * thetaS;
    double t3 = thetaS * thetaS * thetaS;
    double T2 = T * T;

    zx =
        ( 0.00166 * t3 - 0.00375 * t2 + 0.00209 * thetaS) * T2 +
        (-0.02903 * t3 + 0.06377 * t2 - 0.03202 * thetaS + 0.00394) * T +
        ( 0.11693 * t3 - 0.21196 * t2 + 0.06052 * thetaS + 0.25886);

    zy =
        ( 0.00275 * t3 - 0.00610 * t2 + 0.00317 * thetaS) * T2 +
        (-0.04214 * t3 + 0.08970 * t2 - 0.04153 * thetaS + 0.00516) * T +
        ( 0.15346 * t3 - 0.26756 * t2 + 0.06670 * thetaS + 0.26688);


    data.Px[0] =   -0.01925 * T  - 0.25922;
    data.Px[1] =   -0.06651 * T  + 0.00081;
    data.Px[2] =   -0.00041 * T  + 0.21247;
    data.Px[3] =   -0.06409 * T  - 0.89887;
    data.Px[4] =   -0.00325 * T  + 0.04517;

    data.Py[0] =   -0.01669 * T  - 0.26078;
    data.Py[1] =   -0.09495 * T  + 0.00921;
    data.Py[2] =   -0.00792 * T  + 0.21023;
    data.Py[3] =   -0.04405 * T  - 1.65369;
    data.Py[4] =   -0.01092 * T  + 0.05291;

    data.kx = zx / F(data.Px, 0, thetaS);
    data.ky = zy / F(data.Py, 0, thetaS);

    vec3_copy(sun_pos, data.sun_pos);
    vec3_copy(moon_pos, data.moon_pos);

    // Ad-hoc formula to estimate the landscape luminance.
    // From 0 to 5kcd/m².
    data.landscape_lum = smoothstep(0, 0.5, sun_pos[2]) * 5000;

    // Compute factor due to solar eclipse.
    // I am using an ad-hoc formula to make it look OK here.
    data.eclipse_factor = pow(10, (base_sun_vmag - sun_vmag) / 2.512 * 1.1);

    data.light_pollution_lum = max(0., 0.0004 * pow(bortle_index - 1, 2.1));
    return data;
}

// Convert MJD UTC to proleptic Gergorian Calendar.
static void mjd2gcal(double mjd, int *year, int *month)
{
    // Algo based on 'Date Algorithms'
    // By Peter Baum, 2017.
    double z, g, a, b, c;
    #define INT(x) ((int)floor(x))
    #define FIX(x) ((int)(x))
    z = INT(mjd + 678882.0);
    g = z - 0.25;
    a = INT(g / 36524.25);
    b = a - INT(a / 4.0);
    *year = INT((b + g) / 365.25);
    c = b + z - INT(365.25 * (*year));
    *month = FIX((5 * c + 456) / 153.0);
    if (*month > 12) {
        *year += 1;
        *month -= 12;
    }
    #undef INT
    #undef FIX
}

static void prepare_skybrightness(
        skybrightness_t *sb, const painter_t *painter,
        const double sun_pos[3], const double moon_pos[3], double moon_vmag)
{
    int year, month;
    const observer_t *obs = painter->obs;
    const double zenith[3] = {0, 0, 1};
    mjd2gcal(obs->utc, &year, &month);
    skybrightness_prepare(sb, year, month,
                          moon_vmag,
                          obs->phi, obs->hm,
                          15, 40,
                          eraSepp(moon_pos, zenith),
                          eraSepp(sun_pos, zenith));
}

static float compute_lum(void *user, const float pos[3])
{
    render_data_t *d = user;
    double p[3] = {pos[0], pos[1], pos[2]};
    const double zenith[3] = {0, 0, 1};
    float lum;
    // Our formula does not work below the horizon.
    p[2] = fabs(p[2]);
    lum = skybrightness_get_luminance(&d->skybrightness,
                min(vec3_dot(p, d->moon_pos), d->cos_grid_angular_step),
                min(vec3_dot(p, d->sun_pos), d->cos_grid_angular_step),
                vec3_dot(p, zenith));
    lum *= d->eclipse_factor;

    lum += d->light_pollution_lum;

    // Update luminance sum for eye adaptation.
    // If we are below horizon use the precomputed landscape luminance.
    if (pos[2] > 0) {
        d->sum_lum += lum;
        d->nb_lum++;
        d->max_lum = max(d->max_lum, lum);
    }
    else {
        d->max_lum = max(d->max_lum, d->landscape_lum);
    }
    return lum;
}

static int atmosphere_update(obj_t *obj, double dt)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    return fader_update(&atm->visible, dt);
}

static void render_tile(atmosphere_t *atm, const painter_t *painter,
                        int order, int pix)
{
    int split, i;
    uv_map_t map;

    if (painter_is_healpix_clipped(painter, FRAME_OBSERVED, order, pix))
        return;
    if (order < 1) {
        for (i = 0; i < 4; i++)
            render_tile(atm, painter, order + 1, pix * 4 + i);
        return;
    }
    split = 4; // Adhoc split value to look good while not being too slow.
    uv_map_init_healpix(&map, order, pix, true, true);
    paint_quad(painter, FRAME_OBSERVED, &map, split);
}

static int atmosphere_render(const obj_t *obj, const painter_t *painter_)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    obj_t *sun, *moon;
    double sun_pos[4], moon_pos[4], sun_vmag, moon_vmag;
    render_data_t data;
    int i;
    painter_t painter = *painter_;
    core->lwsky_average = 0.0001;
    observer_t *obs = painter.obs;

    if (atm->visible.value == 0.0) return 0;
    sun = core_get_planet(PLANET_SUN);
    moon = core_get_planet(PLANET_MOON);
    assert(sun);
    assert(moon);
    obj_get_pos(sun, painter.obs, FRAME_OBSERVED, sun_pos);
    obj_get_pos(moon, painter.obs, FRAME_OBSERVED, moon_pos);
    vec3_normalize(sun_pos, sun_pos);
    vec3_normalize(moon_pos, moon_pos);
    obj_get_info(sun, obs, INFO_VMAG, &sun_vmag);
    obj_get_info(moon, obs, INFO_VMAG, &moon_vmag);

    // XXX: this could be cached!
    data = prepare_render_data(sun_pos, sun_vmag, moon_pos, moon_vmag,
                               atm->turbidity, core->bortle_index);
    // This is quite ad-hoc as in reality we are using a HIPS grid
    data.cos_grid_angular_step = cos(15. * DD2R);
    prepare_skybrightness(&data.skybrightness,
            &painter, sun_pos, moon_pos, moon_vmag);

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

    data.max_lum = 0;
    for (i = 0; i < 12; i++) {
        render_tile(atm, &painter, 0, i);
    }

    core_report_luminance_in_fov(data.max_lum, true);
    if (data.nb_lum)
        core->lwsky_average = data.sum_lum / data.nb_lum;
    return 0;
}

static int atmosphere_init(obj_t *obj, json_value *args)
{
    atmosphere_t *atm = (void*)obj;
    atm->turbidity = 0.96;  // Calibrated visually
    fader_init(&atm->visible, true);
    return 0;
}

static void atmosphere_gui(obj_t *obj, int location)
{
    atmosphere_t *atm = (void*)obj;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 1) { // debug.
        gui_double_log("atm turbidity", &atm->turbidity, 0.1, 10, 1, NAN);
    }
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
    .gui    = atmosphere_gui,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(atmosphere_t, visible.target)),
        PROPERTY(turbidity, TYPE_FLOAT, MEMBER(atmosphere_t, turbidity)),
        {}
    },
};
OBJ_REGISTER(atmosphere_klass)
