/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include "hip.h"
#include "ini.h"

#include <regex.h>
#include <zlib.h>

#define URL_MAX_SIZE 4096

static const double LABEL_SPACING = 4;

// Indices of the two surveys we use.
enum {
    SURVEY_DEFAULT  = 0,
    SURVEY_GAIA     = 1,
};

typedef struct stars stars_t;
typedef struct {
    uint64_t oid;
    uint64_t gaia;  // Gaia source id (0 if none)
    uint32_t tyc;   // Tycho2 id.
    int     hip;    // HIP number.
    int     hd;     // HD number.
    float   vmag;
    float   ra;     // ICRS RA  J2000.0 (rad)
    float   de;     // ICRS Dec J2000.0 (rad)
    float   pra;    // RA proper motion (rad/year)
    float   pde;    // Dec proper motion (rad/year)
    float   plx;    // Parallax (arcsec)
    float   bv;
    float   illuminance; // (lux)
    // Normalized Astrometric direction.
    double  pos[3];
    double  distance;    // Distance in AU
    // List of extra names, separated by '\0', terminated by two '\0'.
    char    *names;
} star_data_t;

// A single star.
typedef struct {
    obj_t       obj;
    star_data_t data;
} star_t;

/*
 * Type: stars_t
 * The module object.
 */
struct stars {
    obj_t           obj;
    regex_t         search_reg;

    // The stars module supports up to two surveys.
    // One for the bundled stars, and one for the online gaia survey.
    struct {
        stars_t *stars;
        hips_t  *hips;
        char    url[URL_MAX_SIZE - 256];
        int     min_order;
        double  min_vmag; // Don't render survey below this mag.
    } surveys[2];

    bool            visible;
};

/*
 * Type: tile_t
 * Custom tile structure for the stars hips survey.
 */
typedef struct tile {
    int         flags;
    double      mag_min;
    double      mag_max;
    double      illuminance; // Totall illuminance (lux).
    int         nb;
    star_data_t *sources;
} tile_t;

static uint64_t pix_to_nuniq(int order, int pix)
{
    return pix + 4 * (1L << (2 * order));
}

static void nuniq_to_pix(uint64_t nuniq, int *order, int *pix)
{
    *order = log2(nuniq / 4) / 2;
    *pix = nuniq - 4 * (1 << (2 * (*order)));
}

static double illuminance_for_vmag(double vmag)
{
    /*
     * S = m + 2.5 * log10(A)         | S: vmag/arcmin², A: arcmin²
     * L = 10.8e4 * 10^(-0.4 * S)     | S: vmag/arcmin², L: cd/m²
     * E = L * A                      | E: lux (= cd.sr/m²), A: sr, L: cd/m²
     *
     * => E = 10.8e4 / R2AS^2 * 10^(-0.4 * m)
     */
    return 10.8e4 / (ERFA_DR2AS * ERFA_DR2AS) * pow(10, -0.4 * vmag);
}

/*
 * Precompute values about the star position to make rendering faster.
 * Parameters:
 *   pls    - Parallax (arcseconds).
 */
static void compute_pv(double ra, double de,
                       double pra, double pde, double plx, star_data_t *s)
{
    int r;
    double pv[2][3];
    if (isnan(plx)) plx = 0;
    r = eraStarpv(ra, de, pra, pde, plx, 0, pv);
    if (r & (2 | 4)) LOG_W("Wrong star coordinates");
    if (r & 1) {
        s->distance = NAN;
        vec3_normalize(pv[0], s->pos);
    } else {
        s->distance = vec3_norm(pv[0]);
        vec3_mul(1.0 / s->distance, pv[0], s->pos);
    }
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
        d->illuminance = illuminance_for_vmag(d->vmag);
        compute_pv(d->ra, d->de, d->pra, d->pde, d->plx, d);
    }
    return 0;
}

