/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "ini.h"
#include <regex.h>
#include <zlib.h> // For crc32.

/* Planets module.
 *
 * All the data is in the file data/planets.ini.
 */

typedef struct planet planet_t;

// The planet object klass.
struct planet {
    obj_t       obj;

    UT_hash_handle  hh;    // For the global map.
    const char  *name;
    planet_t    *parent;
    double      radius_m;  // (meter)
    double      albedo;
    double      color[4];
    double      shadow_brightness; // in [0-1].
    int         id; // Uniq id number, as defined in JPL HORIZONS.

    // Precomputed values.
    uint64_t    observer_hash;
    double      pvh[2][3];   // equ, J2000.0, AU heliocentric pos and speed.
    double      hpos[3];     // ecl, heliocentric pos J2000.0
    double      phase;
    double      radius;     // XXX ?
    double      mass;       // kg (0 if unknown).

    // Rotation elements
    struct {
        double obliquity;   // (rad)
        double period;      // (day)
        double offset;      // (rad)
    } rot;

    // Orbit elements.
    struct {
        int type; // 0: special, 1: kepler.
        union {
            struct {
                double jd;      // date (MJD).
                double in;      // inclination (rad).
                double om;      // Longitude of the Ascending Node (rad).
                double w;       // Argument of Perihelion (rad).
                double a;       // Mean distance (Semi major axis) (AU).
                double n;       // Daily motion (rad/day).
                double ec;      // Eccentricity.
                double ma;      // Mean Anomaly (rad).
            } kepler;
        };
    } orbit;

    // Rings attributes.
    struct {
        double inner_radius; // (meter)
        double outer_radius; // (meter)
        texture_t *tex;
    } rings;

    hips_t      *hips;              // Hips survey of the planet.
    hips_t      *hips_normalmap;    // Normal map survey.
};

// Planets layer object type;
typedef struct planets {
    obj_t       obj;
    planet_t    *planets; // The map of all the planets.
    planet_t    *sun;
    planet_t    *earth;

    // Earth shadow on a lunar eclipse.
    texture_t *earth_shadow_tex;
    // Sun halo.
    texture_t *halo_tex;
} planets_t;


static int planet_update_(planet_t *planet, const observer_t *obs);
static int planet_update(obj_t *obj, const observer_t *obs, double dt);
static planet_t *planet_get_by_name(planets_t *planets, const char *name);
static int planets_init(obj_t *obj, json_value *args);
static int planets_update(obj_t *obj, const observer_t *obs, double dt);
static int planets_render(const obj_t *obj, const painter_t *painter);
static obj_t *planets_get(const obj_t *obj, const char *id, int flags);

/*
 * List of known bodies id.
 * We use them to quickly test for a given planet.  They follow the ids
 * used by JPL HORIZONS service.
 */
enum {
    SUN = 10,
    MERCURY = 199,
    VENUS = 299,
    MOON = 301,
    EARTH = 399,
    MARS = 499,
    IO = 501,
    EUROPA = 502,
    GANYMEDE = 503,
    CALLISTO = 504,
    JUPITER = 599,
    SATURN = 699,
    URANUS = 799,
    NEPTUNE = 899,
};

/* visual elements of planets
 * [planet][0] = angular size at 1 AU
 * [planet][1] = magnitude at 1 AU from sun and earth and 0 deg phase angle
 * [planet][2] = A
 * [planet][3] = B
 * [planet][4] = C
 *   where mag correction = A*(i/100) + B*(i/100)^2 + C*(i/100)^3
 *      i = angle between sun and earth from planet, degrees
 * from Explanatory Supplement, 1992
 */
static const double VIS_ELEMENTS[][5] = {
        /*         */   {},
        /* Mercury */   {6.74, -0.36, 3.8, -2.73, 2.00},
        /* Venus */     {16.92, -4.29, 0.09, 2.39, -.65},
        /* Earth */     {},
        /* Mars */      {9.36, -1.52, 1.60, 0., 0.},
        /* Jupiter */   {196.74, -9.25, 0.50, 0., 0.},
        /* Saturn */    {165.6, -8.88, 4.40, 0., 0.},
        /* Uranus */    {70.481, -7.19, 0.28, 0., 0.},
        /* Neptune */   {68.294, -6.87, 0., 0., 0.},
        /* Pluto */     {8.2, -1.01, 4.1, 0., 0.},
};


#define PLANETS_ITER(o, p) \
    for (   p = (planet_t*)(((obj_t*)o)->children); \
            p; \
            p = (planet_t*)p->obj.next)

