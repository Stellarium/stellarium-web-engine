/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <regex.h>
#include <zlib.h>

// Size of the cache allocated to all the tiles.
#define CACHE_SIZE (32 * (1 << 20))

// Min mag to start loading the gaia survey.
static const double GAIA_MIN_MAG = 8.0;

typedef struct stars stars_t;
typedef struct {
    uint64_t gaia;  // Gaia source id (0 if none)
    int     hd;     // HD number.
    char    sp;
    float   vmag;
    float   ra;     // ICRS RA  J2000.0 (rad)
    float   de;     // ICRS Dec J2000.0 (rad)
    float   pra;    // RA proper motion (rad/year)
    float   pde;    // Dec proper motion (rad/year)
    float   plx;    // Parallax (arcsec)
    float   bv;
    // Normalized ICRS position.
    double  pos[3];
} star_data_t;

// A single star.
typedef struct {
    obj_t       obj;
    star_data_t data;
} star_t;

static int star_update(obj_t *obj, const observer_t *obs, double dt);
static int star_init(obj_t *obj, json_value *args);
static int star_render(const obj_t *obj, const painter_t *painter);

static void star_render_name(const painter_t *painter, const star_data_t *s,
                             const double pos[3], double size, double vmag);
static void get_star_color(const char sp, double out[3]);


static obj_klass_t star_klass = {
    .id         = "star",
    .init       = star_init,
    .size       = sizeof(star_t),
    .update     = star_update,
    .render     = star_render,
    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        PROPERTY("ra"),
        PROPERTY("dec"),
        PROPERTY("distance"),
        PROPERTY("alt"),
        PROPERTY("az"),
        PROPERTY("radec"),
        PROPERTY("azalt"),
        PROPERTY("rise"),
        PROPERTY("set"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {},
    },
};
OBJ_REGISTER(star_klass)

// Precompute values about the star position to make rendering faster.
static void compute_pv(double ra, double de,
                       double pra, double pde, double plx, star_data_t *s)
{
    int r;
    double pv[2][3];
    r = eraStarpv(ra, de, pra, pde, plx, 0, pv);
    if (r & (2 | 4)) LOG_W("Wrong star coordinates");
    vec3_normalize(pv[0], s->pos);
}

// Get the pix number from a gaia source id at a given level.
static int gaia_index_to_pix(int order, uint64_t id)
{
    return (id >> 35) / (1 << (2 * (12 - order)));
}

static int star_init(obj_t *obj, json_value *args)
{
    // Support creating a star using noctuasky model data json values.
    star_t *star = (star_t*)obj;
    json_value *model;
    star_data_t *d = &star->data;

    model= json_get_attr(args, "model_data", json_object);
    if (model) {
        d->ra = json_get_attr_f(model, "ra", 0) * DD2R;
        d->de = json_get_attr_f(model, "de", 0) * DD2R;
        d->plx = json_get_attr_f(model, "plx", 0) / 1000.0;
        d->pra = json_get_attr_f(model, "pm_ra", 0) * ERFA_DMAS2R;
        d->pde = json_get_attr_f(model, "pm_de", 0) * ERFA_DMAS2R;
        d->vmag = json_get_attr_f(model, "Vmag", NAN);
        if (isnan(d->vmag))
            d->vmag = json_get_attr_f(model, "Bmag", NAN);
        compute_pv(d->ra, d->de, d->pra, d->pde, d->plx, d);
    }
    return 0;
}

