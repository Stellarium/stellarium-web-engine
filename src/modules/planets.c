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
    int         ss_index;  // For Mercury to Neptune.
    int         jupmoon;   // Jupiter moon index.

    // Precomputed values.
    double      pvh[2][3];   // equ, J2000.0, AU heliocentric pos and speed.
    double      pvg[2][3];   // equ, J2000.0, AU geocentric pos and speed.
    double      hpos[3];     // ecl, heliocentric pos J2000.0
    double      phase;
    double      radius;     // XXX ?

    // Rotation elements
    struct {
        double obliquity;
        double period;
        double offset;
    } rot;

    // Rings attributes.
    struct {
        double inner_radius; // (meter)
        double outer_radius; // (meter)
        texture_t *tex;
    } rings;

    hips_t      *hips;              // Hips survey of the planet.
    hips_t      *hips_normalmap;    // Normal map survey.
};


static int planet_update(obj_t *obj, const observer_t *obs, double dt);

static obj_klass_t planet_klass = {
    .id = "planet",
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
        PROPERTY("rise"),
        PROPERTY("set"),
        PROPERTY("phase", "f", MEMBER(planet_t, phase)),
        PROPERTY("radius", "f", MEMBER(planet_t, radius)),
        PROPERTY("type"),
        {}
    },
};
OBJ_REGISTER(planet_klass)

// Planets layer object type;
typedef struct planets {
    obj_t       obj;
    planet_t    *planets; // The map of all the planets.
    planet_t    *sun;
    planet_t    *earth;
    planet_t    *moon;

    // Status of the hipslist parsing.
    int         hipslist_parsed;
} planets_t;

static planet_t *planet_get(planets_t *planets, const char *name);
static int planets_init(obj_t *obj, json_value *args);
static int planets_update(obj_t *obj, const observer_t *obs, double dt);
static int planets_render(const obj_t *obj, const painter_t *painter);
static obj_t *planets_get(const obj_t *obj, const char *id, int flags);
static int planets_list(const obj_t *obj, double max_mag, void *user,
                        int (*f)(const char *id, void *user));

static obj_klass_t planets_klass = {
    .id     = "planets",
    .size   = sizeof(planets_t),
    .flags  = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init   = planets_init,
    .update = planets_update,
    .render = planets_render,
    .get     = planets_get,
    .list    = planets_list,
    .render_order = 30,
};