static int earth_update(planet_t *planet, const observer_t *obs)
{
    double pv[2][3];
    // Heliocentric position of the earth (AU)
    eraCpv(obs->earth_pvh, planet->pvh);
    eraZpv(pv);
    position_to_apparent(obs, ORIGIN_GEOCENTRIC, false, pv, pv);
    vec3_copy(pv[0], planet->obj.pvo[0]);
    vec3_copy(pv[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;
    // Ecliptic position.
    mat4_mul_vec3(obs->ri2e, planet->pvh[0], planet->hpos);
    planet->phase = NAN;
    return 0;
}

/*
 * Compute the illumination from the sun taking into possible eclipses.
 */
static double compute_sun_eclipse_factor(const planet_t *sun,
                                         const observer_t *obs)
{
    // For the moment we assume the observer is always on Earth!
    double sun_r;   // Sun radius as seen from observer.
    double sph_r;   // Blocking body radius as seen from observer.
    double sep;     // Separation angle between sun and body.
    planet_t *p;

    sun_r = 2.0 * sun->radius_m / DAU / vec3_norm(sun->obj.pvo[0]);

    PLANETS_ITER(sun->obj.parent, p) {
        if (p->id != MOON) continue; // Only consider the Moon.
        planet_update_(p, obs);
        sph_r = 2.0 * p->radius_m / DAU / vec3_norm(p->obj.pvo[0]);
        sep = eraSepp(sun->obj.pvo[0], p->obj.pvo[0]);
        // Compute shadow factor.
        // XXX: this should be in algos.
        if (sep >= sun_r + sph_r) return 1.0; // Outside of shadow.
        if (sep <= sph_r - sun_r) return 0.0; // Umbra.
        if (sep <= sun_r - sph_r) // Penumbra completely inside.
            return 1.0 - sph_r * sph_r / (sun_r * sun_r);
        // Penumbra partially inside.
        double x = (sun_r * sun_r + sep * sep - sph_r * sph_r) / (2.0 * sep);
        double alpha = acos(x / sun_r);
        double beta = acos((sep - x) / sph_r);
        double AR = sun_r * sun_r * (alpha - 0.5 * sin(2.0 * alpha));
        double Ar = sph_r * sph_r * (beta - 0.5 * sin(2.0 * beta));
        double AS = sun_r * sun_r * 2.0 * 1.57079633;
        return 1.0 - (AR + Ar) / AS;
    }
    return 1.0;
}


static int sun_update(planet_t *planet, const observer_t *obs)
{
    double eclipse_factor;
    double dist_pc; // Distance in parsec.
    eraZpv(planet->pvh);
    vec3_copy(obs->sun_pvo[0], planet->obj.pvo[0]);
    vec3_copy(obs->sun_pvo[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;
    planet->phase = NAN;

    // Compute the apparent magnitude for the absolute mag (V: 4.83) and
    // observer's distance
    dist_pc = vec3_norm(obs->earth_pvh[0]) * (M_PI / 648000);
    eclipse_factor = max(compute_sun_eclipse_factor(planet, obs), 0.000128);
    planet->obj.vmag = 4.83 + 5.0 * (log10(dist_pc) - 1.0) -
                       2.5 * (log10(eclipse_factor));
    return 0;
}

static int moon_update(planet_t *planet, const observer_t *obs)
{
    double i;   // Phase angle.
    double lambda, beta, dist;
    double rmatecl[3][3], rmatp[3][3];
    double obl;
    double pos[3], el;
    double pv[2][3];
    // Get ecliptic position of date.
    moon_pos(DJM0 + obs->tt, &lambda, &beta, &dist);
    dist *= 1000.0 / DAU; // km to AU.
    // Convert to equatorial.
    obl = eraObl06(DJM0, obs->tt); // Mean oblicity of ecliptic at J2000.
    eraIr(rmatecl);
    eraRx(-obl, rmatecl);
    eraS2p(lambda, beta, dist, pos);
    eraRxp(rmatecl, pos, pos);

    // Precess back to J2000.
    eraPmat76(DJM0, obs->tt, rmatp);
    eraTrxp(rmatp, pos, pos);

    eraCp(pos, pv[0]);
    // We don't know the speed, set to zero as moon (geocentric) speed is too
    // small for most effects anyway
    pv[1][0] = 0;
    pv[1][1] = 0;
    pv[1][2] = 0;

    // Compute heliocentric position.
    eraPvppv(pv, obs->earth_pvb, planet->pvh);
    eraPvmpv(planet->pvh, obs->sun_pvb, planet->pvh);

    // Compute apparent position
    position_to_apparent(obs, ORIGIN_GEOCENTRIC, false, pv, pv);
    vec3_copy(pv[0], planet->obj.pvo[0]);
    vec3_copy(pv[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;

    i = eraSepp(planet->pvh[0], planet->obj.pvo[0]);
    planet->phase = 0.5 * cos(i) + 0.5;

    // Compute visual mag.
    // This is based on the algo of pyephem.
    // XXX: move into 'algos'.
    el = eraSepp(planet->obj.pvo[0], obs->sun_pvo[0]); // Elongation.
    planet->obj.vmag = -12.7 +
        2.5 * (log10(M_PI) - log10(M_PI / 2.0 * (1.0 + 1.e-6 - cos(el)))) +
        5.0 * log10(dist / .0025);

    return 0;
}

static int plan94_update(planet_t *planet, const observer_t *obs)
{
    const double *vis;  // Visual element of planet.
    double rho; // Distance to Earth (AU).
    double rp;  // Distance to Sun (AU).
    double i;   // Phase angle.
    double pv[2][3];
    int n = (planet->id - MERCURY) / 100 + 1;
    eraPlan94(DJM0, obs->tt, n, planet->pvh);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, planet->pvh, pv);
    vec3_copy(pv[0], planet->obj.pvo[0]);
    vec3_copy(pv[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;

    i = eraSepp(planet->pvh[0], planet->obj.pvo[0]);
    planet->phase = 0.5 * cos(i) + 0.5;
    // Compute visual magnitude.
    i *= DR2D / 100;
    rho = vec3_norm(planet->pvh[0]);
    rp = vec3_norm(planet->obj.pvo[0]);
    vis = VIS_ELEMENTS[n];
    planet->obj.vmag = vis[1] + 5 * log10(rho * rp) +
                        i * (vis[2] + i * (vis[3] + i * vis[4]));
    return 0;
}

static int l12_update(planet_t *planet, const observer_t *obs)
{
    double pvj[2][3], pv[2][3];
    double mag;
    double rho; // Distance to Earth (AU).
    double rp;  // Distance to Sun (AU).
    planet_t *jupiter = planet->parent;
    planet_update_(jupiter, obs);
    l12(DJM0, obs->tt, planet->id - IO + 1, pvj);
    eraPvppv(pvj, jupiter->pvh, planet->pvh);
    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, planet->pvh, pv);
    vec3_copy(pv[0], planet->obj.pvo[0]);
    vec3_copy(pv[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;

    // Compute visual magnitude.
    // http://www.physics.sfasu.edu/astro/asteroids/sizemagnitude.html
    rho = vec3_norm(planet->pvh[0]);
    rp = vec3_norm(planet->obj.pvo[0]);
    mag = -1.0 / 0.2 * log10(sqrt(planet->albedo) *
            2.0 * planet->radius_m / 1000.0 / 1329.0);
    planet->obj.vmag = mag + 5 * log10(rho * rp);
    return 0;
}

static int kepler_update(planet_t *planet, const observer_t *obs)
{
    double rho; // Distance to Earth (AU).
    double rp;  // Distance to Sun (AU).
    double p[3], v[3], pv[2][3];
    planet_update_(planet->parent, obs);
    orbit_compute_pv(0, obs->tt, p, v,
            planet->orbit.kepler.jd,
            planet->orbit.kepler.in,
            planet->orbit.kepler.om,
            planet->orbit.kepler.w,
            planet->orbit.kepler.a,
            planet->orbit.kepler.n,
            planet->orbit.kepler.ec,
            planet->orbit.kepler.ma,
            0.0, 0.0);
    // Ecliptic -> Equatorial.
    double obl;
    double rmatecl[3][3];
    obl = eraObl06(DJM0, obs->tt); // Mean oblicity of ecliptic at J2000.
    eraIr(rmatecl);
    eraRx(-obl, rmatecl);
    eraRxp(rmatecl, p, p);
    eraRxp(rmatecl, v, v);

    // Add parent position and speed to get heliocentric position.
    vec3_add(p, planet->parent->pvh[0], planet->pvh[0]);
    vec3_add(v, planet->parent->pvh[1], planet->pvh[1]);

    position_to_apparent(obs, ORIGIN_HELIOCENTRIC, false, planet->pvh, pv);
    vec3_copy(pv[0], planet->obj.pvo[0]);
    vec3_copy(pv[1], planet->obj.pvo[1]);
    planet->obj.pvo[0][3] = 1;
    planet->obj.pvo[1][3] = 1;

    // Compute visual magnitude.
    // http://www.physics.sfasu.edu/astro/asteroids/sizemagnitude.html
    rho = vec3_norm(planet->pvh[0]);
    rp = vec3_norm(planet->obj.pvo[0]);
    double mag = -1.0 / 0.2 * log10(sqrt(planet->albedo) *
            2.0 * planet->radius_m / 1000.0 / 1329.0);
    planet->obj.vmag = mag + 5 * log10(rho * rp);
    return 0;
}


static int planet_update_(planet_t *planet, const observer_t *obs)
{
    // Skip if already up to date.
    if (planet->observer_hash == obs->hash) return 0;

    // Compute the position of the planet.
    // XXX: we could use an approximation at the beginning, and only compute
    // the exact pos if needed.
    if (planet->id == EARTH) earth_update(planet, obs);
    if (planet->id == SUN) sun_update(planet, obs);
    if (planet->id == MOON) moon_update(planet, obs);

    // Solar system planet: use PLAN94 model.
    if (    planet->id == MERCURY ||
            planet->id == VENUS ||
            planet->id == MARS ||
            planet->id == JUPITER ||
            planet->id == SATURN ||
            planet->id == URANUS ||
            planet->id == NEPTUNE)
    {
        plan94_update(planet, obs);
    }

    // Galilean moons: Use L12.
    if (    planet->id == IO ||
            planet->id == EUROPA ||
            planet->id == GANYMEDE ||
            planet->id == CALLISTO)
    {
        l12_update(planet, obs);
    }

    // Kepler orbit planets.
    if (planet->orbit.type == 1) kepler_update(planet, obs);

    mat4_mul_vec3(obs->ri2e, planet->pvh[0], planet->hpos);

    // Adjust vmag for saturn.
    if (planet->id == SATURN) {
        double hlon, hlat;
        double earth_hlon, earth_hlat;
        double et, st, set;
        double earth_hpos[3];
        mat4_mul_vec3(obs->ri2e, obs->earth_pvh[0], earth_hpos);

        eraC2s(planet->hpos, &hlon, &hlat);
        eraC2s(earth_hpos, &earth_hlon, &earth_hlat);
        satrings(hlat, hlon, vec3_norm(planet->pvh[0]),
                 earth_hlon, vec3_norm(obs->earth_pvh[0]),
                 obs->ut1 + DJM0, &et, &st);
        set = sin(fabs(et));
        planet->obj.vmag += (-2.60 + 1.25 * set) * set;
    }

    planet->radius = planet->radius_m / DAU /
            eraPm((double*)planet->obj.pvo[0]);

    return 0;
}

static int planet_update(obj_t *obj, const observer_t *obs, double dt)
{
    planet_t *planet = (planet_t*)obj;
    planet_update_(planet, obs);
    assert(obj->pvo[0][3] == 1.0);
    return 0;
}

static int on_render_tile(hips_t *hips, const painter_t *painter_,
                          int order, int pix, int flags, void *user)
{
    planet_t *planet = USER_GET(user, 0);
    int *nb_tot = USER_GET(user, 1);
    int *nb_loaded = USER_GET(user, 2);
    painter_t painter = *painter_;
    texture_t *tex, *normalmap = NULL;
    projection_t proj;
    int split;
    double fade, uv[4][2];
    bool loaded;

    (*nb_tot)++;
    tex = hips_get_tile_texture(hips, order, pix, flags,
                                uv, &proj, &split, &fade, &loaded);
    if (loaded) (*nb_loaded)++;
    if (planet->hips_normalmap && order > 0) {
        // XXX: need to check if the UVs don't match.
        (*nb_tot)++;
        normalmap = hips_get_tile_texture(planet->hips_normalmap,
                order, pix, false, NULL, NULL, NULL, NULL, &loaded);
        if (loaded) (*nb_loaded)++;
    }

    // Texture not ready yet, we just use the planet color.
    if (!tex) {
        vec3_copy(planet->color, painter.color);
        painter.color[3] = 1;
    }
    painter.color[3] *= fade;
    if (planet->id == MOON) painter.flags |= PAINTER_IS_MOON;

    // Hardcoded increase of the luminosity of the moon for the moment!
    // This should be specified in the survey itsefl I guess.
    if (planet->id == MOON)
        vec3_mul(1.8, painter.color, painter.color);

    paint_quad(&painter, FRAME_ICRF, tex, normalmap, uv, &proj, split);
    return 0;
}

static void ring_project(const projection_t *proj, int flags,
                           const double *v, double *out)
{
    const double *radii = proj->user;
    double theta, r, mat[3][3], p[4] = {1, 0, 0, 1};
    theta = v[0] * 2 * M_PI;
    r = mix(radii[0], radii[1], v[1]);
    mat3_set_identity(mat);
    mat3_rz(theta, mat, mat);
    mat3_iscale(mat, r, r, 1.0);
    mat3_mul_vec3(mat, p, p);
    vec4_copy(p, out);
}

static void render_rings(const planet_t *planet,
                         const painter_t *painter_)
{
    texture_t *tex = planet->rings.tex;
    double inner_radius = planet->rings.inner_radius / planet->radius_m;
    double outer_radius = planet->rings.outer_radius / planet->radius_m;
    projection_t proj = {
        .backward   = ring_project,
    };
    painter_t painter = *painter_;
    const double radii[2] = {inner_radius, outer_radius};

    // Add the planet in the painter shadow candidates.
    if (painter.shadow_spheres_nb < 4) {
        vec3_copy(planet->obj.pvo[0],
                  painter.shadow_spheres[painter.shadow_spheres_nb]);
        painter.shadow_spheres[painter.shadow_spheres_nb][3] =
                  planet->radius_m / DAU;
        painter.shadow_spheres_nb++;
    }

    proj.user = radii;
    painter.light_emit = NULL;
    painter.flags &= ~PAINTER_PLANET_SHADER;
    painter.flags |= PAINTER_RING_SHADER;
    paint_quad(&painter, FRAME_ICRF, tex, NULL, NULL, &proj, 64);
}

/*
 * Test if a planet A could cast shadow on a planet B.
 * if a is NULL, then we return false if we know for sure that no body
 * could cast a shadow on b.
 */
static bool could_cast_shadow(const planet_t *a, const planet_t *b)
{
    // Not sure about this algo, I took it pretty as it is from Stellarium.
    const double SUN_RADIUS = 695508000.0 / DAU;
    double pp[3];
    double shadow_dist, d, penumbra_r;

    // For the moment we only consider the Jupiter major moons or the
    // Earth on the Moon.
    if (a == NULL)
        return b->id == MOON || (b->id >= IO && b->id <= JUPITER);
    if (b->id == a->id) return false; // No self shadow.
    if ((b->id >= IO && b->id <= JUPITER) && (a->id < IO || a->id > JUPITER))
            return false;
    if (b->id == MOON && a->id != EARTH) return false;

    if (vec3_norm2(a->pvh[0]) > vec3_norm2(b->pvh[0])) return false;
    vec3_normalize(a->pvh[0], pp);
    shadow_dist = vec3_dot(pp, b->pvh[0]);
    d = vec2_norm(a->pvh[0]) / (a->radius_m / DAU / SUN_RADIUS + 1.0);
    penumbra_r = (shadow_dist - d) / d * SUN_RADIUS;
    vec3_mul(shadow_dist, pp, pp);
    vec3_sub(pp, b->pvh[0], pp);
    return (vec3_norm(pp) < penumbra_r + b->radius_m / DAU);
}

/*
 * Compute the list of potential shadow spheres that should be considered
 * when rendering a planet.
 *
 * The returned spheres are xyz = position (in view frame) and w = radius (AU).
 * Return the number of candidates.
 */
static int get_shadow_candidates(const planet_t *planet, int nb_max,
                                 double (*spheres)[4])
{
    int nb = 0;
    planets_t *planets = (planets_t*)planet->obj.parent;
    planet_t *other;

    if (!could_cast_shadow(NULL, planet)) return 0;

    PLANETS_ITER(planets, other) {
        if (could_cast_shadow(other, planet)) {
            if (nb >= nb_max) break;
            vec3_copy(other->obj.pvo[0], spheres[nb]);
            spheres[nb][3] = other->radius_m / DAU;
            nb++;
        }
    }
    return nb;
}

static void planet_render_hips(const planet_t *planet,
                               double radius,
                               double alpha,
                               const painter_t *painter_)
{
    // XXX: cleanup this function.  It is getting too big.
    double mat[4][4];
    double pos[4];
    double dist;
    double full_emit[3] = {1.0, 1.0, 1.0};
    double rot;
    double angle = 2 * radius / vec2_norm(planet->obj.pvo[0]);
    int nb_tot = 0, nb_loaded = 0;
    double sun_pos[4] = {0, 0, 0, 1};
    planets_t *planets = (planets_t*)planet->obj.parent;
    painter_t painter = *painter_;
    double depth_range[2];
    double shadow_spheres[4][4];
    double epoch = DJM00; // J2000.

    painter.shadow_spheres_nb =
        get_shadow_candidates(planet, 4, shadow_spheres);
    painter.shadow_spheres = shadow_spheres;

    painter.color[3] *= alpha;
    painter.flags |= PAINTER_PLANET_SHADER;

    vec4_copy(planet->obj.pvo[0], pos);
    mat4_set_identity(mat);
    mat4_itranslate(mat, pos[0], pos[1], pos[2]);
    mat4_iscale(mat, radius, radius, radius);

    // Compute sun position.
    vec3_copy(planets->sun->obj.pvo[0], sun_pos);
    sun_pos[3] = planets->sun->radius_m / DAU;
    painter.sun = &sun_pos;

    // Apply the rotation.
    mat4_mul(mat, core->observer->re2i, mat);
    mat4_rx(-planet->rot.obliquity, mat, mat);

    if (planet->rot.period) {
        rot = (core->observer->tt - epoch) / planet->rot.period * 2 * M_PI;
        rot += planet->rot.offset;
        mat4_rz(rot, mat, mat);
    }
    painter.transform = &mat;

    if (planet->id == SUN)
        painter.light_emit = &full_emit;
    if (planet->id == MOON)
        painter.shadow_color_tex = planets->earth_shadow_tex;
    // Lower current moon texture contrast.
    if (planet->id == MOON) painter.contrast = 0.6;

    // XXX: for the moment we only use depth if the planet has a ring,
    // to prevent having to clean the depth buffer.
    if (planet->rings.tex) {
        dist = vec3_norm(planet->obj.pvo[0]);
        depth_range[0] = dist * 0.5;
        depth_range[1] = dist * 2;
        painter.depth_range = &depth_range;
    }

    hips_render_traverse(planet->hips, &painter, angle,
                         USER_PASS(planet, &nb_tot, &nb_loaded),
                         on_render_tile);
    if (planet->rings.tex)
        render_rings(planet, &painter);
    progressbar_report(planet->name, planet->name, nb_loaded, nb_tot, -1);
}

static void planet_render_orbit(const planet_t *planet,
                                double alpha,
                                const painter_t *painter_)
{
    painter_t painter = *painter_;
    double pos[4], mat[4][4], p[3], v[3];
    double dist, depth_range[2];
    const double G = 6.674e-11;
    const double SPD = 60 * 60 * 24;
    // μ in (AU)³(day)⁻²
    double mu = G * planet->parent->mass / (DAU * DAU * DAU) * SPD * SPD;
    double in, om, w, a, n, ec, ma;

    if (planet->color[3]) vec3_copy(planet->color, painter.color);

    // Compute orbit elements.
    vec3_sub(planet->obj.pvo[0], planet->parent->obj.pvo[0], p);
    vec3_sub(planet->obj.pvo[1], planet->parent->obj.pvo[1], v);
    orbit_elements_from_pv(p, v, mu, &in, &om, &w, &a, &n, &ec, &ma);

    // Center the rendering on the parent planet.
    vec4_copy(planet->parent->obj.pvo[0], pos);
    mat4_set_identity(mat);
    mat4_itranslate(mat, pos[0], pos[1], pos[2]);
    painter.transform = &mat;

    // Set the depth range same as the parent!!!!
    dist = vec3_norm(planet->parent->obj.pvo[0]);
    depth_range[0] = dist * 0.5;
    depth_range[1] = dist * 2;
    painter.depth_range = &depth_range;

    painter.lines_width = 1.5;
    paint_orbit(&painter, FRAME_ICRF, painter.obs->tt,
                in, om, w, a, n, ec, ma);
}

static void planet_render(const planet_t *planet, const painter_t *painter_)
{
    double pos[4], vpos[3], p_win[4];
    double label_color[4] = RGBA(124, 124, 255, 255);
    double color[4];
    double vmag;             // Observed magnitude.
    double point_size;       // Radius size of point (pixel).
    double point_r;          // Size (rad) and luminance if the planet is seen
    double point_luminance;  // as a point source (like a star).
    double radius;           // Planet rendered radius (AU).
    double r_scale = 1.0;    // Artificial size scale.
    double r;                // Angular diameter (rad).
    double sep;              // Angular sep to screen center.
    double hips_alpha = 0;
    double az, alt;
    painter_t painter = *painter_;
    point_t point;
    double hips_k = 2.0; // How soon we switch to the hips survey.
    char label[256];
    planets_t *planets = (planets_t*)planet->obj.parent;

    vmag = planet->obj.vmag;
    if (planet->id == EARTH) return;
    if (planet->id != MOON && vmag > painter.mag_max) return;

    vec4_copy(planet->obj.pvo[0], pos);
    convert_direction(painter.obs, FRAME_ICRF, FRAME_OBSERVED, 0, pos, pos);

    core_get_point_for_mag(vmag, &point_size, &point_luminance);
    point_r = core_get_apparent_angle_for_point(&painter, point_size);

    radius = planet->radius_m / DAU;

    // Artificially increase the moon size when we are zoomed out, so that
    // we can render it as a hips survey.
    if (planet->id == MOON) {
        hips_k = 4.0;
        r_scale = mix(1, 8, smoothstep(35 * DD2R, 220 * DD2R, core->fov));
    }

    r = 2.0 * radius / vec3_norm(planet->obj.pvo[0]);
    // First approximation, to skip non visible planets.
    // XXX: we need to compute the max visible fov (for the moment I
    // add a factor of 1.5 to make sure).
    vec3_normalize(pos, pos);
    mat4_mul_vec3(painter.obs->ro2v, pos, vpos);
    sep = eraSepp(vpos, (double[]){0, 0, -1});
    if (sep - r > core->fov * 1.5)
        return;

    // Remove below horizon planets (so that the names don't show up).
    if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0) {
        eraC2s(pos, &az, &alt);
        if (alt < -max(r * r_scale, point_r)) return;
    }

    // If the planet is visible, report it for tonemapping.
    if (project(painter.proj, PROJ_TO_WINDOW_SPACE, 2, vpos, p_win)) {
        if (pos[2] > 0) {
            core_report_vmag_in_fov(vmag, r, sep);
        }
    }

    if (planet->hips && hips_k * r * r_scale >= point_r) {
        hips_alpha = smoothstep(1.0, 0.5, point_r / (hips_k * r * r_scale ));
    }
    vec4_copy(planet->color, color);
    if (!color[3]) vec4_set(color, 1, 1, 1, 1);
    color[3] *= point_luminance * (1.0 - hips_alpha);

    // Lower point halo effect for objects with large radius.
    // (Mostly for the Sun, but also affect planets at large fov).
    painter.points_smoothness *= mix(1.0, 0.25,
                                     smoothstep(0.5, 3.0, point_r * DR2D));
    point = (point_t) {
        .pos = {p_win[0], p_win[1]},
        .size = point_size,
        .color = {color[0], color[1], color[2], color[3]},
        .oid = planet->obj.oid,
    };
    paint_points(&painter, 1, &point, FRAME_WINDOW);

    if (hips_alpha > 0) {
        planet_render_hips(planet, radius * r_scale, hips_alpha, &painter);
    }


    if (vmag <= painter.label_mag_max) {
        mat4_mul_vec3(core->observer->ro2v, pos, vpos);
        if (project(painter.proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_WINDOW_SPACE,
                2, vpos, vpos)) {
            if (r_scale == 1.0) strcpy(label, planet->name);
            else sprintf(label, "%s (x%.1f)", planet->name, r_scale);
            labels_add(label, vpos, point_size, 16, label_color, 0,
                       ANCHOR_AROUND, -vmag);
        }
    }

    // For the moment we never render the orbits!
    // I think it would be better to render the orbit in a separate module
    // so that it works with any body, not just planets.  But this is
    // hard to know in advance what depth range to use.  I leave this
    // disabled until I implement a deferred renderer.
    if ((0)) planet_render_orbit(planet, 1.0, &painter);

    // Render the Sun halo.
    if (planet->id == SUN) {
        mat4_mul_vec3(core->observer->ro2v, pos, vpos);
        project(painter.proj, PROJ_TO_WINDOW_SPACE, 2, vpos, vpos);
        paint_texture(&painter, planets->halo_tex, NULL, vpos, 200.0, NULL, 0);
    }
}

static int sort_cmp(const obj_t *a, const obj_t *b)
{
    const planet_t *pa = (const planet_t*)a;
    const planet_t *pb = (const planet_t*)b;
    return cmp(eraPm(pb->obj.pvo[0]), eraPm(pa->obj.pvo[0]));
}

static int planets_update(obj_t *obj, const observer_t *obs, double dt)
{
    planets_t *planets = (planets_t*)obj;
    planet_t *p;

    PLANETS_ITER(planets, p) {
        obj_update((obj_t*)p, obs, dt);
    }
    // Sort all the planets by distance to the observer.
    DL_SORT(obj->children, sort_cmp);
    return 0;
}

static int planets_render(const obj_t *obj, const painter_t *painter)
{
    planets_t *planets = (planets_t*)obj;
    planet_t *p;

    PLANETS_ITER(planets, p) {
        planet_render(p, painter);
    }
    return 0;
}

static obj_t *planets_get_by_oid(
        const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *child;
    if (!oid_is_catalog(oid, "HORI")) return NULL;
    OBJ_ITER(obj, child, "planet") {
        if (child->oid == oid) {
            child->ref++;
            return child;
        }
    }
    return NULL;
}

/*
 * Conveniance function to look for a planet by name
 */
static planet_t *planet_get_by_name(planets_t *planets, const char *name)
{
    planet_t *planet;
    char id[64];
    str_to_upper(name, id);
    HASH_FIND_STR(planets->planets, id, planet);
    return planet;
}

static uint64_t compute_nsid(const char *name)
{
    uint64_t crc;
    crc = crc32(0L, (const Bytef*)name, strlen(name));
    return (1ULL << 63) | (201326592ULL << 35) | (crc & 0xffffffff);
}

// Parse an orbit line as returned by HORIZONS online service.
static int parse_orbit(planet_t *p, const char *v)
{
    int r;
    double mjd, ec, qr, in, om, w, tp, n, ma, ta, a, ad, pr;
    if (!str_startswith(v, "horizons:")) return 0;
    r = sscanf(v, "horizons:%lf, A.D. %*s %*s "
               "%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf",
               &mjd, &ec, &qr, &in, &om, &w, &tp, &n, &ma, &ta, &a, &ad, &pr);
    if (r != 13) {
        LOG_E("Cannot parse orbit line '%s'", v);
        return -1;
    }
    p->orbit.type = 1;
    p->orbit.kepler.jd = mjd;
    p->orbit.kepler.in = in * DD2R;
    p->orbit.kepler.om = om * DD2R;
    p->orbit.kepler.w = w * DD2R;
    p->orbit.kepler.a = a * (1000.0 / DAU);
    p->orbit.kepler.n = n * DD2R * 60 * 60 * 24;
    p->orbit.kepler.ec = ec;
    p->orbit.kepler.ma = ma * DD2R;
    return 0;
}

// Parse the planet data.
static int planets_ini_handler(void* user, const char* section,
                               const char* attr, const char* value)
{
    planets_t *planets = user;
    planet_t *planet;
    char id[64], name[64], unit[32];
    float v;

    str_to_upper(section, id);
    HASH_FIND_STR(planets->planets, id, planet);
    if (!planet) {
        planet = (void*)obj_create("planet", id, (obj_t*)planets, NULL);
        HASH_ADD_KEYPTR(hh, planets->planets, planet->obj.id,
                        strlen(planet->obj.id), planet);
        strcpy(name, section);
        name[0] += 'A' - 'a';
        planet->name = strdup(name);
        planet->obj.nsid = compute_nsid(name);
        if (strcmp(id, "SUN") == 0) planets->sun = planet;
        if (strcmp(id, "EARTH") == 0) planets->earth = planet;
    }
    if (strcmp(attr, "horizons_id") == 0) {
        sscanf(value, "%d", &planet->id);
        planet->obj.oid = oid_create("HORI", planet->id);
    }
    if (strcmp(attr, "type") == 0) {
        strncpy(planet->obj.type, value, 4);
    }
    if (strcmp(attr, "radius") == 0) {
        sscanf(value, "%f km", &v);
        planet->radius_m = v * 1000;
    }
    if (strcmp(attr, "parent") == 0) {
        planet->parent = planet_get_by_name(planets, value);
        assert(planet->parent);
    }
    if (strcmp(attr, "vmag") == 0) {
        sscanf(value, "%f", &v);
        planet->obj.vmag = v;
    }
    if (strcmp(attr, "color") == 0) {
        sscanf(value, "%lf, %lf, %lf",
               &planet->color[0], &planet->color[1], &planet->color[2]);
        planet->color[3] = 1.0;
    }
    if (strcmp(attr, "albedo") == 0) {
        sscanf(value, "%f", &v);
        planet->albedo = v;
    }
    if (strcmp(attr, "rot_obliquity") == 0) {
        if (sscanf(value, "%f deg", &v) != 1) goto error;
        planet->rot.obliquity = v * DD2R;
    }
    if (strcmp(attr, "rot_period") == 0) {
        if (sscanf(value, "%f %s", &v, unit) != 2) goto error;
        if (strcmp(unit, "h") == 0) v /= 24.0;
        planet->rot.period = v;
    }
    if (strcmp(attr, "rot_offset") == 0) {
        sscanf(value, "%f", &v);
        planet->rot.offset = v * DD2R;
    }
    if (strcmp(attr, "rings_inner_radius") == 0) {
        sscanf(value, "%f km", &v);
        planet->rings.inner_radius = v * 1000;
    }
    if (strcmp(attr, "rings_outer_radius") == 0) {
        sscanf(value, "%f km", &v);
        planet->rings.outer_radius = v * 1000;
    }
    if (strcmp(attr, "orbit") == 0) {
        parse_orbit(planet, value);
    }
    if (strcmp(attr, "mass") == 0) {
        sscanf(value, "%lg kg", &planet->mass);
    }

    return 0;

error:
    LOG_W("Cannot parse planet attribute: [%s] %s = %s",
          section, attr, value);
    return -1;
}

static int planets_init(obj_t *obj, json_value *args)
{
    planets_t *planets = (void*)obj;
    char *data, name[128];
    const char *path;
    int r;
    planet_t *p;
    regex_t reg;
    regmatch_t matches[2];

    data = asset_get_data("asset://planets.ini", NULL, NULL);
    ini_parse_string(data, planets_ini_handler, planets);
    assert(planets->sun);
    assert(planets->earth);

    // Add name identifiers:
    PLANETS_ITER(planets, p) {
        if (!p->obj.oid || !p->name) continue;
        identifiers_add("NAME", p->name, p->obj.oid, 0, p->obj.type,
                        p->obj.vmag, NULL, NULL);
    }

    // Add rings textures from assets.
    regcomp(&reg, "^.*/([^/]+)_rings.png$", REG_EXTENDED);
    ASSET_ITER("asset://textures/", path) {
        r = regexec(&reg, path, 2, matches, 0);
        if (r) continue;
        sprintf(name, "%.*s", (int)(matches[1].rm_eo - matches[1].rm_so),
                path + matches[1].rm_so);
        p = planet_get_by_name(planets, name);
        if (!p) continue;
        p->rings.tex = texture_from_url(path, TF_LAZY_LOAD);
    }
    regfree(&reg);

    planets->earth_shadow_tex =
        texture_from_url("asset://textures/earth_shadow.png", TF_LAZY_LOAD);
    planets->halo_tex =
        texture_from_url("asset://textures/halo.png", TF_LAZY_LOAD);

    return 0;
}

static obj_t *planets_get(const obj_t *obj, const char *id, int flags)
{
    planet_t *p;
    if (!str_startswith(id, "PLANET")) return NULL;
    PLANETS_ITER(obj, p) {
        if (str_equ(p->obj.id, id)) {
            p->obj.ref++;
            return &p->obj;
        }
    }
    return NULL;
}

static int planets_list(const obj_t *obj, observer_t *obs,
                        double max_mag, uint64_t hint, void *user,
                        int (*f)(void *user, obj_t *obj))
{
    planet_t *p;
    int nb = 0;
    PLANETS_ITER(obj, p) {
        obj_update((obj_t*)p, obs, 0);
        if (p->obj.vmag > max_mag) continue;
        nb++;
        if (f && f(user, &p->obj)) break;
    }
    return nb;
}

static double parse_release_date(const char *str)
{
    int iy, im, id, ihr, imn;
    double d1, d2;
    sscanf(str, "%d-%d-%dT%d:%dZ", &iy, &im, &id, &ihr, &imn);
    eraDtf2d("UTC", iy, im, id, ihr, imn, 0, &d1, &d2);
    return d1 - DJM0 + d2;
}

static int planets_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    const char *args_type, *frame, *release_date_str;
    double release_date = 0;
    planets_t *planets = (void*)obj;
    planet_t *p;

    if (!type || !args || strcmp(type, "hips")) return 1;
    args_type = json_get_attr_s(args, "type");
    if (!args_type) return 1;
    if (strcmp(args_type, "planet") && strcmp(args_type, "planet-normal"))
        return 1;

    frame = json_get_attr_s(args, "hips_frame");
    if (!frame) return 1;
    p = planet_get_by_name(planets, frame);
    if (!p) return 1;
    release_date_str = json_get_attr_s(args, "hips_release_date");
    if (release_date_str)
        release_date = parse_release_date(release_date_str);
    if (strcmp(args_type, "planet") == 0) {
        p->hips = hips_create(url, release_date, NULL);
        hips_set_frame(p->hips, FRAME_ICRF);
    } else {
        p->hips_normalmap = hips_create(url, release_date, NULL);
        hips_set_frame(p->hips_normalmap, FRAME_ICRF);
    }
    return 0;
}


/*
 * Meta class declarations.
 */

static obj_klass_t planet_klass = {
    .id = "planet",
    .model = "jpl_sso",
    .size = sizeof(planet_t),
    .update = planet_update,
    .attributes = (attribute_t[]) {
        PROPERTY("name"),
        PROPERTY("alt"),
        PROPERTY("az"),
        PROPERTY("ra"),
        PROPERTY("dec"),
        PROPERTY("radec"),
        PROPERTY("azalt"),
        PROPERTY("vmag"),
        PROPERTY("distance"),
        PROPERTY("phase", "f", MEMBER(planet_t, phase)),
        PROPERTY("radius", "f", MEMBER(planet_t, radius)),
        PROPERTY("type"),
        {}
    },
};
OBJ_REGISTER(planet_klass)

static obj_klass_t planets_klass = {
    .id     = "planets",
    .size   = sizeof(planets_t),
    .flags  = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init   = planets_init,
    .update = planets_update,
    .render = planets_render,
    .get_by_oid = planets_get_by_oid,
    .get     = planets_get,
    .list    = planets_list,
    .add_data_source = planets_add_data_source,
    .render_order = 30,
};
OBJ_REGISTER(planets_klass)