static int star_update(obj_t *obj, const observer_t *obs, double dt)
{
    star_t *star = (star_t*)obj;
    eraASTROM *astrom = (void*)&obs->astrom;
    eraPmpx(star->data.ra, star->data.de, 0, 0, star->data.plx, 0,
            astrom->pmt, astrom->eb, obj->pvo[0]);
    assert(!isnan(obj->pvo[0][0]));

    obj->pvo[0][3] = 0.0;
    obj->vmag = star->data.vmag;
    // We need to renormalize
    vec3_normalize(obj->pvo[0], obj->pvo[0]);
    // Set speed to 0.
    obj->pvo[1][0] = obj->pvo[1][1] = obj->pvo[1][2] = 0;
    astrometric_to_apparent(obs, obj->pvo[0], true, obj->pvo[0]);
    return 0;
}

static void star_render_name(const painter_t *painter, const star_data_t *s,
                             const double pos[3], double size, double vmag,
                             double color[3])
{
    const char *greek[] = {"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
                           "λ", "μ", "ν", "ξ", "ο", "π", "ρ", "σ", "τ",
                           "υ", "φ", "χ", "ψ", "ω"};
    int bayer, bayer_n;
    const char *name = NULL;
    double label_color[4] = {color[0], color[1], color[2], 0.5};
    static const double white[4] = {1, 1, 1, 1};
    const bool selected = core->selection && s->oid == core->selection->oid;
    int label_flags = LABEL_AROUND;
    char buf[128];
    obj_t *skycultures;

    //if (!s->hip) return;
    if (selected) {
        vec4_copy(white, label_color);
        label_flags |= LABEL_BOLD;
    }
    size += LABEL_SPACING;

    // First try the current skyculture name.
    skycultures = core_get_module("skycultures");
    name = skycultures_get_name(skycultures, s->oid, buf);

    // If no name, try a HD number.
    if (!name && selected) {
        if (s->hd) {
            sprintf(buf, "HD %d", s->hd);
            name = buf;
        }
    }
    if (name) {
        labels_add(sys_translate("star", name),
                   pos, size, 13, label_color, 0, label_flags, -vmag, s->oid);
        return;
    }

    // Still no name, maybe we can show a bayer id.
    if (painter->flags & PAINTER_SHOW_BAYER_LABELS) {
        bayer_get(s->hip, NULL, &bayer, &bayer_n);
        if (bayer) {
            sprintf(buf, "%s%.*d", greek[bayer - 1], bayer_n ? 1 : 0, bayer_n);
            labels_add(buf, pos, size, 13, label_color, 0,
                       label_flags, -vmag, s->oid);
        }
    }
}

// Render a single star.
// This should be used only for stars that have been manually created.
static int star_render(const obj_t *obj, const painter_t *painter_)
{
    // XXX: the code is almost the same as the inner loop in stars_render.
    star_t *star = (star_t*)obj;
    const star_data_t *s = &star->data;
    double ri, di, aob, zob, hob, dob, rob;
    double p[4], size, luminance;
    double color[3];
    painter_t painter = *painter_;
    point_t point;
    const bool selected = core->selection && obj->oid == core->selection->oid;

    // XXX: Use convert_coordinates instead!
    eraAtciq(s->ra, s->de, s->pra, s->pde, s->plx, 0,
            &painter.obs->astrom, &ri, &di);
    eraAtioq(ri, di, &painter.obs->astrom,
            &aob, &zob, &hob, &dob, &rob);
    if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && zob > 90 * DD2R)
        return 0;
    eraS2c(aob, 90 * DD2R - zob, p);
    core_get_point_for_mag(s->vmag, &size, &luminance);
    bv_to_rgb(s->bv, color);
    point = (point_t) {
        .pos = {p[0], p[1], p[2]},
        .size = size,
        .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                  luminance * 255},
        .oid = s->oid,
    };
    paint_points(&painter, 1, &point, FRAME_OBSERVED);

    if (selected || (s->vmag <= painter.hints_limit_mag - 4.0)) {
        convert_frame(painter.obs, FRAME_OBSERVED, FRAME_VIEW,
                          true, p, p);
        if (project(painter.proj,
                    PROJ_ALREADY_NORMALIZED | PROJ_TO_WINDOW_SPACE, 2, p, p))
            star_render_name(&painter, s, p, size, s->vmag, color);
    }
    return 0;
}