enum {
    SUN, MERCURY, VENUS, EARTH, MARS, JUPITER, SATURN, URANUS, NEPTUNE
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


/*
 * Conveniance function to look for a planet by name
 */
static planet_t *planet_find(planets_t *planets, const char *name)
{
    planet_t *p;
    PLANETS_ITER(planets, p) {
        if (strcasecmp(p->name, name) == 0) return p;
    }
    return NULL;
}


static int planet_update(obj_t *obj, const observer_t *obs, double dt)
{
    planet_t *planet = (planet_t*)obj;
    const double *vis;  // Visual element of planet.
    double i;   // Phase angle.
    double rho; // Distance to Earth (AU).
    double rp;  // Distance to Sun (AU).
    planets_t *planets = (planets_t*)obj->parent;
    planet_t *earth = planets->earth;

    // Skip if already up to date.
    if (obj->observer_hash == obs->hash) return 0;

    // Make sure the earth is up to date
    if (planet != planets->earth) obj_update(&earth->obj, obs, 0);

    // Compute the position of the planet.
    // XXX: we could use an approximation at the beginning, and only compute
    // the exact pos if needed.
    obj->pos.unit = 1.0; // AU.
    if (planet == earth) {
        // Heliocentric position of the earth (AU)
        eraCpv(obs->earth_pvh, planet->pvh);
        eraZpv(planet->pvg);
        // Ecliptic position.
        mat4_mul_vec3(obs->ri2e, planet->pvh[0], planet->hpos);
        planet->phase = NAN;
    }
    if (planet == planets->sun) {
        eraZpv(planet->pvh);
        eraSxpv(-1, obs->earth_pvh, planet->pvg);
        planet->phase = NAN;
    }

    if (planet == planets->moon) {
        double lambda, beta, dist;
        double rmatecl[3][3], rmatp[3][3];
        double obl;
        double pos[3];
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
        eraCp(pos, planet->pvg[0]);

        // Heliocentric position.
        eraPpp(planet->pvg[0], obs->earth_pvh[0], planet->pvh[0]);

        i = eraSepp(planet->pvh[0], planet->pvg[0]);
        planet->phase = 0.5 * cos(i) + 0.5;

        // We don't know the speed.
        planet->pvg[1][0] = NAN;
        planet->pvg[1][1] = NAN;
        planet->pvg[1][2] = NAN;
        planet->pvh[1][0] = NAN;
        planet->pvh[1][1] = NAN;
        planet->pvh[1][2] = NAN;
    }

    if (planet->ss_index) {
        eraPlan94(DJM0, obs->tt, planet->ss_index, planet->pvh);
        eraPvmpv(planet->pvh, obs->earth_pvh, planet->pvg);
        i = eraSepp(planet->pvh[0], planet->pvg[0]);
        planet->phase = 0.5 * cos(i) + 0.5;
        // Compute visual magnitude.
        i *= DR2D / 100;
        rho = vec3_norm(planet->pvh[0]);
        rp = vec3_norm(planet->pvg[0]);
        vis = VIS_ELEMENTS[planet->ss_index];
        planet->obj.vmag = vis[1] + 5 * log10(rho * rp) +
                           i * (vis[2] + i * (vis[3] + i * vis[4]));
    }

    if (planet->jupmoon) {
        double pvj[2][3];
        double mag;
        planet_t *jupiter;
        jupiter = planet_get(planets, "jupiter");
        obj_update(&jupiter->obj, obs, 0);
        l12(DJM0, obs->tt, planet->jupmoon, pvj);
        vec3_add(pvj[0], jupiter->pvg[0], planet->pvg[0]);
        vec3_add(pvj[1], jupiter->pvg[1], planet->pvg[1]);
        vec3_add(planet->pvg[0], obs->earth_pvh[0], planet->pvh[0]);

        // Compute visual magnitude.
        // http://www.physics.sfasu.edu/astro/asteroids/sizemagnitude.html
        rho = vec3_norm(planet->pvh[0]);
        rp = vec3_norm(planet->pvg[0]);
        mag = -1.0 / 0.2 * log10(sqrt(planet->albedo) *
                2.0 * planet->radius_m / 1000.0 / 1329.0);
        planet->obj.vmag = mag + 5 * log10(rho * rp);
    }
    mat4_mul_vec3(obs->ri2e, planet->pvh[0], planet->hpos);

    // Adjust vmag for saturn.
    if (planet->ss_index == SATURN) {
        double hlon, hlat;
        double earth_hlon, earth_hlat;
        double et, st, set;
        double earth_hpos[3];
        mat4_mul_vec3(obs->ri2e, obs->earth_pvh[0], earth_hpos);

        eraC2s(planet->hpos, &hlon, &hlat);
        eraC2s(earth_hpos, &earth_hlon, &earth_hlat);
        satrings(hlat, hlon, vec3_norm(planet->pvh[0]),
                 earth_hlon, vec3_norm(earth->pvh[0]),
                 obs->ut1 + DJM0, &et, &st);
        set = sin(fabs(et));
        planet->obj.vmag += (-2.60 + 1.25 * set) * set;
    }

    planet->radius = planet->radius_m / DAU / eraPm((double*)planet->pvg[0]);
    eraCpv(planet->pvg, obj->pos.pvg);

    compute_coordinates(obs, obj->pos.pvg, obj->pos.unit,
                        &obj->pos.ra, &obj->pos.dec,
                        NULL, NULL, NULL, NULL,
                        &obj->pos.az, &obj->pos.alt);
    return 0;
}

static int on_render_tile(hips_t *hips, const painter_t *painter_,
                          int order, int pix, void *user)
{
    planet_t *planet = USER_GET(user, 0);
    int *nb_tot = USER_GET(user, 1);
    int *nb_loaded = USER_GET(user, 2);
    painter_t painter = *painter_;
    texture_t *tex, *normalmap = false;
    projection_t proj;
    int split;
    double fade, uv[4][2];
    bool loaded;

    (*nb_tot)++;
    tex = hips_get_tile_texture(hips, order, pix, false, &painter,
                                uv, &proj, &split, &fade, &loaded);
    if (loaded) (*nb_loaded)++;
    if (planet->hips_normalmap) {
        // XXX: need to check if the UVs don't match.
        (*nb_tot)++;
        normalmap = hips_get_tile_texture(planet->hips_normalmap,
                order, pix, false, &painter, NULL, NULL, NULL, NULL, &loaded);
        if (loaded) (*nb_loaded)++;
    }

    // Texture not ready yet, we just use the planet color.
    if (!tex) {
        vec3_copy(planet->color, painter.color);
        painter.color[3] = 1;
    }
    painter.color[3] *= fade;

    // Hardcoded increase of the luminosity of the moon for the moment!
    // This should be specified in the survey itsefl I guess.
    if (strcmp(planet->obj.id, "MOON") == 0)
        vec3_mul(1.8, painter.color, painter.color);

    paint_quad(&painter, FRAME_OBSERVED, tex, normalmap, uv, &proj, split);
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
    mat3_iscale(mat, r, r);
    mat3_mul_vec3(mat, p, p);
    vec4_copy(p, out);
}

static void render_rings(texture_t *tex,
                         double inner_radius, double outer_radius,
                         const painter_t *painter_)
{
    projection_t proj = {
        .backward   = ring_project,
    };
    painter_t painter = *painter_;
    const double radii[2] = {inner_radius, outer_radius};
    proj.user = radii;
    painter.light_dir = NULL;
    painter.light_emit = NULL;
    painter.flags &= ~PAINTER_PLANET_SHADER;
    painter.flags |= PAINTER_NO_CULL_FACE;
    paint_quad(&painter, FRAME_OBSERVED, tex, NULL, NULL, &proj, 64);
}

static void planet_render_hips(const planet_t *planet,
                               double alpha,
                               const painter_t *painter)
{
    double mat[4][4];
    double pos[3];
    double radius = planet->radius_m / DAU;
    double dist = vec3_norm(planet->pvg[0]);
    double full_emit[3] = {1.0, 1.0, 1.0};
    double rh2e[4][4];
    double rot;
    double light_dir[3];
    double angle = 2 * planet->radius_m / DAU / vec2_norm(planet->pvg[0]);
    int nb_tot = 0, nb_loaded = 0;

    planets_t *planets = (planets_t*)planet->obj.parent;
    painter_t painter2 = *painter;
    painter2.color[3] *= alpha;
    painter2.flags |= PAINTER_PLANET_SHADER;

    // To make sure the hips is aligned with the planet position, we
    // compute the position in observed frame.
    eraS2c(planet->obj.pos.az, planet->obj.pos.alt, pos);
    mat4_set_identity(mat);
    mat4_itranslate(mat, pos[0] * dist, pos[1] * dist, pos[2] * dist);
    mat4_iscale(mat, radius, radius, radius);

    // Compute sun light direction.
    vec3_mul(-1, planet->pvh[0], light_dir);
    vec3_normalize(light_dir, light_dir);
    mat4_mul_vec3_dir(core->observer->ri2h, light_dir, light_dir);
    mat4_mul_vec3_dir(core->observer->ro2v, light_dir, light_dir);

    // Apply the rotation.  We have to change the light direction accordingly.
    // XXX: it's not very clear why we need to flip the y axis, this is
    // related to the fact that the horizontal coordinates are left handed!
    if (planet->parent == planets->earth) {
        mat4_mul(mat, core->observer->ri2h, mat);
        rot = core->observer->tt / planet->rot.period * 2 * M_PI;
        rot += planet->rot.offset;
        mat4_rz(rot, mat, mat);
    } else {
        mat4_invert(core->observer->re2h, rh2e);
        mat4_mul(mat, core->observer->re2h, mat);
        // Not sure about this.
        mat4_rx(-planet->rot.obliquity, mat, mat);
    }
    painter2.transform = &mat;
    painter2.light_dir = &light_dir;

    if (planet == planets->sun) painter2.light_emit = &full_emit;
    double depth_range[2] = {dist * 0.5, dist * 2};
    painter2.depth_range = &depth_range;
    hips_render_traverse(planet->hips, &painter2, angle,
                         USER_PASS(planet, &nb_tot, &nb_loaded),
                         on_render_tile);
    if (planet->rings.tex)
        render_rings(planet->rings.tex,
                     planet->rings.inner_radius / planet->radius_m,
                     planet->rings.outer_radius / planet->radius_m,
                     &painter2);
    painter2.depth_range = NULL;
    progressbar_report(planet->name, planet->name, nb_loaded, nb_tot);
}

static void planet_render(const planet_t *planet, const painter_t *painter)
{
    double pos[3], light_dir[3], vpos[3];
    double label_color[4] = RGBA(124, 124, 255, 255);
    double color[4];
    double mag;              // Observed magnitude at this fov.
    double point_r;          // Size (rad) and luminance if the planet is seen
    double point_luminance;  // as a point source (like a star).
    double r;                // Angular diameter (rad).
    double sep;              // Angular sep to screen center.
    double hips_alpha = 0;
    painter_t painter2 = *painter;
    point_t point;
    const double hips_k = 2.0; // How soon we switch to the hips survey.

    if (planet->ss_index == EARTH) return;

    eraS2c(planet->obj.pos.az, planet->obj.pos.alt, pos);
    if ((painter->flags & PAINTER_HIDE_BELOW_HORIZON) && pos[2] < 0)
        return;
    vec3_normalize(pos, pos);

    mag = core_get_observed_mag(planet->obj.vmag);
    if (mag > painter->mag_max) return;
    core_get_point_for_mag(mag, &point_r, &point_luminance);
    r = 2 * planet->radius_m / DAU / vec3_norm(planet->pvg[0]);

    // First approximation, to skip non visible planets.
    // XXX: we need to compute the max visible fov (for the moment I
    // add a factor of 1.5 to make sure).
    mat4_mul_vec3(painter2.obs->ro2v, pos, vpos);
    sep = eraSepp(vpos, (double[]){0, 0, -1});
    if (sep - r > core->fov * 1.5)
        return;

    core_report_vmag_in_fov(planet->obj.vmag, r, sep);

    if (planet->hips && hips_k * r >= point_r) {
        hips_alpha = smoothstep(1.0, 0.5, point_r / (hips_k * r));
    }
    vec4_copy(planet->color, color);
    if (!color[3]) vec4_set(color, 1, 1, 1, 1);
    color[3] *= point_luminance * (1.0 - hips_alpha);
    if (color[3] == 0) goto skip_point;

    vec3_mul(-1, planet->pvh[0], light_dir);
    vec3_normalize(light_dir, light_dir);
    mat4_mul_vec3(core->observer->ri2h, light_dir, light_dir);
    painter2.light_dir = &light_dir;

    if (0) {    // 'design' style.  Disabled for the moment.
        point_r *= 0.7;
        paint_planet(&painter2, pos, point_r, color,
                     planet->shadow_brightness, planet->obj.id);
    } else {
        point = (point_t) {
            .pos = {pos[0], pos[1], pos[2]},
            .size = point_r,
            .color = {color[0], color[1], color[2], color[3]},
        };
        strcpy(point.id, planet->obj.id);
        paint_points(&painter2, 1, &point, FRAME_OBSERVED);
    }

skip_point:
    if (hips_alpha > 0) {
        planet_render_hips(planet, hips_alpha, &painter2);
    }


    if (mag <= painter->label_mag_max) {
        mat4_mul_vec3(core->observer->ro2v, pos, pos);
        if (project(painter->proj,
                PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE,
                2, pos, pos)) {
            labels_add(planet->name, pos, point_r, 16, label_color,
                       ANCHOR_AROUND, -mag);
        }
    }
}

static int sort_cmp(const obj_t *a, const obj_t *b)
{
    const planet_t *pa = (const planet_t*)a;
    const planet_t *pb = (const planet_t*)b;
    return cmp(eraPm(pb->pvg[0]), eraPm(pa->pvg[0]));
}

static int on_hips(const char *url, double release_date, void *user)
{
    regex_t reg;
    regmatch_t matches[3];
    int r;
    char name[128];
    bool normalmap = false;
    planets_t *planets = user;
    planet_t *p;

    // Determine the name of the survey from the url:
    // some/thing/jupiter -> jupiter.
    regcomp(&reg, ".*/([^/-]+)(-normal)?$", REG_EXTENDED);
    r = regexec(&reg, url, 3, matches, 0);
    if (r) goto end;
    sprintf(name, "%.*s", matches[1].rm_eo - matches[1].rm_so,
                          url + matches[1].rm_so);
    normalmap = matches[2].rm_so >= 0;

    // Check if the name correspond to a planet.
    if ((p = planet_find(planets, name))) {
        LOG_V("Assign hips '%s' to planet '%s'", url, name);
        if (!normalmap) {
            p->hips = hips_create(url, release_date);
            hips_set_frame(p->hips, FRAME_OBSERVED);
        }
        else {
            p->hips_normalmap = hips_create(url, release_date);
            hips_set_frame(p->hips_normalmap, FRAME_OBSERVED);
        }
    }

end:
    regfree(&reg);
    return 0;
}

static int planets_update(obj_t *obj, const observer_t *obs, double dt)
{
    planets_t *planets = (planets_t*)obj;
    planet_t *p;

    if (planets->hipslist_parsed == -1) {
        planets->hipslist_parsed = hips_parse_hipslist(
                "https://data.stellarium.org/surveys/hipslist",
                on_hips, planets);
    }

    // Sort all the planets by distance to the observer.
    obj_update(&planets->earth->obj, obs, dt);
    PLANETS_ITER(planets, p) {
        if (p->ss_index != EARTH)
            obj_update((obj_t*)p, obs, dt);
    }
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

static planet_t *planet_get(planets_t *planets, const char *name)
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

// Parse the planet data.
static int planets_ini_handler(void* user, const char* section,
                               const char* attr, const char* value)
{
    planets_t *planets = user;
    planet_t *planet;
    char id[64], name[64];
    float v;
    str_to_upper(section, id);
    HASH_FIND_STR(planets->planets, id, planet);
    if (!planet) {
        planet = (void*)obj_create("planet", id, (obj_t*)planets, NULL);
        HASH_ADD_KEYPTR(hh, planets->planets, planet->obj.id,
                        strlen(planet->obj.id), planet);
        strcpy(name, section);
        name[0] += 'A' - 'a';
        identifiers_add(id, "NAME", name, NULL, NULL);
        planet->name = strdup(name);
        planet->obj.nsid = compute_nsid(name);
        if (strcmp(id, "SUN") == 0) planets->sun = planet;
        if (strcmp(id, "EARTH") == 0) planets->earth = planet;
        if (strcmp(id, "MOON") == 0) planets->moon = planet;

        if (strcmp(id, "MERCURY") == 0) planet->ss_index = 1;
        if (strcmp(id, "VENUS") == 0) planet->ss_index = 2;
        if (strcmp(id, "EARTH") == 0) planet->ss_index = 3;
        if (strcmp(id, "MARS") == 0) planet->ss_index = 4;
        if (strcmp(id, "JUPITER") == 0) planet->ss_index = 5;
        if (strcmp(id, "SATURN") == 0) planet->ss_index = 6;
        if (strcmp(id, "URANUS") == 0) planet->ss_index = 7;
        if (strcmp(id, "NEPTUNE") == 0) planet->ss_index = 8;

        if (strcmp(id, "IO") == 0) planet->jupmoon = 1;
        if (strcmp(id, "EUROPA") == 0) planet->jupmoon = 2;
        if (strcmp(id, "GANYMEDE") == 0) planet->jupmoon = 3;
        if (strcmp(id, "CALLISTO") == 0) planet->jupmoon = 4;
    }
    if (strcmp(attr, "type") == 0) {
        strncpy(planet->obj.type, value, 4);
    }
    if (strcmp(attr, "radius") == 0) {
        sscanf(value, "%f km", &v);
        planet->radius_m = v * 1000;
    }
    if (strcmp(attr, "parent") == 0) {
        planet->parent = planet_get(planets, value);
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
        sscanf(value, "%f", &v);
        planet->rot.obliquity = v * DD2R;
    }
    if (strcmp(attr, "rot_period") == 0) {
        sscanf(value, "%f", &v);
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

    return 0;
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

    planets->hipslist_parsed = -1;
    data = asset_get_data("asset://planets.ini", NULL, NULL);
    ini_parse_string(data, planets_ini_handler, planets);
    assert(planets->sun);
    assert(planets->earth);
    assert(planets->moon);

    // Add rings textures from assets.
    regcomp(&reg, "^.*/([^/]+)_rings.png$", REG_EXTENDED);
    ASSET_ITER("asset://textures/", path) {
        r = regexec(&reg, path, 2, matches, 0);
        if (r) continue;
        sprintf(name, "%.*s", matches[1].rm_eo - matches[1].rm_so,
                path + matches[1].rm_so);
        p = planet_find(planets, name);
        if (!p) continue;
        p->rings.tex = texture_from_url(path, 0);
    }

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

static int planets_list(const obj_t *obj, double max_mag, void *user,
                        int (*f)(const char *id, void *user))
{
    planet_t *p;
    int nb = 0;
    PLANETS_ITER(obj, p) {
        if (p->obj.vmag > max_mag) continue;
        nb++;
        if (f && f(p->obj.id, user)) break;
    }
    return nb;
}

OBJ_REGISTER(planets_klass)
