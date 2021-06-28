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
#include "designation.h"
#include "ini.h"

#include <regex.h>
#include <zlib.h>

#define URL_MAX_SIZE 4096

static const double LABEL_SPACING = 4;

static obj_klass_t star_klass;

typedef struct stars stars_t;
typedef struct {
    obj_t   obj;
    uint64_t gaia;  // Gaia source id (0 if none)
    int     hip;    // HIP number.
    float   vmag;
    float   plx;    // Parallax (arcsec) (Note: could be computed from pvo).
    float   bv;
    float   illuminance; // (lux)
    // Normalized Astrometric direction + movement.
    double  pvo[2][3];
    double  distance;    // Distance in AU
    // List of extra names, separated by '\0', terminated by two '\0'.
    char    *names;
    char    *sp_type;
} star_t;

typedef struct survey survey_t;
struct survey {
    stars_t *stars;
    char    key[128];
    hips_t *hips;
    char    url[URL_MAX_SIZE - 256];
    int     min_order;
    double  min_vmag; // Don't render survey below this mag.
    double  max_vmag;
    bool    is_gaia;
    survey_t *next, *prev;
};

/*
 * Type: stars_t
 * The module object.
 */
struct stars {
    obj_t           obj;
    regex_t         search_reg;
    survey_t        *surveys; // All the added surveys.
    bool            visible;
    // Hints/labels magnitude offset
    double          hints_mag_offset;
    bool            hints_visible;
};

// Static instance.
static stars_t *g_stars = NULL;

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
    star_t      *sources;
} tile_t;

static void nuniq_to_pix(uint64_t nuniq, int *order, int *pix)
{
    *order = log2(nuniq / 4) / 2;
    *pix = nuniq - 4 * (1 << (2 * (*order)));
}

/*
 * Precompute values about the star position to make rendering faster.
 * Parameters:
 *   ra     - ICRS coordinate J200 (rad).
 *   de     - ICRS coordinate J200 (rad).
 *   pra    - Proper motion (rad/year).
 *   pde    - Proper motion (rad/year).
 *   plx    - Parallax (arcseconds).
 */
static void compute_pv(double ra, double de, double pra, double pde,
                       double plx, double epoch, star_t *s)
{
    int r;
    double djm0, djm = 0;

    if (isnan(plx)) plx = 0;
    if (isnan(pde)) pde = 0;
    if (isnan(pra)) pra = 0;

    /* For the moment we ignore the proper motion of stars without parallax,
     * because that would result in an infinite vector speed. */
    if (plx <= 0)
        plx = pde = pra = 0;

    // Pre-compute 3D position and speed in catalog/barycentric position
    // at epoch 2000, to broadly match DSS images.
    r = eraStarpv(ra, de, pra / cos(de), pde, plx, 0, s->pvo);
    if (r & (2 | 4)) {
        LOG_W("Wrong star coordinates");
        if (r & 2) LOG_W("Excessive speed");
        if (r & 4) LOG_W("Solution didn't converge");
        LOG_W("ra:%.1f°, de:%.1f°, "
              "pmra:%.1f mas/year, pmde:%.1f mas/year, plx:%.1f mas",
              ra * DR2D, de * DR2D, pra * DR2MAS, pde * DR2MAS,
              plx * 1000);
    }
    if (r & 1) {
        s->distance = NAN;
    } else {
        s->distance = vec3_norm(s->pvo[0]);
    }

    // Apply proper motion to bring from catalog epoch to 2000.0 epoch
    eraEpb2jd(epoch, &djm0, &djm);
    double dt = ERFA_DJM00 - djm;
    vec3_addk(s->pvo[0], s->pvo[1], dt, s->pvo[0]);
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
    double epoch, ra, de, pra, pde;

    model= json_get_attr(args, "model_data", json_object);
    if (model) {
        ra = json_get_attr_f(model, "ra", 0) * DD2R;
        de = json_get_attr_f(model, "de", 0) * DD2R;
        star->plx = json_get_attr_f(model, "plx", 0) / 1000.0;
        pra = json_get_attr_f(model, "pm_ra", 0) * ERFA_DMAS2R;
        pde = json_get_attr_f(model, "pm_de", 0) * ERFA_DMAS2R;
        star->vmag = json_get_attr_f(model, "Vmag", NAN);
        epoch = json_get_attr_f(model, "epoch", 2000);
        if (isnan(star->vmag))
            star->vmag = json_get_attr_f(model, "Bmag", NAN);
        star->illuminance = core_mag_to_illuminance(star->vmag);
        compute_pv(ra, de, pra, pde, star->plx, epoch, star);
    }

    names = json_get_attr(args, "names", json_array);
    if (names)
        star->names = parse_json_names(names);
    return 0;
}