void star_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    star_t *star = (star_t*)obj;
    const star_data_t *s = &star->data;
    const char *names = s->names;
    char buf[128];
    char cat[128] = {};

    if (s->hd) {
        sprintf(buf, "%d", s->hd);
        f(obj, user, "HD", buf);
    }
    if (s->hip) {
        sprintf(buf, "%d", s->hip);
        f(obj, user, "HIP", buf);
    }
    if (s->gaia) {
        sprintf(buf, "%" PRId64, s->gaia);
        f(obj, user, "GAIA", buf);
        sprintf(buf, "%016" PRIx64, s->gaia);
        f(obj, user, "NSID", buf);
    }
    while (names && *names) {
        strncpy(cat, names, sizeof(cat) - 1);
        if (!strchr(cat, ' ')) { // No catalog.
            f(obj, user, "", cat);
        } else {
            *strchr(cat, ' ') = '\0';
            f(obj, user, cat, names + strlen(cat) + 1);
        }
        names += strlen(names) + 1;
    }
}

static json_value *star_get_distance(obj_t *obj, const attribute_t *attr,
                                      const json_value *args)
{
    star_t *star = (star_t*)obj;
    return args_value_new("f", "dist", star->data.distance);
}

static star_t *star_create(const star_data_t *data)
{
    star_t *star;
    star = (star_t*)obj_create("star", NULL, NULL, NULL);
    strcpy(star->obj.type, "*");
    star->data = *data;
    star->obj.nsid = star->data.gaia;
    star->obj.oid = star->data.oid;
    star_update(&star->obj, core->observer, 0);
    return star;
}

// Used by the cache.
static int del_tile(void *data)
{
    int i;
    tile_t *tile = data;
    for (i = 0; i < tile->nb; i++) free(tile->sources[i].names);
    free(tile->sources);
    free(tile);
    return 0;
}

static int star_data_cmp(const void *a, const void *b)
{
    return cmp(((const star_data_t*)a)->vmag, ((const star_data_t*)b)->vmag);
}