static int star_update(obj_t *obj, const observer_t *obs, double dt)
{
    star_t *star = (star_t*)obj;
    double dist;
    eraASTROM *astrom = (void*)&obs->astrom;
    eraPmpx(star->data.ra, star->data.de, 0, 0, star->data.plx, 0,
            astrom->pmt, astrom->eb, obj->pos.pvg[0]);

    // Multiply by distance in AU:
    if (star->data.plx > 0.001) {
        dist = 1.0 / (star->data.plx) * PARSEC_IN_METER / DAU;
        eraSxp(dist, obj->pos.pvg[0], obj->pos.pvg[0]);
        obj->pos.unit = 1.0;
    } else {
        obj->pos.unit = INFINITY;
    }
    obj->vmag = star->data.vmag;
    // Set speed to 0.
    obj->pos.pvg[1][0] = obj->pos.pvg[1][1] = 0;
    // Compute radec and azalt.
    compute_coordinates(obs, obj->pos.pvg, obj->pos.unit,
                        &obj->pos.ra, &obj->pos.dec,
                        NULL, NULL, NULL, NULL,
                        &obj->pos.az, &obj->pos.alt);
    return 0;
}

// Render a single star.
// This should be used only for stars that have been manually created.
static int star_render(const obj_t *obj, const painter_t *painter_)
{
    // XXX: the code is almost the same as the inner loop in stars_render.
    star_t *star = (star_t*)obj;
    const star_data_t *s = &star->data;
    double ri, di, aob, zob, hob, dob, rob;
    double p[4], size, luminance, mag;
    double color[3];
    painter_t painter = *painter_;
    point_t point;

    // XXX: Use convert_coordinates instead!
    eraAtciq(s->ra, s->de, s->pra, s->pde, s->plx, 0,
            &core->observer->astrom, &ri, &di);
    eraAtioq(ri, di, &core->observer->astrom,
            &aob, &zob, &hob, &dob, &rob);
    if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && zob > 90 * DD2R)
        return 0;
    eraS2c(aob, 90 * DD2R - zob, p);
    mag = core_get_observed_mag(s->vmag);
    core_get_point_for_mag(mag, &size, &luminance);
    get_star_color(s->sp, color);
    point = (point_t) {
        .pos = {p[0], p[1], p[2]},
        .size = size,
        .color = {color[0], color[1], color[2], luminance},
    };
    strcpy(point.id, obj->id);
    paint_points(&painter, 1, &point, FRAME_OBSERVED);

    if (s->vmag <= painter.label_mag_max) {
        convert_coordinates(core->observer, FRAME_OBSERVED, FRAME_VIEW, 0,
                            p, p);
        if (project(painter.proj,
                    PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE, 2, p, p))
            star_render_name(&painter, s, p, size, mag);
    }
    return 0;
}

// Could we just replace with a nuniq uint64_t?
typedef struct {
    int order;
    int pix;
} tile_pos_t;

typedef struct {
    tile_pos_t  pos;
    stars_t     *parent;
    int         flags;
    double      mag_min;
    double      mag_max;
    int         nb;
    star_data_t *stars;
} tile_t;

void stars_load(const char *path, stars_t *stars);
static void get_star_color(const char sp, double out[3]);

struct stars {
    obj_t           obj;
    cache_t         *tiles;
    double          mag_max;
    regex_t         search_reg;
    char            *survey;
    bool            visible;
};

static int stars_init(obj_t *obj, json_value *args);
static int stars_render(const obj_t *obj, const painter_t *painter);
static obj_t *stars_get(const obj_t *obj, const char *id, int flags);
static obj_t *stars_get_by_nsid(const obj_t *obj, uint64_t nsid);
static int stars_list(const obj_t *obj, double max_mag, void *user,
                     int (*f)(const char *id, void *user));
static obj_t *stars_add_res(obj_t *obj, json_value *val,
                            const char *base_path);