// Return the star astrometric position, that is as seen from earth center
// after applying proper motion and parallax.
static void star_get_astrom(const star_t *s, const observer_t *obs,
                            double v[3])
{
    // Apply proper motion
    double dt = obs->tt - ERFA_DJM00;
    vec3_addk(s->pvo[0], s->pvo[1], dt, v);
    // Move to geocentric to get the astrometric position (apply parallax)
    vec3_sub(v, obs->earth_pvb[0], v);
    vec3_normalize(v, v);
}

// Return position and velocity in ICRF with origin on observer (AU).
static int star_get_pvo(const obj_t *obj, const observer_t *obs,
                        double pvo[2][4])
{
    star_t *s = (void*)obj;
    star_get_astrom(s, obs, pvo[0]);
    convert_frame(obs, FRAME_ASTROM, FRAME_ICRF, true, pvo[0], pvo[0]);
    pvo[0][3] = 0.0;
    pvo[1][0] = pvo[1][1] = pvo[1][2] = pvo[1][3] = 0.0;
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
        *(double*)out = star->vmag;
        return 0;
    case INFO_DISTANCE:
        *(double*)out = star->distance;
        return 0;
    default:
        return 1;
    }
}

static json_value *star_get_json_data(const obj_t *obj)
{
    const star_t *star = (star_t*)obj;
    json_value* ret = json_object_new(0);
    json_value* md = json_object_new(0);
    if (!isnan(star->plx)) {
        json_object_push(md, "plx", json_double_new(star->plx * 1000));
    }
    if (!isnan(star->bv)) {
        json_object_push(md, "BVMag", json_double_new(star->bv));
    }
    if (star->sp_type) {
        json_object_push(md, "spect_t", json_string_new(star->sp_type));
    }
    json_object_push(ret, "model_data", md);
    return ret;
}

/*
 * Function: star_get_skycultural_name
 * Return the common name for a given star in the current sky culture and
 * translated in the current locale.
 *
 * Parameters:
 *   s      - A star_data_t struct instance.
 *   out    - Output buffer.
 *   size   - Output buffer size.
 *
 * Return:
 *   true if a label was found, false otherwise.
 */
static bool star_get_skycultural_name(const star_t *s, char *out, int size)
{
    const char *name;
    char hip_buf[128];

    // Only hipparcos stars have names in sky cultures
    if (s->hip == 0)
        return false;
    snprintf(hip_buf, sizeof(hip_buf), "HIP %d", s->hip);
    name = skycultures_get_label(hip_buf, out, size);
    return name != NULL;
}


static bool name_is_bayer(const char* name) {
    return strncmp(name, "* ", 2) == 0 || strncmp(name, "V* ", 3) == 0;
}

/*
 * Function: star_get_bayer_name
 * Return the Bayer / Flamsteed name for a given star
 *
 * Parameters:
 *   s      - A star_data_t struct instance.
 *   out    - Output buffer.
 *   size   - Output buffer size.
 *   flags  - Combination of BAYER_FLAGS
 *
 * Return:
 *   true if a label was found, false otherwise.
 */
static bool star_get_bayer_name(const star_t *s, char *out, int size,
                                int flags)
{
    const char *names = s->names;
    if (!names)
        return false;

    while (*names) {
        if (!name_is_bayer(names))
            names += strlen(names) + 1;
        else
            break;
    }
    if (*names) {
        designation_cleanup(names, out, size, flags);
        return true;
    }
    return false;
}