static int on_file_tile_loaded(const char type[4],
                               const void *data, int size, void *user)
{
    int version, nb, data_ofs = 0, row_size, flags, i, j, order, pix;
    double vmag, gmag, ra, de, pra, pde, plx, bv;
    char ids[256] = {};
    typeof(((stars_t*)0)->surveys[0]) *survey = USER_GET(user, 0);
    tile_t **out = USER_GET(user, 1); // Receive the tile.
    tile_t *tile;
    void *table_data;
    star_data_t *s;

    // All the columns we care about in the source file.
    eph_table_column_t columns[] = {
        {"gaia", 'Q'},
        {"hip",  'i'},
        {"hd",   'i'},
        {"tyc",  'i'},
        {"vmag", 'f', EPH_VMAG},
        {"gmag", 'f', EPH_VMAG},
        {"ra",   'f', EPH_RAD},
        {"de",   'f', EPH_RAD},
        {"plx",  'f', EPH_ARCSEC},
        {"pra",  'f', EPH_RAD_PER_YEAR},
        {"pde",  'f', EPH_RAD_PER_YEAR},
        {"bv",   'f'},
        {"ids",  's', .size=256},
    };

    *out = NULL;
    // Only support STAR and GAIA chunks.  Ignore anything else.
    if (strncmp(type, "STAR", 4) != 0 &&
        strncmp(type, "GAIA", 4) != 0) return 0;

    eph_read_tile_header(data, size, &data_ofs, &version, &order, &pix);
    assert(version >= 3); // No more support for old style format.
    nb = eph_read_table_header(version, data, size,
                               &data_ofs, &row_size, &flags,
                               ARRAY_SIZE(columns), columns);
    if (nb < 0) {
        LOG_E("Cannot parse file");
        return -1;
    }

    table_data = eph_read_compressed_block(data, size, &data_ofs, &size);
    if (!table_data) {
        LOG_E("Cannot get table data");
        return -1;
    }
    data_ofs = 0;
    if (flags & 1) eph_shuffle_bytes(table_data, row_size, nb);

    tile = calloc(1, sizeof(*tile));
    tile->sources = calloc(nb, sizeof(*tile->sources));
    tile->mag_min = DBL_MAX;
    tile->mag_max = -DBL_MAX;

    for (i = 0; i < nb; i++) {
        s = &tile->sources[tile->nb];
        eph_read_table_row(
                table_data, size, &data_ofs, ARRAY_SIZE(columns), columns,
                &s->gaia, &s->hip, &s->hd, &s->tyc, &vmag, &gmag,
                &ra, &de, &plx, &pra, &pde, &bv, ids);
        assert(!isnan(ra));
        assert(!isnan(de));
        if (isnan(vmag)) vmag = gmag;
        assert(!isnan(vmag));

        if (!isnan(survey->min_vmag) && (vmag < survey->min_vmag)) continue;
        s->vmag = vmag;
        s->ra = ra;
        s->de = de;
        s->pra = pra;
        s->pde = pde;
        s->plx = plx;
        s->bv = isnan(bv) ? 0 : bv;
        s->oid = s->hip ? oid_create("HIP", s->hip) :
                 s->tyc ? oid_create("TYC", s->tyc) :
                 s->gaia;
        assert(s->oid);
        compute_pv(ra, de, pra, pde, plx, s);
        s->illuminance = illuminance_for_vmag(vmag);

        // Turn '|' separated ids into '\0' separated values.
        if (*ids) {
            s->names = calloc(1, 2 + strlen(ids));
            for (j = 0; ids[j]; j++)
                s->names[j] = ids[j] != '|' ? ids[j] : '\0';
        }

        tile->illuminance += illuminance_for_vmag(vmag);
        tile->mag_min = min(tile->mag_min, vmag);
        tile->mag_max = max(tile->mag_max, vmag);
        tile->nb++;
    }

    // Sort the data by vmag, so that we can early exit during render.
    qsort(tile->sources, tile->nb, sizeof(*tile->sources), star_data_cmp);
    free(table_data);

    *out = tile;
    return 0;
}

static const void *stars_create_tile(
        void *user, int order, int pix, void *data, int size,
        int *cost, int *transparency)
{
    tile_t *tile;
    typeof(((stars_t*)0)->surveys[0]) *survey = user;
    eph_load(data, size, USER_PASS(survey, &tile), on_file_tile_loaded);
    if (tile) *cost = tile->nb * sizeof(*tile->sources);
    return tile;
}

