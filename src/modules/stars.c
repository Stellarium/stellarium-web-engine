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
    char    type[4];
    int     hip;    // HIP number.
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

// Turn a json array of string into a '\0' separated C string.
// Move this in utils?
static char *parse_json_names(json_value *names)
{
    int i;
    json_value *jstr;
    UT_string ret;
    utstring_init(&ret);
    for (i = 0; i < names->u.array.length; i++) {
        jstr = names->u.array.values[i];
        if (jstr->type != json_string) continue; // Not normal!
        utstring_bincpy(&ret, jstr->u.string.ptr, jstr->u.string.length + 1);
    }
    utstring_bincpy(&ret, "", 1); // Add extra '\0' at the end.
    return utstring_body(&ret);
}

static int star_init(obj_t *obj, json_value *args)
{
    // Support creating a star using noctuasky model data json values.
    star_t *star = (star_t*)obj;
    json_value *model, *names;
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
        d->illuminance = core_mag_to_illuminance(d->vmag);
        compute_pv(d->ra, d->de, d->pra, d->pde, d->plx, d);
    }

    names = json_get_attr(args, "names", json_array);
    if (names)
        d->names = parse_json_names(names);
    return 0;
}

// Return position and velocity in ICRF with origin on observer (AU).
static int star_get_pvo(const obj_t *obj, const observer_t *obs,
                        double pvo[2][4])
{
    star_data_t *s = &((star_t*)obj)->data;
    double plx = s->plx, astro_pv[3];

    if (isnan(plx)) plx = 0;
    eraPmpx(s->ra, s->de, 0, 0, plx, 0, obs->astrom.pmt, obs->astrom.eb,
            astro_pv);
    vec3_normalize(astro_pv, astro_pv);
    astrometric_to_apparent(obs, astro_pv, true, pvo[0]);
    pvo[0][3] = 0.0;
    pvo[1][0] = pvo[1][2] = pvo[1][3] = 0.0;
    return 0;
}

static int star_get_info(const obj_t *obj, const observer_t *obs, int info,
                         void *out)
{
    star_t *star = (star_t*)obj;
    switch (info) {
    case INFO_PVO:
        star_get_pvo(obj, obs, out);
        return 0;
    case INFO_VMAG:
        *(double*)out = star->data.vmag;
        return 0;
    case INFO_DISTANCE:
        *(double*)out = star->data.distance;
        return 0;
    case INFO_PARALLAX:
        *(double*)out = star->data.plx * 1000;
        return 0;
    default:
        return 1;
    }
}

static void star_render_name(const painter_t *painter, const star_data_t *s,
                             int frame, const double pos[3], double radius,
                             double vmag, double color[3])
{
    const char *greek[] = {"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
                           "λ", "μ", "ν", "ξ", "ο", "π", "ρ", "σ", "τ",
                           "υ", "φ", "χ", "ψ", "ω"};
    int bayer, bayer_n;
    const char *name = NULL;
    double label_color[4] = {color[0], color[1], color[2], 0.5};
    static const double white[4] = {1, 1, 1, 1};
    const bool selected = core->selection && s->oid == core->selection->oid;
    int effects = TEXT_FLOAT;
    char buf[128];
    char cst[5];
    obj_t *skycultures;

    //if (!s->hip) return;
    if (selected) {
        vec4_copy(white, label_color);
        effects = TEXT_BOLD;
    }
    radius += LABEL_SPACING;

    // Display the current skyculture's star name, but only
    // for bright stars (mag < 3) or when very zoomed.
    // Names for fainter stars tend to be suspiscious, and just
    // pollute the screen space.
    // For those, we rather display bayer name below.
    if (s->vmag < max(3, painter->hints_limit_mag - 8.0)) {
        skycultures = core_get_module("skycultures");
        name = skycultures_get_name(skycultures, s->oid, buf);
    }

    if (!name && selected) {
        bayer_get(s->hip, cst, &bayer, &bayer_n);
        if (bayer) {
            snprintf(buf, sizeof(buf), "%s%.*d %s",
                     greek[bayer - 1], bayer_n ? 1 : 0, bayer_n, cst);
            name = buf;
        }
    }

    if (name) {
        labels_add_3d(sys_translate("skyculture", name), frame, pos, true,
                      radius, FONT_SIZE_BASE, label_color, 0, 0,
                      effects, -vmag, s->oid);
        return;
    }

    // Still no name, maybe we can show a bayer id.
    bayer_get(s->hip, NULL, &bayer, &bayer_n);
    if (bayer) {
        snprintf(buf, sizeof(buf), "%s%.*d",
                 greek[bayer - 1], bayer_n ? 1 : 0, bayer_n);
        labels_add_3d(buf, frame, pos, true, radius, FONT_SIZE_BASE,
                      label_color, 0, 0, effects, -vmag, s->oid);
    }
}