static void star_render_name(const painter_t *painter, const star_t *s,
                             int frame, const double pos[3],
                             const double win_pos[2], double radius,
                             double color[3])
{
    double label_color[4] = {color[0], color[1], color[2], 0.8};
    static const double white[4] = {1, 1, 1, 1};
    const bool selected = (&s->obj == core->selection);
    int effects = TEXT_FLOAT;
    char buf[128];
    const double hints_mag_offset = g_stars->hints_mag_offset +
                                    core_get_hints_mag_offset(win_pos);
    int flags = DSGN_TRANSLATE;
    const char *first_name = NULL;

    double lim_mag = painter->hints_limit_mag - 5 + hints_mag_offset;
    double lim_mag2 = painter->hints_limit_mag - 7.5 + hints_mag_offset;
    double lim_mag3 = painter->hints_limit_mag - 9.0 + hints_mag_offset;

    // Decide whether a label must be displayed
    if (!selected && s->vmag > lim_mag)
        return;

    buf[0] = 0;

    // Display the current skyculture's star name
    star_get_skycultural_name(s, buf, sizeof(buf));

    // Without international fallback, just stop here if we didn't find a name
    if (!buf[0] && !skycultures_fallback_to_international_names())
        return;

    first_name = s->names && s->names[0] ? s->names : NULL;

    // Fallback to international common names/bayer names
    if (first_name && !buf[0]) {
        if (selected || s->vmag < max(3, lim_mag2)) {
            // The star is quite bright or selected, displat a name
            if (selected || s->vmag < max(3, lim_mag3)) {
                // Use long version of bayer name for very bright stars
                flags |= BAYER_LATIN_LONG | BAYER_CONST_LONG;
            }
            designation_cleanup(first_name, buf, sizeof(buf), flags);
        } else {
            // From here we know the star is not selected and not very bright
            // just display the small form of bayer name to save space.
            star_get_bayer_name(s, buf, sizeof(buf), flags);
        }
    }

    if (!buf[0]) return;

    if (selected) {
        vec4_copy(white, label_color);
        effects = TEXT_BOLD;
    }
    radius += LABEL_SPACING;

    labels_add_3d(buf, frame, pos, true,
                 radius, FONT_SIZE_BASE, label_color, 0, 0,
                 effects, -s->vmag, &s->obj);
}

// Render a single star.
// This should be used only for stars that have been manually created.
static int star_render(const obj_t *obj, const painter_t *painter_)
{
    // XXX: the code is almost the same as the inner loop in stars_render.
    star_t *star = (star_t*)obj;
    double pvo[2][4], p[2], size, luminance;
    double color[3];
    painter_t painter = *painter_;
    point_t point;

    obj_get_pvo(obj, painter.obs, pvo);
    if (!painter_project(painter_, FRAME_ICRF, pvo[0], true, true, p))
        return 0;

    if (!core_get_point_for_mag(star->vmag, &size, &luminance))
        return 0;
    bv_to_rgb(isnan(star->bv) ? 0 : star->bv, color);

    point = (point_t) {
        .pos = {p[0], p[1]},
        .size = size,
        .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                  luminance * 255},
        .obj = &star->obj,
    };
    paint_2d_points(&painter, 1, &point);

    star_render_name(&painter, star, FRAME_ICRF, pvo[0], p, size, color);
    return 0;
}


void star_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    star_t *star = (star_t*)obj;
    const char *names = star->names;
    char buf[128];

    while (names && *names) {
        f(obj, user, NULL, names);
        names += strlen(names) + 1;
    }
    if (star->gaia) {
        snprintf(buf, sizeof(buf), "%" PRId64, star->gaia);
        f(obj, user, "GAIA", buf);
    }
}

// Used by the cache.
static int del_tile(void *data)
{
    int i;
    tile_t *tile = data;

    // Don't delete the tile if any contained star is used somehwere else.
    for (i = 0; i < tile->nb; i++) {
        if (tile->sources[i].obj.ref > 1) return CACHE_KEEP;
    }

    for (i = 0; i < tile->nb; i++) {
        free(tile->sources[i].names);
        free(tile->sources[i].sp_type);
    }
    free(tile->sources);
    free(tile);
    return 0;
}