static int stars_init(obj_t *obj, json_value *args)
{
    stars_t *stars = (stars_t*)obj;
    hips_settings_t survey_settings = {
        .create_tile = stars_create_tile,
        .delete_tile = del_tile,
    };

    stars->visible = true;

    regcomp(&stars->search_reg, "(hd|hip|gaia) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);

    // Online gaia survey at index 1.
    survey_settings.user = &stars->surveys[SURVEY_GAIA];
    sprintf(stars->surveys[SURVEY_GAIA].url,
            "https://data.stellarium.org/surveys/gaia_dr2_v2");
    stars->surveys[SURVEY_GAIA].hips = hips_create(
            stars->surveys[SURVEY_GAIA].url, 0, &survey_settings);
    stars->surveys[SURVEY_GAIA].min_order = 3; // Hardcoded!
    return 0;
}

static tile_t *get_tile(stars_t *stars, int survey, int order, int pix,
                        bool *loading_complete)
{
    int code, flags = 0;
    tile_t *tile;
    // Immediate load of the level 0 stars (they are needed for the
    // constellations).  The other tiles can be loaded in a thread.
    if (order > 0) flags |= HIPS_LOAD_IN_THREAD;
    if (!stars->surveys[survey].hips) return NULL;
    tile = hips_get_tile(stars->surveys[survey].hips,
                         order, pix, flags, &code);
    if (loading_complete) *loading_complete = (code != 0);
    return tile;
}

bool debug_stars_show_all = false;

static int render_visitor(int order, int pix, void *user)
{
    PROFILE(stars_render_visitor, PROFILE_AGGREGATE);
    stars_t *stars = USER_GET(user, 0);
    int survey = *((int*)USER_GET(user, 1));
    painter_t painter = *(const painter_t*)USER_GET(user, 2);
    int *nb_tot = USER_GET(user, 3);
    int *nb_loaded = USER_GET(user, 4);
    double *illuminance = USER_GET(user, 5);
    tile_t *tile;
    int i, n = 0;
    star_data_t *s;
    double p[4], p_win[4], size, luminance;
    double color[3];
    bool loaded;
    bool selected;

    // Early exit if the tile is clipped.
    if (painter_is_tile_clipped(&painter, FRAME_ASTROM, order, pix, true))
        return 0;
    if (order < stars->surveys[survey].min_order) return 1;

    (*nb_tot)++;
    tile = get_tile(stars, survey, order, pix, &loaded);
    if (loaded) (*nb_loaded)++;

    if (!tile) goto end;
    if (tile->mag_min > painter.stars_limit_mag) goto end;

    point_t *points = malloc(tile->nb * sizeof(*points));
    for (i = 0; i < tile->nb; i++) {
        s = &tile->sources[i];
        if (s->vmag > painter.stars_limit_mag) break;
        if (!cap_contains_vec3(painter.viewport_cap, s->pos))
            continue;
        // Skip if below horizon.
        if (painter.flags & PAINTER_HIDE_BELOW_HORIZON &&
                !cap_contains_vec3(painter.sky_cap, s->pos))
            continue;

        // Compute star observed and screen pos.
        vec3_copy(s->pos, p);
        p[3] = 0;
        convert_frame(painter.obs, FRAME_ASTROM, FRAME_VIEW, true, p, p);
        if (!project(painter.proj, PROJ_TO_WINDOW_SPACE |
                     PROJ_ALREADY_NORMALIZED, 2, p, p_win))
            continue;

        (*illuminance) += s->illuminance;
        core_get_point_for_mag(s->vmag, &size, &luminance);
        bv_to_rgb(s->bv, color);
        points[n] = (point_t) {
            .pos = {p_win[0], p_win[1], 0, 0},
            .size = size,
            .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                      luminance * 255},
            .oid = s->oid,
            .hint = pix_to_nuniq(order, pix),
        };
        n++;
        selected = core->selection && s->oid == core->selection->oid;
        if (selected || (s->vmag <= painter.hints_limit_mag - 4.0 &&
            survey != SURVEY_GAIA))
            star_render_name(&painter, s, p_win, size, s->vmag, color);
    }
    paint_points(&painter, n, points, FRAME_WINDOW);
    free(points);

end:
    // Test if we should go into higher order tiles.
    if (!tile || (tile->mag_max > painter.stars_limit_mag))
        return 0;
    return 1;
}


static int stars_render(const obj_t *obj, const painter_t *painter_)
{
    PROFILE(stars_render, 0);
    stars_t *stars = (stars_t*)obj;
    int i, nb_tot = 0, nb_loaded = 0;
    double illuminance = 0; // Totall illuminance
    painter_t painter = *painter_;

    if (!stars->visible) return 0;

    for (i = 0; i < ARRAY_SIZE(stars->surveys); i++) {
        if (!stars->surveys[i].hips) break;
        // Don't even traverse if the min vmag of the survey is higher than
        // the max visible vmag.
        if (    !isnan(stars->surveys[i].min_vmag) &&
                stars->surveys[i].min_vmag > painter.stars_limit_mag)
            continue;
        hips_traverse(USER_PASS(stars, &i, &painter,
                      &nb_tot, &nb_loaded, &illuminance),
                      render_visitor);
    }

    /* Ad-hoc formula to adjust tonemapping when many stars are visible.
     * I think the illuminance computation is correct, but should we use
     * core_report_illuminance_in_fov? */
    illuminance *= core->telescope.light_grasp;
    core_report_luminance_in_fov(illuminance, false);

    progressbar_report("stars", "Stars", nb_loaded, nb_tot, -1);
    return 0;
}

