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

static const int TEX_SIZE = 64;

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

static double compute_point_color(const render_data_t *d,
                                  const double pos[3],
                                  const double sun_pos[3],
                                  const double moon_pos[3],
                                  double T,
                                  double color[3])
{
    double cos_theta, gamma, cos_gamma, s, lum, v;
    double p[3] = {pos[0], pos[1], pos[2]};
    const double zenith[3] = {0, 0, 1};

    // Our formula does not work below the horizon.
    p[2] = fabs(p[2]);
    cos_theta  = p[2];
    cos_gamma = vec3_dot(p, sun_pos);
    gamma = acos(cos_gamma);

    lum = skybrightness_get_luminance(&d->skybrightness,
            eraSepp(p, moon_pos),
            eraSepp(p, sun_pos),
            eraSepp(p, zenith));
    assert(lum > 0);

    if (!color) return lum;
    color[0] = F2(d->Px, cos_theta, gamma, cos_gamma) * d->kx;
    color[1] = F2(d->Py, cos_theta, gamma, cos_gamma) * d->ky;
    color[2] = lum;

    // Scotopic vision adjustment with blue shift (xy = 0.25, 0.25)
    // Algo inspired from Stellarium.
    if (color[2] < 3.9) {
        // s: ratio between scotopic and photopic vision.
        s = smoothstep(0, 1, (log(color[2]) / log(10) + 2.) / 2.6);
        color[0] = mix(0.25, color[0], s);
        color[1] = mix(0.25, color[1], s);
        v = color[2] * (1.33 * (1. + color[1] / color[0] + color[0] *
                    (1. - color[0] - color[1])) - 1.68);
        color[2] = 0.4468 * (1. - s) * v + s * color[2];
    }

    return lum;
}

static int atmosphere_update(obj_t *obj, const observer_t *obs, double dt)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    return fader_update(&atm->visible, dt);
}

static bool is_tile_clipped(const painter_t *painter, int order, int pix)
{
    int neighbours[8], i;
    if (!painter_is_tile_clipped(painter, FRAME_OBSERVED, order, pix, true))
        return false;
    // Also check the neighbours tiles.
    healpix_get_neighbours(1 << order, pix, neighbours);
    for (i = 0; i < 8; i++) {
        if (neighbours[i] == -1) continue;
        if (!painter_is_tile_clipped(
                    painter, FRAME_OBSERVED, order, neighbours[i], true))
        {
            return false;
        }
    }
    return true;
}

/*
 * Update part of an order zero tile texture data.
 *
 * Parameters:
 *   size       - size of the buffer (pixel).
 *   border     - border of the buffer (pixel).
 *   order      - order of the part to be filled.
 *   pix        - pix of the part to be filled.
 *   out_nb     - Number of pixel set.
 *   out_sum    - Sum of luminance (cd/m²).
 */
static void fill_texture(float (*buf)[3], int size, int border,
                         int order, int pix,
                         const painter_t *painter,
                         const render_data_t *data,
                         int *out_nb, double *out_sum)
{
    int i, j, ix, iy, f, nb, x, y;
    double pos[4], lum, color[3], sum, ndc_pos[4];
    projection_t proj;
    const double *sun_pos = data->sun_pos;
    const double *moon_pos = data->moon_pos;
    const double T = 5.0;
    int fsize = size - 2 * (border - 1); // Actual texture size minus border.
    // Size in pixel we are going to fill.
    int s = ceil((double)fsize / (1 << order));

    *out_nb = 0;
    *out_sum = 0;

    if (is_tile_clipped(painter, order, pix)) return;
    /* If the size of the rect to fill is high, we fill the children healpix
     * pixels, so we get a chance to skip the non visible child.  */
    if (s >= 8) goto go_down;

    healpix_nest2xyf(1 << order, pix, &ix, &iy, &f);
    projection_init_healpix(&proj, 1, f, true, true);

    for (y = 0; y < s; y++)
    for (x = 0; x < s; x++) {
        i = ix * s + x;
        j = iy * s + y;
        vec2_set(pos, j / (fsize - 1.0), i / (fsize - 1.0));
        project(&proj, PROJ_BACKWARD, 4, pos, pos);

        lum = compute_point_color(data, pos, sun_pos, moon_pos, T, color);
        i += border - 1;
        j += border - 1;
        // Prevent error if s doesn't divide size properly.
        if (i >= size || j >= size) continue;
        buf[(i * size + j)][0] = color[0];
        buf[(i * size + j)][1] = color[1];
        buf[(i * size + j)][2] = color[2];
        // If the pixel is probably not visible, don't add it to the lum.
        convert_direction(painter->obs, FRAME_OBSERVED, FRAME_VIEW, true,
                            pos, ndc_pos);
        if (project(painter->proj, PROJ_TO_NDC_SPACE, 4, ndc_pos, ndc_pos)) {
            *out_sum += lum;
            *out_nb += 1;
        }
    }
    return;

go_down:
    for (i = 0; i < 4; i++) {
        fill_texture(buf, size, border, order + 1, pix * 4 + i, painter, data,
                     &nb, &sum);
        *out_nb += nb;
        *out_sum += sum;
    }
    return;
}