static int star_data_cmp(const void *a, const void *b)
{
    return cmp(((const star_t*)a)->vmag, ((const star_t*)b)->vmag);
}

static int on_file_tile_loaded(const char type[4],
                               const void *data, int size,
                               const json_value *json,
                               void *user)
{
    int version, nb, data_ofs = 0, row_size, flags, i, j, order, pix;
    int children_mask;
    double vmag, gmag, ra, de, pra, pde, plx, bv, epoch;
    char ids[256] = {};
    char sp_type[32] = {};
    survey_t *survey = USER_GET(user, 0);
    tile_t **out = USER_GET(user, 1); // Receive the tile.
    int *transparency = USER_GET(user, 2);
    tile_t *tile;
    void *table_data;
    star_t *s;

    // All the columns we care about in the source file.
    eph_table_column_t columns[] = {
        {"type", 's', .size=4},
        {"gaia", 'Q'},
        {"hip",  'i'},
        {"vmag", 'f', EPH_VMAG},
        {"gmag", 'f', EPH_VMAG},
        {"ra",   'f', EPH_RAD},
        {"de",   'f', EPH_RAD},
        {"plx",  'f', EPH_ARCSEC},
        {"pra",  'f', EPH_RAD_PER_YEAR},
        {"pde",  'f', EPH_RAD_PER_YEAR},
        {"epoc", 'f', EPH_YEAR},
        {"bv",   'f'},
        {"ids",  's', .size=256},
        {"spec", 's', .size=32},
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
        s->obj.ref = 1;
        s->obj.klass = &star_klass;
        eph_read_table_row(
                table_data, size, &data_ofs, ARRAY_SIZE(columns), columns,
                s->obj.type, &s->gaia, &s->hip, &vmag, &gmag,
                &ra, &de, &plx, &pra, &pde, &epoch, &bv, ids, sp_type);
        assert(!isnan(ra));
        assert(!isnan(de));
        if (isnan(vmag)) vmag = gmag;
        assert(!isnan(vmag));

        // Ignore plx values that are too low.  This is mostly because the
        // current data has some wrong values.
        if (!isnan(plx) && (plx < 2.0 / 1000)) plx = 0.0;

        // Avoid overlapping stars from Gaia survey.
        if (survey->is_gaia && vmag < survey->min_vmag) continue;

        if (!*s->obj.type) strncpy(s->obj.type, "*", 4); // Default type.
        epoch = epoch ?: 2000; // Default epoch.
        s->vmag = vmag;
        s->plx = plx;
        s->bv = bv;

        // Turn '|' separated ids into '\0' separated values.
        if (*ids) {
            s->names = calloc(1, 2 + strlen(ids));
            for (j = 0; ids[j]; j++)
                s->names[j] = ids[j] != '|' ? ids[j] : '\0';
        }
        if (*sp_type) {
            s->sp_type = strdup(sp_type);
        }

        // If we didn't get any ids, but an HIP number, use it.
        if (!s->names && s->hip) {
            // Add a log this this probably means a problem in the data.
            if (s->vmag < 4) LOG_W_ONCE("HIP %d didn't have any ids", s->hip);
            s->names = calloc(1, 16);
            snprintf(s->names, 15, "HIP %d", s->hip);
        }

        compute_pv(ra, de, pra, pde, plx, epoch, s);
        s->illuminance = core_mag_to_illuminance(vmag);

        tile->illuminance += s->illuminance;
        tile->mag_min = min(tile->mag_min, vmag);
        tile->mag_max = max(tile->mag_max, vmag);
        tile->nb++;
    }

    // Sort the data by vmag, so that we can early exit during render.
    qsort(tile->sources, tile->nb, sizeof(*tile->sources), star_data_cmp);
    free(table_data);

    // If we have a json header, check for a children mask value.
    if (json) {
        children_mask = json_get_attr_i(json, "children_mask", -1);
        if (children_mask != -1) {
            *transparency = (~children_mask) & 15;
        }
    }

    *out = tile;
    return 0;
}