// Render a single star.
// This should be used only for stars that have been manually created.
static int star_render(const obj_t *obj, const painter_t *painter_)
{
    // XXX: the code is almost the same as the inner loop in stars_render.
    star_t *star = (star_t*)obj;
    const star_data_t *s = &star->data;
    double pvo[2][4], p[2], size, luminance;
    double color[3];
    painter_t painter = *painter_;
    point_t point;
    const bool selected = core->selection && obj->oid == core->selection->oid;

    obj_get_pvo(obj, painter.obs, pvo);
    if (!painter_project(painter_, FRAME_ICRF, pvo[0], true, true, p))
        return 0;

    core_get_point_for_mag(s->vmag, &size, &luminance);
    bv_to_rgb(s->bv, color);

    point = (point_t) {
        .pos = {p[0], p[1]},
        .size = size,
        .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                  luminance * 255},
        .oid = s->oid,
    };
    paint_2d_points(&painter, 1, &point);

    if (selected || (s->vmag <= painter.hints_limit_mag - 4.0)) {
        star_render_name(&painter, s, FRAME_ICRF, pvo[0], size,
                s->vmag, color);
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
    const char *name;
    char buf[128];
    char cat[128] = {};
    obj_t *skycultures;

    /*
     * Now the stars designation are supposed to be all the in 'names'
     * attributes.  For the bundled survey that doesn't have it, we still
     * do it manually.
     */
    if (!names) {
        skycultures = core_get_module("skycultures");
        name = skycultures_get_name(skycultures, obj->oid, buf);
        if (name) f(obj, user, "NAME", name);
        if (s->hip) {
            snprintf(buf, sizeof(buf), "%d", s->hip);
            f(obj, user, "HIP", buf);
        }
    }

    while (names && *names) {
        strncpy(cat, names, sizeof(cat));
        if (!strchr(cat, ' ')) { // No catalog.
            f(obj, user, "", cat);
        } else {
            *strchr(cat, ' ') = '\0';
            f(obj, user, cat, names + strlen(cat) + 1);
        }
        names += strlen(names) + 1;
    }
    if (s->gaia) {
        snprintf(buf, sizeof(buf), "%" PRId64, s->gaia);
        f(obj, user, "GAIA", buf);
    }
}

static star_t *star_create(const star_data_t *data)
{
    star_t *star;
    star = (star_t*)obj_create("star", NULL, NULL);
    strncpy(star->obj.type, data->type, 4);
    star->data = *data;
    star->obj.oid = star->data.oid;
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
        {"type", 's', .size=4},
        {"gaia", 'Q'},
        {"hip",  'i'},
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
                s->type, &s->gaia, &s->hip, &s->tyc, &vmag, &gmag,
                &ra, &de, &plx, &pra, &pde, &bv, ids);
        assert(!isnan(ra));
        assert(!isnan(de));
        if (isnan(vmag)) vmag = gmag;
        assert(!isnan(vmag));

        if (!isnan(survey->min_vmag) && (vmag < survey->min_vmag)) continue;
        if (!*s->type) strncpy(s->type, "*", 4); // Default type.
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
        s->illuminance = core_mag_to_illuminance(vmag);

        // Turn '|' separated ids into '\0' separated values.
        if (*ids) {
            s->names = calloc(1, 2 + strlen(ids));
            for (j = 0; ids[j]; j++)
                s->names[j] = ids[j] != '|' ? ids[j] : '\0';
        }

        tile->illuminance += s->illuminance;
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
    tile_t *tile = NULL;
    typeof(((stars_t*)0)->surveys[0]) *survey = user;
    eph_load(data, size, USER_PASS(survey, &tile), on_file_tile_loaded);
    if (tile) *cost = tile->nb * sizeof(*tile->sources);
    return tile;
}