static int stars_get_visitor(int order, int pix, void *user)
{
    int i, p;
    bool is_gaia;
    struct {
        stars_t     *stars;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } *d = user;
    tile_t *tile = NULL;

    is_gaia = d->cat == 2 || (d->cat == 3 && oid_is_gaia(d->n));

    // If we are looking for a gaia star the id already gives us the tile.
    if (is_gaia) {
        if (gaia_index_to_pix(order, d->n) != pix)
            return 0;
    }

    // For HIP lookup, we can use the bundled hip -> pix data if available.
    if (d->cat == 3 && oid_is_catalog(d->n, "HIP")) {
        p = hip_get_pix(oid_get_index(d->n), order);
        if ((p != -1) && (p != pix)) return 0;
    }

    // Try both surveys (bundled and gaia).
    for (i = 0; !tile && i < 2; i++) {
        tile = get_tile(d->stars, i, order, pix, NULL);
    }

    // Gaia survey has a min order of 3.
    // XXX: read the survey properties file instead of hard coding!
    if (!tile) return order < 3 ? 1 : 0;
    for (i = 0; i < tile->nb; i++) {
        if (    (d->cat == 0 && tile->sources[i].hip == d->n) ||
                (d->cat == 1 && tile->sources[i].hd  == d->n) ||
                (d->cat == 2 && tile->sources[i].gaia == d->n) ||
                (d->cat == 3 && tile->sources[i].oid  == d->n)) {
            d->ret = &star_create(&tile->sources[i])->obj;
            return -1; // Stop the search.
        }
    }
    return 1;
}

static obj_t *stars_get(const obj_t *obj, const char *id, int flags)
{
    int r, cat;
    uint64_t n = 0;
    regmatch_t matches[3];

    stars_t *stars = (stars_t*)obj;
    r = regexec(&stars->search_reg, id, 3, matches, 0);
    if (r) return NULL;
    n = strtoull(id + matches[2].rm_so, NULL, 10);
    if (strncasecmp(id, "hip", 3) == 0) cat = 0;
    if (strncasecmp(id, "hd", 2) == 0) cat = 1;
    if (strncasecmp(id, "gaia", 4) == 0) cat = 2;

    struct {
        stars_t  *stars;
        obj_t    *ret;
        int      cat;
        uint64_t n;
    } d = {.stars=(void*)obj, .cat=cat, .n=n};

    hips_traverse(&d, stars_get_visitor);
    return d.ret;
}

static obj_t *stars_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    int order, pix, i;
    stars_t *stars = (void*)obj;
    tile_t *tile = NULL;

    struct {
        stars_t  *stars;
        obj_t    *ret;
        int      cat;
        uint64_t n;
    } d = {.stars=(void*)obj, .cat=3, .n=oid};

    if (!hint) {
        if (    !oid_is_catalog(oid, "HIP") &&
                !oid_is_catalog(oid, "TYC") &&
                !oid_is_gaia(oid)) return NULL;
        hips_traverse(&d, stars_get_visitor);
        return d.ret;
    }

    // Get tile from hint (as nuniq).
    nuniq_to_pix(hint, &order, &pix);
    // Try both surveys (bundled and gaia).
    for (i = 0; !tile && i < 2; i++) {
        tile = get_tile(stars, i, order, pix, NULL);
    }
    if (!tile) return NULL;
    for (i = 0; i < tile->nb; i++) {
        if (tile->sources[i].oid == oid)
            return (obj_t*)star_create(&tile->sources[i]);
    }
    return NULL;
}