static const void *stars_create_tile(
        void *user, int order, int pix, void *data, int size,
        int *cost, int *transparency)
{
    tile_t *tile = NULL;
    survey_t *survey = user;
    eph_load(data, size, USER_PASS(survey, &tile, transparency),
             on_file_tile_loaded);
    if (tile) *cost = tile->nb * sizeof(*tile->sources);
    return tile;
}

static int stars_init(obj_t *obj, json_value *args)
{
    stars_t *stars = (stars_t*)obj;
    assert(!g_stars);
    g_stars = stars;
    stars->visible = true;
    regcomp(&stars->search_reg, "(hip|gaia) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);
    stars->hints_visible = true;
    return 0;
}

static survey_t *get_survey(const stars_t *stars, const char *key)
{
    survey_t *survey;
    DL_FOREACH(stars->surveys, survey) {
        if (strcmp(survey->key, key) == 0)
            return survey;
    }
    return NULL;
}

/*
 * Function: get_tile
 * Load and return a tile.
 *
 * Parameters:
 *   stars  - The stars module.
 *   survey - The survey.
 *   order  - Healpix order.
 *   pix    - Healpix pix.
 *   sync   - If set, don't load in a thread.  This will block the main
 *            loop so should be avoided.
 *   code   - http return code (0 if still loading).
 */
static tile_t *get_tile(stars_t *stars, survey_t *survey, int order, int pix,
                        bool sync, int *code)
{
    int flags = 0;
    tile_t *tile;
    assert(code);
    assert(survey);
    if (!sync) flags |= HIPS_LOAD_IN_THREAD;
    if (!survey->hips) {
        *code = 0;
        return NULL;
    }
    tile = hips_get_tile(survey->hips, order, pix, flags, code);
    return tile;
}

static int render_visitor(stars_t *stars, const survey_t *survey,
                          int order, int pix,
                          const painter_t *painter_,
                          int *nb_tot, int *nb_loaded,
                          double *illuminance)
{
    painter_t painter = *painter_;
    tile_t *tile;
    int i, n = 0, code;
    star_t *s;
    double p_win[4], size = 0, luminance = 0, vmag = -DBL_MAX;
    double color[3];
    double v[3];
    double limit_mag = min(painter.stars_limit_mag, painter.hard_limit_mag);
    bool selected;

    // Early exit if the tile is clipped.
    if (painter_is_healpix_clipped(&painter, FRAME_ASTROM, order, pix))
        return 0;
    if (order < survey->min_order) return 1;

    (*nb_tot)++;
    tile = get_tile(stars, survey, order, pix, false, &code);
    if (code) (*nb_loaded)++;

    if (!tile) goto end;
    if (tile->mag_min > limit_mag) goto end;

    point_t *points = malloc(tile->nb * sizeof(*points));
    for (i = 0; i < tile->nb; i++) {
        s = &tile->sources[i];
        if (s->vmag > limit_mag) break;

        star_get_astrom(s, painter.obs, v);
        if (!painter_project(&painter, FRAME_ASTROM, v, true, true, p_win))
            continue;

        (*illuminance) += s->illuminance;

        // No need to recompute the point size and luminance if the last
        // star had the same vmag (often the case since we sort by vmag).
        if (s->vmag != vmag) {
            vmag = s->vmag;
            core_get_point_for_mag(vmag, &size, &luminance);
        }
        if (size == 0.0 || luminance == 0.0)
            continue;

        bv_to_rgb(isnan(s->bv) ? 0 : s->bv, color);
        points[n] = (point_t) {
            .pos = {p_win[0], p_win[1]},
            .size = size,
            .color = {color[0] * 255, color[1] * 255, color[2] * 255,
                      luminance * 255},
            // This makes very faint stars not selectable
            .obj = (luminance > 0.5 && size > 1) ? &s->obj : NULL,
        };
        n++;
        selected = (&s->obj == core->selection);
        if (selected || (stars->hints_visible && !survey->is_gaia))
            star_render_name(&painter, s, FRAME_ASTROM, v, p_win, size, color);
    }
    if (n > 0) {
        paint_2d_points(&painter, n, points);
    }
    free(points);

end:
    // Test if we should go into higher order tiles.
    if (!tile || (tile->mag_max > limit_mag))
        return 0;
    return 1;
}


static int stars_render(const obj_t *obj, const painter_t *painter_)
{
    stars_t *stars = (stars_t*)obj;
    int nb_tot = 0, nb_loaded = 0, order, pix, r;
    double illuminance = 0; // Totall illuminance
    painter_t painter = *painter_;
    survey_t *survey;
    hips_iterator_t iter;

    if (!stars->visible) return 0;

    DL_FOREACH(stars->surveys, survey) {
        // Don't even traverse if the min vmag of the survey is higher than
        // the max visible vmag.
        if (survey->min_vmag > painter.stars_limit_mag)
            continue;
        hips_iter_init(&iter);
        while (hips_iter_next(&iter, &order, &pix)) {
            r = render_visitor(stars, survey, order, pix, &painter,
                               &nb_tot, &nb_loaded, &illuminance);
            if (r == 1) hips_iter_push_children(&iter, order, pix);
        }
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

static int stars_list(const obj_t *obj,
                      double max_mag, uint64_t hint, const char *source,
                      void *user, int (*f)(void *user, obj_t *obj))
{
    int order, pix, i, r, code;
    tile_t *tile;
    stars_t *stars = (void*)obj;
    hips_iterator_t iter;
    survey_t *survey = NULL;

    if (isnan(max_mag)) max_mag = DBL_MAX;
    // Find the survey corresponding to the source.  If we don't find it,
    // default to the first survey.
    if (source) {
        DL_FOREACH(stars->surveys, survey) {
            if (strcmp(survey->key, source) == 0)
                break;
        }
    }
    if (!survey) survey = stars->surveys;
    if (!survey) return 0;

    // Without hint, we have to iter all the tiles.
    if (!hint) {
        hips_iter_init(&iter);
        while (hips_iter_next(&iter, &order, &pix)) {
            tile = get_tile(stars, survey, order, pix, false, &code);
            if (!tile || tile->mag_min >= max_mag) continue;
            for (i = 0; i < tile->nb; i++) {
                if (tile->sources[i].vmag > max_mag) continue;
                r = f(user, &tile->sources[i].obj);
                if (r) break;
            }
            if (i < tile->nb) break;
            hips_iter_push_children(&iter, order, pix);
        }
        return 0;
    }

    // Get tile from hint (as nuniq).
    nuniq_to_pix(hint, &order, &pix);
    tile = get_tile(stars, survey, order, pix, false, &code);
    if (!tile) {
        if (!code) return MODULE_AGAIN; // Try again later.
        return -1;
    }
    for (i = 0; i < tile->nb; i++) {
        r = f(user, &tile->sources[i].obj);
        if (r) break;
    }
    return 0;
}

static int hips_property_handler(void* user, const char* section,
                                 const char* name, const char* value)
{
    json_value *args = user;
    json_object_push(args, name, json_string_new(value));
    return 0;
}

static json_value *hips_load_properties(const char *url, int *code)
{
    char path[1024];
    const char *data;
    json_value *ret;
    snprintf(path, sizeof(path), "%s/properties", url);
    data = asset_get_data(path, NULL, code);
    if (!data) return NULL;
    ret = json_object_new(0);
    ini_parse_string(data, hips_property_handler, ret);
    return ret;
}

static int survey_cmp(const void *s1_, const void *s2_)
{
    const survey_t *s1 = s1_;
    const survey_t *s2 = s2_;
    const double inf = 1000;
    return cmp(isnan(s1->max_vmag) ? inf : s1->max_vmag,
               isnan(s2->max_vmag) ? inf : s2->max_vmag);
}

/*
 * Function: properties_get_f
 * Extract a float value from a hips properties json.
 *
 * We can just use json_get_attr_f, since the properties files attributes
 * are not typed and so all parsed as string.
 */
static double properties_get_f(const json_value *props, const char *key,
                               double default_value)
{
    const char *str;
    str = json_get_attr_s(props, key);
    if (!str) return default_value;
    return atof(str);
}

static int stars_add_data_source(obj_t *obj, const char *url, const char *key)
{
    stars_t *stars = (stars_t*)obj;
    json_value *args;
    const char *args_type, *release_date_str;
    hips_settings_t survey_settings = {
        .create_tile = stars_create_tile,
        .delete_tile = del_tile,
    };
    int i, code;
    double release_date = 0;
    survey_t *survey, *gaia;

    // We can't add the source until the properties file has been parsed.
    args = hips_load_properties(url, &code);
    if (code == 0) return MODULE_AGAIN;

    if (!args) return -1;
    args_type = json_get_attr_s(args, "type");
    if (!args_type || strcmp(args_type, "stars")) {
        LOG_W("Source is not a star survey: %s", url);
        json_builder_free(args);
        return -1;
    }

    survey = calloc(1, sizeof(*survey));
    if (key) snprintf(survey->key, sizeof(survey->key), "%s", key);
    if (key && strcasecmp(key, "gaia") == 0) {
        survey->is_gaia = true;
    }

    release_date_str = json_get_attr_s(args, "hips_release_date");
    if (release_date_str)
        release_date = hips_parse_date(release_date_str);

    survey_settings.user = survey;
    snprintf(survey->url, sizeof(survey->url), "%s", url);
    survey->hips = hips_create(survey->url, release_date, &survey_settings);
    survey->min_order = properties_get_f(args, "hips_order_min", 0);
    survey->max_vmag = properties_get_f(args, "max_vmag", NAN);
    survey->min_vmag = properties_get_f(args, "min_vmag", -2.0);

    // Preload the first level of the survey (only for bright stars).
    if (survey->min_order == 0 && survey->min_vmag <= 0.0) {
        for (i = 0; i < 12; i++) {
            hips_get_tile(survey->hips, 0, i, 0, &code);
        }
    }

    DL_APPEND(stars->surveys, survey);
    DL_SORT(stars->surveys, survey_cmp);
    if (survey->is_gaia) assert(survey == stars->surveys->prev);

    // Tell online gaia survey to only start after the vmag for this survey.
    // XXX: We should remove that.
    gaia = get_survey(stars, "gaia");
    if (gaia) {
        DL_FOREACH(stars->surveys, survey) {
            if (!survey->is_gaia && !isnan(survey->max_vmag)) {
                gaia->min_vmag = max(gaia->min_vmag, survey->max_vmag);
            }
        }
    }

    json_builder_free(args);
    return 0;
}

obj_t *obj_get_by_hip(int hip, int *code)
{
    int order, pix, i;
    stars_t *stars = g_stars;
    survey_t *survey;
    tile_t *tile;

    for (order = 0; order < 2; order++) {
        pix = hip_get_pix(hip, order);
        if (pix == -1) {
            *code = 404;
            return NULL;
        }
        for (survey = stars->surveys; survey; survey = survey->next) {
            if (survey->is_gaia) continue;
            tile = get_tile(stars, survey, order, pix, true, code);
            if (code == 0) return NULL; // Still loading.
            if (!tile) return NULL;
            for (i = 0; i < tile->nb; i++) {
                if (tile->sources[i].hip == hip) {
                    return obj_retain(&tile->sources[i].obj);
                }
            }
        }
    }
    *code = 404;
    return NULL;
}

/*
 * Meta class declarations.
 */

static obj_klass_t star_klass = {
    .id         = "star",
    .init       = star_init,
    .size       = sizeof(star_t),
    .get_info   = star_get_info,
    .get_json_data = star_get_json_data,
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
    .list           = stars_list,
    .add_data_source = stars_add_data_source,
    .render_order   = 20,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(stars_t, visible)),
        PROPERTY(hints_mag_offset, TYPE_FLOAT,
                 MEMBER(stars_t, hints_mag_offset)),
        PROPERTY(hints_visible, TYPE_BOOL, MEMBER(stars_t, hints_visible)),
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
    star = obj_create_str("star", data);
    assert(star);
    obj_get_info(star, core->observer, INFO_VMAG, &vmag);
    assert(fabs(vmag - 5.153) < 0.0001);
    obj_release(star);
}
TEST_REGISTER(NULL, test_create_from_json, TEST_AUTO);

#endif