static int stars_init(obj_t *obj, json_value *args)
{
    stars_t *stars = (stars_t*)obj;
    stars->visible = true;
    regcomp(&stars->search_reg, "(hip|gaia) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);
    return 0;
}

/*
 * Function: get_tile
 * Load and return a tile.
 *
 * Parameters:
 *   stars  - The stars module.
 *   survey - The survey index (SURVEY_DEFAULT or SURVEY_GAIA).
 *   order  - Healpix order.
 *   pix    - Healpix pix.
 *   sync   - If set, don't load in a thread.  This will block the main
 *            loop so should be avoided.
 *   code   - http return code (0 if still loading).
 */
static tile_t *get_tile(stars_t *stars, int survey, int order, int pix,
                        bool sync, int *code)
{
    int flags = 0;
    tile_t *tile;
    assert(code);
    if (!sync) flags |= HIPS_LOAD_IN_THREAD;
    if (!stars->surveys[survey].hips) {
        *code = 0;
        return NULL;
    }
    tile = hips_get_tile(stars->surveys[survey].hips,
                         order, pix, flags, code);
    return tile;
}

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
    int i, n = 0, code;
    star_data_t *s;
    double p_win[4], size, luminance;
    double color[3];
    double limit_mag = min(painter.stars_limit_mag, painter.hard_limit_mag);
    bool selected;

    // Early exit if the tile is clipped.
    if (painter_is_healpix_clipped(&painter, FRAME_ASTROM, order, pix, true))
        return 0;
    if (order < stars->surveys[survey].min_order) return 1;

    (*nb_tot)++;
    tile = get_tile(stars, survey, order, pix, false, &code);
    if (code) (*nb_loaded)++;

    if (!tile) goto end;
    if (tile->mag_min > limit_mag) goto end;

    point_t *points = malloc(tile->nb * sizeof(*points));
    for (i = 0; i < tile->nb; i++) {
        s = &tile->sources[i];
        if (s->vmag > limit_mag) break;

        if (!painter_project(&painter, FRAME_ASTROM, s->pos, true, true, p_win))
            continue;

        (*illuminance) += s->illuminance;
        core_get_point_for_mag(s->vmag, &size, &luminance);
        bv_to_rgb(s->bv, color);
        points[n] = (point_t) {
            .pos = {p_win[0], p_win[1]},
            .size = size,
            .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                      luminance * 255},
            // This makes very faint stars not selectable
            .oid = luminance > 0.5 ? s->oid : 0,
            .hint = pix_to_nuniq(order, pix),
        };
        n++;
        selected = core->selection && s->oid == core->selection->oid;
        if (selected || (s->vmag <= painter.hints_limit_mag - 4.0 &&
            survey != SURVEY_GAIA))
            star_render_name(&painter, s, FRAME_ASTROM, s->pos, size,
                             s->vmag, color);
    }
    paint_2d_points(&painter, n, points);
    free(points);

end:
    // Test if we should go into higher order tiles.
    if (!tile || (tile->mag_max > limit_mag))
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

    /* Get the global stars luminance */
    double lum = core_illuminance_to_lum_apparent(illuminance, 0);

    // This is 100% ad-hoc formula adjusted so that DSS properly disappears
    // when stars bright enough are visible
    lum = pow(lum, 0.333);
    lum /= 300;
    core_report_luminance_in_fov(lum, false);

    progressbar_report("stars", "Stars", nb_loaded, nb_tot, -1);
    return 0;
}