static obj_klass_t stars_klass = {
    .id             = "stars",
    .size           = sizeof(stars_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = stars_init,
    .render         = stars_render,
    .get            = stars_get,
    .get_by_nsid    = stars_get_by_nsid,
    .list           = stars_list,
    .add_res        = stars_add_res,
    .render_order   = 20,
    .attributes = (attribute_t[]) {
        PROPERTY("max_mag", "f", MEMBER(stars_t, mag_max)),
        PROPERTY("visible", "b", MEMBER(stars_t, visible)),
        {},
    },
};

static star_t *star_create(const star_data_t *data)
{
    star_t *star;
    star = (star_t*)obj_create("star", NULL, NULL, NULL);
    strcpy(star->obj.type, "*");
    star->data = *data;
    star->obj.nsid = star->data.gaia;
    if (star->data.hd) {
        asprintf(&star->obj.id, "HD %d", star->data.hd);
        identifiers_add(star->obj.id, "HD", star->obj.id + 3, NULL, NULL);
    } else {
        asprintf(&star->obj.id, "GAIA %" PRId64, star->data.gaia);
    }
    return star;
}

// Used by the cache.
static int del_tile(void *data)
{
    tile_t *tile = data;
    free(tile->stars);
    free(tile);
    return 0;
}

// In place shuffle of the data bytes for optimized compression.
static void shuffle_bytes(uint8_t *data, int nb, int size)
{
    int i, j;
    uint8_t *buf = calloc(nb, size);
    memcpy(buf, data, nb * size);
    for (j = 0; j < size; j++) {
        for (i = 0; i < nb; i++) {
            data[j * nb + i] = buf[i * size + j];
        }
    }
    free(buf);
}

static int on_file_tile_loaded(int version, int order, int pix,
                               int size, void *data, void *user)
{
    int i, sp;
    tile_pos_t pos = {order, pix};
    stars_t *stars = user;
    tile_t *tile;
    star_data_t *d;
    double vmag, ra, de, pra, pde, plx, bv;

    assert(size % (4 * 9) == 0);
    tile = cache_get(stars->tiles, &pos, sizeof(pos));
    assert(!tile);

    tile = calloc(1, sizeof(*tile));
    tile->parent = stars;
    tile->pos = pos;
    tile->mag_min = +INFINITY;
    tile->mag_max = -INFINITY;
    tile->nb = size / (4 * 9);
    tile->stars = calloc(tile->nb, sizeof(*tile->stars));
    cache_add(stars->tiles, &pos, sizeof(pos), tile,
              tile->nb * sizeof(*tile->stars), del_tile);

    // Unshuffle the data.
    shuffle_bytes(data, 4 * 9, tile->nb);

    for (i = 0; i < tile->nb; i++) {
        d = &tile->stars[i];
        binunpack(data + i * 4 * 9, "iifffffff",
                  &d->hd, &sp, &vmag, &ra, &de, &plx, &pra, &pde, &bv);
        d->sp = sp;
        d->vmag = vmag;
        d->ra = ra;
        d->de = de;
        d->pra = pra;
        d->pde = pde;
        d->plx = plx;
        d->bv = bv;
        compute_pv(ra, de, pra, pde, plx, d);
        tile->mag_min = min(tile->mag_min, vmag);
        tile->mag_max = max(tile->mag_max, vmag);
    }
    return 0;
}

static int on_gaia_tile_loaded(int version, int order, int pix,
                               int size, void *data, void *user)
{
    /* Gaia packed data:
     *
     * 8 bytes (uint64_t)   source_id
     * 4 bytes (float)      GMag
     * 4 bytes (float)      ra
     * 4 bytes (float)      dec
     * 4 bytes (float)      plx
     * 4 bytes (float)      pmra
     * 4 bytes (float)      pmdec
     *
     */

    int i, r;
    double vmag, ra, dec, pmra, pmdec, plx, rv;
    tile_pos_t pos = {order, pix};
    stars_t *stars = user;
    tile_t *tile;
    star_data_t *d;

    assert(size % 32 == 0);
    tile = cache_get(stars->tiles, &pos, sizeof(pos));
    assert(!tile);

    tile = calloc(1, sizeof(*tile));
    tile->parent = stars;
    tile->pos = pos;
    tile->mag_min = +INFINITY;
    tile->mag_max = -INFINITY;
    tile->nb = size / 32;
    tile->stars = calloc(tile->nb, sizeof(*tile->stars));
    cache_add(stars->tiles, &pos, sizeof(pos), tile,
              tile->nb * sizeof(*tile->stars), del_tile);

    // Unshuffle the data.
    shuffle_bytes(data, 32, tile->nb);

    for (i = 0; i < tile->nb; i++) {
        d = &tile->stars[i];
        binunpack(data + i * 32, "Qffffff",
                  &d->gaia, &vmag, &ra, &dec, &plx, &pmra, &pmdec);

        // I disable this part until we switch to gaia dr2, since the dr1
        // survey seems to have some bugs in the stars locations.
        // Once we swtich to dr2 we can remore the if ((0)) and this comment.
        if ((0))
        if (gaia_index_to_pix(order, d->gaia) != pix) {
            LOG_E("Wrong gaia id: order: %d, pix: %d, id: %lu, "
                  "expected pix: %d", order, pix, d->gaia,
                  gaia_index_to_pix(order, d->gaia));
            assert(false);
        }

        // Convert to proper units.
        ra *= DD2R;
        dec *= DD2R;
        pmra *= ERFA_DMAS2R;
        pmdec *= ERFA_DMAS2R;
        plx /= 1000.0;

        // Convert values from J2015 to J2000
        r = eraPmsafe(ra, dec, pmra, pmdec, plx, 0.0,
                      ERFA_DJ00, 15 * ERFA_DJY, // J2015
                      ERFA_DJ00, 0.0, // Target J2000
                      &ra, &dec, &pmra, &pmdec, &plx, &rv);
        if (r & ~1) LOG_E("Cannot convert from J2015 to J2000: error %d", r);

        d->vmag = vmag;
        d->ra = ra;
        d->de = dec;
        d->plx = plx;
        d->pra = pmra;
        d->pde = pmdec;
        compute_pv(ra, dec, pmra, pmdec, plx, d);

        tile->mag_min = min(tile->mag_min, d->vmag);
        tile->mag_max = max(tile->mag_max, d->vmag);
    }
    return 0;
}

static int stars_init(obj_t *obj, json_value *args)
{
    const char *path;
    const void *data;
    int size;
    stars_t *stars = (stars_t*)obj;
    stars->visible = true;
    stars->mag_max = INFINITY;
    eph_file_register_tile_type("STAR", on_file_tile_loaded);
    eph_file_register_tile_type("GAIA", on_gaia_tile_loaded);
    stars->tiles = cache_create(CACHE_SIZE);

    // Load all tile files that we have in the assets.
    ASSET_ITER("asset://stars/", path) {
        data = asset_get_data(path, &size, NULL);
        eph_load(data, size, stars);
    }

    regcomp(&stars->search_reg, "(hd|gaia) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);

    asprintf(&stars->survey, "https://data.stellarium.org/surveys/gaia_dr2");
    return 0;
}

static tile_t *get_tile(stars_t *stars, int order, int pix, bool load,
                        bool *loading_complete)
{
    tile_pos_t pos = {order, pix};
    tile_t *tile;
    char *url;
    void *data;
    int size, code, dir;

    if (loading_complete) *loading_complete = true;
    tile = cache_get(stars->tiles, &pos, sizeof(pos));
    if (!tile && load && stars->survey && order >= 3) {
        dir = (pix / 10000) * 10000;
        asprintf(&url, "%s/Norder%d/Dir%d/Npix%d.eph",
                stars->survey, order, dir, pix);
        data = asset_get_data(url, &size, &code);
        if (!data && code) {
            // XXX: need to handle this case: the tiles should have a flag
            // that says that there is no data at this level, like in hips.c
        }
        if (loading_complete) *loading_complete = (code != 0);
        free(url);
        if (data) {
            eph_load(data, size, stars);
        }
    }
    return tile;
}

// Much faster than using sprintf!
// This is more less copied from stb_sprintf.
static void make_id(char *buf, const char *prefix, uint64_t n64)
{
    uint32_t n;
    char num[512], *p = buf, *o;
    char *s = num + sizeof(num);
    int num_size;

    for (;;) {
        o = s - 8;
        if (n64 >= 100000000) {
            n = (uint32_t)(n64 % 100000000);
            n64 /= 100000000;
        } else {
            n = (uint32_t)n64;
            n64 = 0;
        }
        while (n) {
            *--s = (char)(n % 10) + '0';
            n /= 10;
        }
        if (n64 == 0) {
            if ((s != (num + sizeof(num)) && (s[0] == '0')))
                ++s;
            break;
        }
        while (s != o) *--s = '0';
    }

    num_size = num + sizeof(num) - s;
    memcpy(p, prefix, strlen(prefix));
    p += strlen(prefix);
    *p++ = ' ';
    memcpy(p, s, num_size);
    p += num_size;
    *p = '\0';
}

bool debug_stars_show_all = false;

static int render_visitor(int order, int pix, void *user)
{
    stars_t *stars = USER_GET(user, 0);
    painter_t painter = *(const painter_t*)USER_GET(user, 1);
    int *nb_tot = USER_GET(user, 2);
    int *nb_loaded = USER_GET(user, 3);
    tile_t *tile;
    int i, n = 0;
    star_data_t *s;
    double p[4], p_ndc[4], size, luminance, mag;
    double color[3], viewport_cap[4];
    bool loaded;
    bool debug_show_all = DEBUG ? debug_stars_show_all : false;

    painter.mag_max = min(painter.mag_max, stars->mag_max);
    // Early exit if the tile is clipped.
    // (don't test at order 0 because is_tile_clipped doesn't work very
    //  well in that case!).
    if (    order > 0 &&
            painter_is_tile_clipped(&painter, FRAME_ICRS, order, pix, true))
        return 0;

    (*nb_tot)++;
    tile = get_tile(stars, order, pix, true, &loaded);
    if (loaded) (*nb_loaded)++;

    if (!tile) goto end;
    if (!debug_show_all && tile->mag_min > painter.mag_max) goto end;

    // Compute viewport cap for fast clipping test.
    // The cap is defined as the vector xyzw with xyz the observer viewing
    // direction in ICRS and w the cosinus of the max separation between
    // a visible point and xyz.
    //
    // XXX: this should probably already be available in the observer or
    //      painter.
    // XXX: compute it properly.  For the moment I just use some large
    //      security values.
    double max_sep = core->fov / 2.0 * 1.5 + 0.5 * DD2R;
    eraS2c(painter.obs->azimuth, painter.obs->altitude, viewport_cap);
    mat4_mul_vec3(painter.obs->rh2i, viewport_cap, viewport_cap);
    viewport_cap[3] = cos(max_sep);

    point_t *points = malloc(tile->nb * sizeof(*points));
    for (i = 0; i < tile->nb; i++) {
        s = &tile->stars[i];
        if (!debug_show_all && s->vmag > painter.mag_max) continue;
        if (vec3_dot(s->pos, viewport_cap) < viewport_cap[3]) continue;

        // Compute star observed and screen pos.
        vec3_copy(s->pos, p);
        p[3] = 0;
        convert_coordinates(core->observer, FRAME_ICRS, FRAME_OBSERVED, 0,
                            p, p);
        // Skip if below horizon.
        if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && p[2] < 0)
            continue;
        // Skip if not visible.
        convert_coordinates(core->observer, FRAME_OBSERVED, FRAME_VIEW, 0,
                            p, p);
        if (!project(painter.proj,
                     PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE,
                     2, p, p_ndc))
            continue;

        mag = core_get_observed_mag(s->vmag);
        if (debug_show_all) mag = 4.0;
        core_get_point_for_mag(mag, &size, &luminance);
        get_star_color(s->sp, color);
        points[n] = (point_t) {
            .pos = {p[0], p[1], p[2], 0},
            .size = size,
            .color = {color[0], color[1], color[2], luminance},
        };
        if (s->hd)
            make_id(points[n].id, "HD", s->hd);
        else
            make_id(points[n].id, "GAIA", s->gaia);
        n++;
        if (s->vmag <= painter.label_mag_max)
            star_render_name(&painter, s, p_ndc, size, mag);
    }
    paint_points(&painter, n, points, FRAME_VIEW);
    free(points);

end:
    // Test if we should go into higher order tiles.
    // Since for the moment we have two different surveys, keep going
    // until order 3 no matter what, so that we reach the gaia survey.
    if (order < 3 && painter.mag_max > GAIA_MIN_MAG) return 1;
    if (!tile) return 0;
    if (debug_show_all) return 1;
    if (tile->mag_max > painter.mag_max) return 0;
    return 1;
}


static int stars_render(const obj_t *obj, const painter_t *painter_)
{
    stars_t *stars = (stars_t*)obj;
    int nb_tot = 0, nb_loaded = 0;
    painter_t painter = *painter_;

    if (!stars->visible) return 0;
    hips_traverse(USER_PASS(stars, &painter, &nb_tot, &nb_loaded),
                  render_visitor);
    progressbar_report("stars", "Stars", nb_loaded, nb_tot);
    return 0;
}

static int stars_get_visitor(int order, int pix, void *user)
{
    int i;
    struct {
        stars_t     *stars;
        obj_t       *ret;
        int         hd;
        uint64_t    gaia;
    } *d = user;
    tile_t *tile;
    tile = get_tile(d->stars, order, pix, false, NULL);

    // If we are looking for a gaia star the id already gives us the tile.
    if (d->gaia && gaia_index_to_pix(order, d->gaia) != pix)
        return 0;

    // Gaia survey has a min order of 3.
    // XXX: read the survey properties file instead of hard coding!
    if (!tile) return order < 3 ? 1 : 0;

    for (i = 0; i < tile->nb; i++) {
        if (    (d->hd && d->hd == tile->stars[i].hd) ||
                (d->gaia && d->gaia == tile->stars[i].gaia)) {
            d->ret = &star_create(&tile->stars[i])->obj;
            return -1; // Stop the search.
        }
    }
    return 1;
}

static obj_t *stars_get(const obj_t *obj, const char *id, int flags)
{
    int hd = 0, r = 0;
    uint64_t gaia = 0;
    regmatch_t matches[3];

    stars_t *stars = (stars_t*)obj;
    r = regexec(&stars->search_reg, id, 3, matches, 0);
    if (r) return NULL;
    if (id[matches[1].rm_so] == 'h' || id[matches[1].rm_so] == 'H')
        hd = atoi(id + matches[2].rm_so);
    else
        gaia = atoll(id + matches[2].rm_so);

    struct {
        stars_t  *stars;
        obj_t    *ret;
        int      hd;
        uint64_t gaia;
    } d = {.stars=(void*)obj, .hd=hd, .gaia=gaia};

    hips_traverse(&d, stars_get_visitor);
    return d.ret;
}

static obj_t *stars_get_by_nsid(const obj_t *obj, uint64_t nsid)
{
    struct {
        stars_t  *stars;
        obj_t    *ret;
        int      hd;
        uint64_t gaia;
    } d = {.stars=(void*)obj, .gaia=nsid};
    hips_traverse(&d, stars_get_visitor);
    return d.ret;
}

// XXX: we can probably merge this with stars_get_visitor!
static int stars_list_visitor(int order, int pix, void *user)
{
    int i;
    char id[128];
    struct {
        stars_t *stars;
        double max_mag;
        int nb;
        int (*f)(const char *id, void *user);
        void *user;
    } *d = user;
    tile_t *tile;
    tile = get_tile(d->stars, order, pix, false, NULL);
    if (!tile || tile->mag_max <= d->max_mag) return 0;
    for (i = 0; i < tile->nb; i++) {
        if (tile->stars[i].vmag <= d->max_mag) {
            if (tile->stars[i].hd)
                make_id(id, "HD", tile->stars[i].hd);
            else
                make_id(id, "GAIA", tile->stars[i].gaia);
            d->nb++;
            if (d->f && d->f(id, d->user)) return -1; // Stop here.
        }
    }
    return 1;
}

static int stars_list(const obj_t *obj, double max_mag, void *user,
                     int (*f)(const char *id, void *user))
{
    struct {
        stars_t *stars;
        double max_mag;
        int nb;
        int (*f)(const char *id, void *user);
        void *user;
    } d = {.stars=(void*)obj, .max_mag=max_mag, .f=f, .user=user};

    hips_traverse(&d, stars_list_visitor);
    return d.nb;
}

static obj_t *stars_add_res(obj_t *obj, json_value *val,
                            const char *base_path)
{
    const char *type, *path;
    stars_t *stars = (stars_t*)obj;

    type = json_get_attr_s(val, "type");
    if (!type || strcmp(type, "survey") != 0) return NULL;
    val = json_get_attr(val, "survey", json_object);
    if (!val) return NULL;
    type = json_get_attr_s(val, "type");
    if (!type || strcmp(type, "stars") != 0) return NULL;
    if (stars->survey) {
        LOG_E("Only support one star survey");
        return NULL;
    }
    path = json_get_attr_s(val, "path");
    if (!path) return NULL;
    asprintf(&stars->survey, "%s/%s", base_path, path);
    return NULL;
}

static void star_render_name(const painter_t *painter, const star_data_t *s,
                             const double pos[3], double size, double vmag)
{
    const char *greek[] = {"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
                           "λ", "μ", "ν", "ξ", "ο", "π", "ρ", "σ", "τ",
                           "υ", "φ", "χ", "ψ", "ω"};
    int bayer, bayer_n;
    const char *name;
    char tmp[8];
    double label_color[4] = {1, 1, 1, 0.5};

    bayer_get(s->hd, NULL, &bayer, &bayer_n);
    name = skyculture_get_star_name(core->skyculture, s->hd);
    if (bayer == 0 && !name) return;
    if (name) {
        labels_add(name, pos, size, 13, label_color, ANCHOR_AROUND, -vmag);
    } else if (bayer) {
        sprintf(tmp, "%s%.*d", greek[bayer - 1], bayer_n ? 1 : 0, bayer_n);
        labels_add(tmp, pos, size, 13, label_color, ANCHOR_AROUND, -vmag);
    }
}

static void get_star_color(const char sp, double out[3])
{
    int r, g, b;
    switch (sp) {
    case 'O':
        r = 155; g = 176; b = 255; break;
    case 'B':
        r = 170; g = 191; b = 255; break;
    case 'A':
        r = 202; g = 215; b = 255; break;
    case 'F':
        r = 248; g = 247; b = 255; break;
    case 'G':
        r = 255; g = 244; b = 234; break;
    case 'K':
        r = 255; g = 210; b = 161; break;
    case 'M':
        r = 255; g = 204; b = 111; break;
    default:
        r = 255, g = 255, b = 255; break;
    }
    out[0] = r / 255.0;
    out[1] = g / 255.0;
    out[2] = b / 255.0;
}

OBJ_REGISTER(stars_klass);


/******* TESTS **********************************************************/
#if COMPILE_TESTS

static void test_create_from_json(void)
{
    obj_t *star;
    double vmag;
    const char *data =
        "{"
        "   \"model_data\": {"
        "       \"Vmag\": 5.153,"
        "       \"de\": 0.48644192,"
        "       \"plx\": 13.99,"
        "       \"pm_de\": -20.5,"
        "       \"ra\": 309.85371232,"
        "       \"pm_ra\": 101.95"
        "   }"
        "}";
    star = obj_create_str("star", NULL, NULL, data);
    assert(star);
    obj_update(star, core->observer, 0);
    obj_get_attr(star, "vmag", "f", &vmag);
    assert(fabs(vmag - 5.153) < 0.0001);
    obj_delete(star);
}
TEST_REGISTER(NULL, test_create_from_json, TEST_AUTO);

#endif