// XXX: we can probably merge this with stars_get_visitor!
static int stars_list_visitor(int order, int pix, void *user)
{
    int i, r;
    star_t *star = NULL;
    struct {
        stars_t *stars;
        double max_mag;
        int nb;
        int (*f)(void *user, obj_t *obj);
        void *user;
    } *d = user;
    tile_t *tile;
    tile = get_tile(d->stars, 0, order, pix, NULL);
    if (!tile || tile->mag_max <= d->max_mag) return 0;
    for (i = 0; i < tile->nb; i++) {
        if (tile->sources[i].vmag > d->max_mag) continue;
        d->nb++;
        if (!d->f) continue;
        star = star_create(&tile->sources[i]);
        r = d->f(d->user, (obj_t*)star);
        obj_release((obj_t*)star);
        if (r) break;
    }
    return 1;
}

static int stars_list(const obj_t *obj, observer_t *obs,
                      double max_mag, uint64_t hint, void *user,
                      int (*f)(void *user, obj_t *obj))
{
    int order, pix, i, r;
    bool complete;
    tile_t *tile;
    stars_t *stars = (void*)obj;
    star_t *star;

    struct {
        stars_t *stars;
        double max_mag;
        int nb;
        int (*f)(void *user, obj_t *obj);
        void *user;
    } d = {.stars=(void*)obj, .max_mag=max_mag, .f=f, .user=user};

    if (!hint) {
        hips_traverse(&d, stars_list_visitor);
        return 0;
    }

    // Get tile from hint (as nuniq).
    nuniq_to_pix(hint, &order, &pix);
    tile = get_tile(stars, 0, order, pix, &complete);
    if (!tile) {
        if (!complete) return OBJ_AGAIN; // Try again later.
        return -1;
    }
    for (i = 0; i < tile->nb; i++) {
        if (!f) continue;
        star = star_create(&tile->sources[i]);
        r = f(user, (obj_t*)star);
        obj_release((obj_t*)star);
        if (r) break;
    }
    return 0;
}

static int stars_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    stars_t *stars = (stars_t*)obj;
    const char *args_type, *max_vmag_str;
    hips_settings_t survey_settings = {
        .create_tile = stars_create_tile,
        .delete_tile = del_tile,
    };

    if (!type || !args || strcmp(type, "hips")) return 1;
    args_type = json_get_attr_s(args, "type");
    if (!args_type || strcmp(args_type, "stars")) return 1;

    // For the moment we only support one stars source in addition to the
    // only gaia survey.
    assert(!stars->surveys[SURVEY_DEFAULT].hips);
    survey_settings.user = &stars->surveys[0];
    sprintf(stars->surveys[SURVEY_DEFAULT].url, "%s", url);
    stars->surveys[SURVEY_DEFAULT].hips = hips_create(
            stars->surveys[SURVEY_DEFAULT].url, 0, &survey_settings);
    stars->surveys[SURVEY_DEFAULT].min_vmag = NAN;

    // Tell online gaia survey to only start after the vmag for this survey.
    max_vmag_str = json_get_attr_s(args, "max_vmag");
    if (max_vmag_str)
        stars->surveys[SURVEY_GAIA].min_vmag = atof(max_vmag_str);

    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t star_klass = {
    .id         = "star",
    .init       = star_init,
    .size       = sizeof(star_t),
    .update     = star_update,
    .render     = star_render,
    .get_designations = star_get_designations,
    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        { "distance", "f", .hint = "dist", .fn = star_get_distance,
          .desc = "Distance (AU)." },
        PROPERTY("radec"),
        PROPERTY("vmag"),
        PROPERTY("type"),
        {},
    },
};
OBJ_REGISTER(star_klass)

static obj_klass_t stars_klass = {
    .id             = "stars",
    .size           = sizeof(stars_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init           = stars_init,
    .render         = stars_render,
    .get            = stars_get,
    .get_by_oid     = stars_get_by_oid,
    .list           = stars_list,
    .add_data_source = stars_add_data_source,
    .render_order   = 20,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(stars_t, visible)),
        {},
    },
};
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
    obj_release(star);
}
TEST_REGISTER(NULL, test_create_from_json, TEST_AUTO);

#endif