static int stars_get_visitor(int order, int pix, void *user)
{
    int i, p, code;
    bool is_gaia, sync;
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

    // Try both surveys (bundled and gaia).  For the bundled survey we
    // don't load in a thread.  This is a fix for the constellations!
    for (i = 0; !tile && i < 2; i++) {
        sync = i == SURVEY_DEFAULT;
        tile = get_tile(d->stars, i, order, pix, sync, &code);
    }

    // Gaia survey has a min order of 3.
    // XXX: read the survey properties file instead of hard coding!
    if (!tile) return order < 3 ? 1 : 0;
    for (i = 0; i < tile->nb; i++) {
        if (    (d->cat == 0 && tile->sources[i].hip == d->n) ||
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
    int order, pix, s, i, code;
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
    for (s = 0; s < 2; s++) {
        tile = get_tile(stars, s, order, pix, false, &code);
        if (!tile) continue;
        for (i = 0; i < tile->nb; i++) {
            if (tile->sources[i].oid == oid) {
                return (obj_t*)star_create(&tile->sources[i]);
            }
        }
    }
    return NULL;
}

static int stars_list(const obj_t *obj, observer_t *obs,
                      double max_mag, uint64_t hint, void *user,
                      int (*f)(void *user, obj_t *obj))
{
    int order, pix, i, r, code;
    tile_t *tile;
    stars_t *stars = (void*)obj;
    star_t *star;
    hips_iterator_t iter;

    // Without hint, we have to iter all the tiles.
    if (!hint) {
        hips_iter_init(&iter);
        while (hips_iter_next(&iter, &order, &pix)) {
            tile = get_tile(stars, 0, order, pix, false, &code);
            if (!tile || tile->mag_min >= max_mag) continue;
            for (i = 0; i < tile->nb; i++) {
                if (tile->sources[i].vmag > max_mag) continue;
                if (!f) continue;
                star = star_create(&tile->sources[i]);
                r = f(user, (obj_t*)star);
                obj_release((obj_t*)star);
                if (r) break;
            }
            if (i < tile->nb) break;
            hips_iter_push_children(&iter, order, pix);
        }
        return 0;
    }

    // Get tile from hint (as nuniq).
    nuniq_to_pix(hint, &order, &pix);
    tile = get_tile(stars, 0, order, pix, false, &code);
    if (!tile) {
        if (!code) return OBJ_AGAIN; // Try again later.
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
    const char *args_type, *title, *release_date_str, *max_vmag_str,
               *order_min_str;
    hips_settings_t survey_settings = {
        .create_tile = stars_create_tile,
        .delete_tile = del_tile,
    };
    int survey = SURVEY_DEFAULT;
    double release_date = 0;

    if (!type || !args || strcmp(type, "hips")) return 1;
    args_type = json_get_attr_s(args, "type");
    title = json_get_attr_s(args, "obs_title");
    if (!args_type || strcmp(args_type, "stars")) return 1;

    if (title && strcasecmp(title, "gaia") == 0) {
        survey = SURVEY_GAIA;
    }

    // Already filled.
    if (stars->surveys[survey].hips) return 1;

    release_date_str = json_get_attr_s(args, "hips_release_date");
    if (release_date_str)
        release_date = hips_parse_date(release_date_str);

    survey_settings.user = &stars->surveys[survey];
    snprintf(stars->surveys[survey].url, sizeof(stars->surveys[survey].url),
             "%s", url);
    stars->surveys[survey].hips = hips_create(
            stars->surveys[survey].url, release_date, &survey_settings);

    order_min_str = json_get_attr_s(args, "hips_order_min");
    if (order_min_str)
        stars->surveys[survey].min_order = atoi(order_min_str);

    // Tell online gaia survey to only start after the vmag for this survey.
    // XXX: We should remove that.
    if (survey == SURVEY_DEFAULT) {
        max_vmag_str = json_get_attr_s(args, "max_vmag");
        if (max_vmag_str)
            stars->surveys[SURVEY_GAIA].min_vmag = atof(max_vmag_str);
        stars->surveys[survey].min_vmag = NAN;
    }

    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t star_klass = {
    .id         = "star",
    .init       = star_init,
    .size       = sizeof(star_t),
    .get_info   = star_get_info,
    .render     = star_render,
    .get_designations = star_get_designations,
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
        PROPERTY(visible, TYPE_BOOL, MEMBER(stars_t, visible)),
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
    star = obj_create_str("star", NULL, data);
    assert(star);
    obj_get_info(star, core->observer, INFO_VMAG, &vmag);
    assert(fabs(vmag - 5.153) < 0.0001);
    obj_release(star);
}
TEST_REGISTER(NULL, test_create_from_json, TEST_AUTO);

#endif