/*
 * Update an order zero tile of the atmosphere.
 *
 * Parameters:
 *   nb     - Output number of pixel set.
 *   sum    - Output sum of luminance (cd/m²).
 */
static void prepare_tile(atmosphere_t *atm, const painter_t *painter,
                         const render_data_t *data, int pix,
                         int *nb, double *sum)
{
    projection_t proj;
    int size;
    typeof(atm->tiles[pix]) *tile = &atm->tiles[pix];

    // Compute the actual size we use in the texture depending on the fov.
    size = clamp(8 / painter->proj->scaling[0], 8, 64);
    tile->border = (TEX_SIZE - size) / 2;
    tile->border = tile->border / 2 * 2 + 1; // make sure border is odd.
    if (!tile->buf)
        tile->buf = malloc(TEX_SIZE * TEX_SIZE * sizeof(*tile->buf));
    projection_init_healpix(&proj, 1, pix, true, true);
    fill_texture(tile->buf, TEX_SIZE, tile->border, 0, pix, painter,
                 data, nb, sum);
    atm->tiles[pix].visible = (*nb) != 0;
}

static void render_tile(atmosphere_t *atm, const painter_t *painter_,
                        int pix, uint8_t *buf)
{
    int split, i, j;
    double uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    double color[3];
    projection_t proj;
    typeof(atm->tiles[pix]) *tile = &atm->tiles[pix];

    if (!tile->visible) return;
    if (!tile->tex)
        tile->tex = texture_create(TEX_SIZE, TEX_SIZE, 3);

    // Convert the tile data to RGB.
    // Could be dont in a shader.
    for (i = tile->border - 1; i < TEX_SIZE - tile->border + 1; i++)
    for (j = tile->border - 1; j < TEX_SIZE - tile->border + 1; j++) {
        color[0] = tile->buf[i * TEX_SIZE + j][0];
        color[1] = tile->buf[i * TEX_SIZE + j][1];
        color[2] = tile->buf[i * TEX_SIZE + j][2];
        color[2] = tonemapper_map(core->tonemapper, color[2]);
        xyY_to_srgb(color, color);
        buf[(i * TEX_SIZE + j) * 3 + 0] = clamp(color[0], 0, 1) * 255;
        buf[(i * TEX_SIZE + j) * 3 + 1] = clamp(color[1], 0, 1) * 255;
        buf[(i * TEX_SIZE + j) * 3 + 2] = clamp(color[2], 0, 1) * 255;
    }

    projection_init_healpix(&proj, 1, pix, true, true);
    texture_set_data(tile->tex, buf, TEX_SIZE, TEX_SIZE, 3);
    painter_t painter = *painter_;
    painter.color[3] = atm->visible.value;
    painter.flags |= PAINTER_ADD;
    split = 4; // Adhoc split computation.
    tile->tex->border = tile->border;
    paint_quad(&painter, FRAME_OBSERVED, tile->tex, NULL, uv, &proj, split);
}

static int atmosphere_render(const obj_t *obj, const painter_t *painter)
{
    atmosphere_t *atm = (atmosphere_t*)obj;
    obj_t *sun, *moon;
    double sun_pos[4], moon_pos[4], avg_lum, moon_phase;
    double sum, sum_lum = 0;
    render_data_t data;
    const double T = 5.0;
    int i, nb, nb_lum = 0;
    uint8_t *buf;

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
    obj_get_attr(moon, "phase", "f", &moon_phase);

    prepare_skybrightness(&data.skybrightness,
            painter, sun_pos, moon_pos, moon->vmag, moon_phase);

    for (i = 0; i < 12; i++) {
        prepare_tile(atm, painter, &data, i, &nb, &sum);
        nb_lum += nb;
        sum_lum += sum;
    }

    // Report luminance before we render the tiles, so that the tonemapping
    // has time to adjust.
    avg_lum = 0;
    if (nb_lum) avg_lum = sum_lum / nb_lum;
    core_report_luminance_in_fov(avg_lum + 0.001, true);

    buf = malloc(TEX_SIZE * TEX_SIZE * 3);
    for (i = 0; i < 12; i++) {
        render_tile(atm, painter, i, buf);
    }
    free(buf);
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
