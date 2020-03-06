/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include "designation.h"
#include "utstring.h"
#include <regex.h>
#include <zlib.h> // For crc32

// XXX: this very similar to stars.c.  I think we could merge most of the code.

#define DSO_DEFAULT_VMAG 16.0

/*
 * Type: dso_quick_data_t
 * Holds information used for clipping a DSO entry when rendering
 */
typedef struct {
    uint64_t oid;
    double bounding_cap[4];
    float  display_vmag;
} dso_clip_data_t;

/*
 * Type: dso_data_t
 * Holds information data about a single DSO entry
 */
typedef struct {
    union {
        struct {
            uint64_t oid;
            double bounding_cap[4];
            float  display_vmag;
        };
        dso_clip_data_t clip_data;
    };
    char        type[4];
    float       ra;     // ra equ J2000
    float       de;     // de equ J2000

    float       smin;   // Angular size (rad)
    float       smax;   // Angular size (rad)
    float       angle;

    int symbol;

    char *morpho;
    // List of extra names, separated by '\0', terminated by two '\0'.
    char *names;
    float  vmag;
} dso_data_t;

/*
 * Type: dso_t
 * A single DSO object.
 */
typedef struct dso {
    obj_t       obj;
    dso_data_t  data;
} dso_t;

/*
 * Type: tile_t
 * Custom tile structure for the dso HiPS survey.
 */
typedef struct tile {
    int         flags;
    double      mag_min;
    double      mag_max;
    int         nb;
    dso_data_t  *sources;
    dso_clip_data_t *sources_quick;
} tile_t;

typedef struct survey survey_t;
struct survey {
    char key[128];
    int idx;
    hips_t *hips;
    survey_t *next, *prev;
};

/*
 * Type: dsos_t
 * The module object.
 */
typedef struct {
    obj_t       obj;
    regex_t     search_reg;
    fader_t     visible;
    survey_t    *surveys; // List of DSO surveys.
    // Hints/labels magnitude offset
    double      hints_mag_offset;
    bool        hints_visible;
} dsos_t;

// Static instance.
static dsos_t *g_dsos = NULL;

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
 * Generate a uniq oid for a DSO.
 *
 * The oid number is generated from the nuniq number of the tile healpix
 * pixel and the index in the tile.
 *
 * We use 20 bits for the nuniq, 10 bits for the running index, and 2 bits
 * for the source index. This should allow to go up to order 8, with 1024
 * sources per tile, with up to 4 data sources.
 */
static uint64_t make_oid(int source, uint64_t nuniq, int index)
{
    if (nuniq >= 1 << 20 || index >= 1 << 10 || source >= 1 << 2) {
        LOG_W("Cannot generate uniq oid for DSO");
        LOG_W("Nuniq: %llu, index: %d", nuniq, index);
    }
    return oid_create("NDSO", (uint32_t)nuniq << 12 | source << 10 | index);
}

static dso_t *dso_create(const dso_data_t *data)
{
    dso_t *dso;
    dso = (dso_t*)obj_create("dso", NULL, NULL);
    dso->data = *data;
    memcpy(&dso->obj.type, data->type, 4);
    dso->obj.oid = data->oid;
    return dso;
}

static int dso_get_info(const obj_t *obj, const observer_t *obs, int info,
                        void *out)
{
    dso_t *dso = (dso_t*)obj;
    switch (info) {
    case INFO_PVO:
        memset(out, 0, 4 * sizeof(double));
        astrometric_to_apparent(obs, dso->data.bounding_cap, true, out);
        return 0;
    case INFO_VMAG:
        *(double*)out = dso->data.vmag;
        return 0;
    case INFO_SMIN:
        *(double*)out = dso->data.smin;
        return 0;
    case INFO_SMAX:
        *(double*)out = dso->data.smax;
        return 0;
    case INFO_MORPHO:
        *(char**)out = dso->data.morpho;
        return 0;
    default:
        return 1;
    }
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

static int dso_init(obj_t *obj, json_value *args)
{
    const double DAM2R = DD2R / 60.0; // arcmin to rad.
    int index;

    // Support creating a dso using noctuasky model data json values.
    dso_t *dso = (dso_t*)obj;
    json_value *model, *names, *types, *jstr;
    model = json_get_attr(args, "model_data", json_object);
    if (model) {
        dso->data.ra = json_get_attr_f(model, "ra", 0) * DD2R;
        dso->data.de = json_get_attr_f(model, "de", 0) * DD2R;
        eraS2c(dso->data.ra, dso->data.de, dso->data.bounding_cap);
        dso->data.vmag = json_get_attr_f(model, "Vmag", NAN);
        if (isnan(dso->data.vmag))
            dso->data.vmag = json_get_attr_f(model, "Bmag", NAN);
        dso->data.angle = json_get_attr_f(model, "angle", NAN) * DD2R;
        dso->data.smax = json_get_attr_f(model, "dimx", NAN) * DAM2R;
        dso->data.smin = json_get_attr_f(model, "dimy", NAN) * DAM2R;
    }
    dso->data.display_vmag = isnan(dso->data.vmag) ? DSO_DEFAULT_VMAG :
                                                      dso->data.vmag;
    names = json_get_attr(args, "names", json_array);
    if (names)
        dso->data.names = parse_json_names(names);

    // Since we are not in a tile, we use the hash of the name to generate
    // the oid.
    if (dso->data.names) {
        index = crc32(0, (void*)dso->data.names, strlen(dso->data.names));
        dso->data.oid = make_oid(0, 0, index % 1024 + 1);
    }

    types = json_get_attr(args, "types", json_array);
    if (types && types->u.array.length > 0) {
        jstr = types->u.array.values[0];
        if (jstr->type == json_string) {
            strncpy(dso->data.type, jstr->u.string.ptr, 4);
            dso->data.symbol = symbols_get_for_otype(dso->data.type);
        }
    }
    return 0;
}

static void strip_type(char str[4])
{
    char *start, *end;
    int i;
    for (start = str; *start == ' '; start++);
    for (end = start; end < &str[4] && *end != ' '; end++);
    for (i = 0; i < end - start; i++)
        str[i] = start[i];
}

// Used by the cache.
static int del_tile(void *data)
{
    int i;
    tile_t *tile = data;
    for (i = 0; i < tile->nb; i++) {
        free(tile->sources[i].names);
        free(tile->sources[i].morpho);
    }
    free(tile->sources);
    free(tile->sources_quick);
    free(tile);
    return 0;
}

static int dso_data_cmp(const void *a, const void *b)
{
    return cmp(((const dso_data_t*)a)->display_vmag,
               ((const dso_data_t*)b)->display_vmag);
}

static int on_file_tile_loaded(const char type[4],
                               const void *data, int size,
                               const json_value *json,
                               void *user)
{
    tile_t *tile;
    dso_data_t *s;
    int nb, i, j, version, data_ofs = 0, flags, row_size, order, pix;
    int children_mask;
    char morpho[32], ids[256] = {};
    double bmag, temp_mag, tmp_ra, tmp_de, tmp_smax, tmp_smin, tmp_angle;
    void *tile_data;
    uint64_t nuniq;
    survey_t *survey = USER_GET(user, 0);
    tile_t **out = USER_GET(user, 1); // Receive the tile.
    int *transparency = USER_GET(user, 2);

    eph_table_column_t columns[] = {
        {"type", 's', .size=4},
        {"vmag", 'f', EPH_VMAG},
        {"bmag", 'f', EPH_VMAG},
        {"ra",   'f', EPH_RAD},
        {"de",   'f', EPH_RAD},
        {"smax", 'f', EPH_RAD},
        {"smin", 'f', EPH_RAD},
        {"angl", 'f', EPH_RAD},
        {"morp", 's', .size=32},
        {"ids",  's', .size=256},
    };

    assert(survey);
    *out = NULL;
    if (strncmp(type, "DSO ", 4) != 0) return 0;

    eph_read_tile_header(data, size, &data_ofs, &version, &order, &pix);
    nb = eph_read_table_header(
            version, data, size, &data_ofs, &row_size, &flags,
            ARRAY_SIZE(columns), columns);
    if (nb < 0) {
        LOG_E("Cannot parse file");
        return -1;
    }
    tile_data = eph_read_compressed_block(data, size, &data_ofs, &size);
    if (!tile_data) return -1;
    data_ofs = 0;
    if (flags & 1) {
        eph_shuffle_bytes(tile_data + data_ofs, row_size, nb);
    }

    tile = calloc(1, sizeof(*tile));
    tile->mag_min = DBL_MAX;
    tile->mag_max = -DBL_MAX;
    tile->nb = nb;

    tile->sources = calloc(tile->nb, sizeof(dso_data_t));

    for (i = 0; i < tile->nb; i++) {
        s = &tile->sources[i];
        eph_read_table_row(tile_data, size, &data_ofs,
                           ARRAY_SIZE(columns), columns,
                           s->type,
                           &temp_mag, &bmag, &tmp_ra, &tmp_de,
                           &tmp_smax, &tmp_smin, &tmp_angle,
                           morpho, ids);
        s->ra = tmp_ra;
        s->de = tmp_de;

        s->smax = tmp_smax;
        s->smin = tmp_smin;
        s->angle = tmp_angle;
        if (!s->smin && s->smax) {
            s->smin = s->smax;
            s->angle = NAN;
        }

        // Compute the cap containing this DSO
        s->bounding_cap[3] = cosf(max(s->smin, s->smax));
        eraS2c(s->ra, s->de, s->bounding_cap);

        s->vmag = temp_mag;
        // For the moment use bmag as fallback vmag value
        if (isnan(s->vmag)) s->vmag = bmag;
        strip_type(s->type);
        s->display_vmag = isnan(s->vmag) ? DSO_DEFAULT_VMAG : s->vmag;
        tile->mag_min = min(tile->mag_min, s->display_vmag);
        tile->mag_max = max(tile->mag_max, s->display_vmag);
        nuniq = pix_to_nuniq(order, pix);
        s->oid = make_oid(survey->idx, nuniq, i);

        if (*morpho) s->morpho = strdup(morpho);
        s->symbol = symbols_get_for_otype(s->type);

        // Turn '|' separated ids into '\0' separated values.
        if (*ids) {
            s->names = calloc(1, 2 + strlen(ids));
            for (j = 0; ids[j]; j++)
                s->names[j] = ids[j] != '|' ? ids[j] : '\0';
        }
    }
    free(tile_data);

    // Sort DSO in tile by display magnitude
    qsort(tile->sources, tile->nb, sizeof(dso_data_t), dso_data_cmp);
    // Create a small table with all data used for fast tile iteration
    tile->sources_quick = calloc(tile->nb, sizeof(dso_clip_data_t));
    for (i = 0; i < tile->nb; ++i)
        tile->sources_quick[i] = tile->sources[i].clip_data;

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

static const void *dsos_create_tile(void *user, int order, int pix, void *data,
                                    int size, int *cost, int *transparency)
{
    tile_t *tile = NULL;
    survey_t *survey = user;
    eph_load(data, size, USER_PASS(survey, &tile, transparency),
             on_file_tile_loaded);
    if (tile) *cost = tile->nb * sizeof(*tile->sources);
    return tile;
}

static int dsos_init(obj_t *obj, json_value *args)
{
    dsos_t *dsos = (dsos_t*)obj;
    assert(!g_dsos);
    g_dsos = dsos;
    dsos->hints_visible = true;
    fader_init(&dsos->visible, true);
    regcomp(&dsos->search_reg, "(m|ngc|ic) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);
    return 0;
}

// Exactly the same that stars.c get_tile function...
static tile_t *get_tile(dsos_t *dsos, survey_t *survey,
                        int order, int pix, bool load,
                        bool *loading_complete)
{
    int code, flags = 0;
    tile_t *tile;
    if (!load) flags |= HIPS_CACHED_ONLY;
    tile = hips_get_tile(survey->hips, order, pix, flags, &code);
    if (loading_complete) *loading_complete = (code != 0);
    return tile;
}

static void compute_hint_transformation(
        const painter_t *painter,
        float ra, float de, float angle,
        float size_x, float size_y, int symbol,
        double win_pos[2], double win_size[2], double *win_angle)
{
    painter_project_ellipse(painter, FRAME_ASTROM, ra, de, angle,
                            size_x, size_y, win_pos, win_size, win_angle);

    win_size[0] = max(win_size[0], symbol == SYMBOL_GALAXY ? 6 : 12);
    win_size[1] = max(win_size[1], 12);
}


static void dso_get_2d_ellipse(const obj_t *obj, const observer_t *obs,
                               const projection_t* proj,
                               double win_pos[2], double win_size[2],
                               double* win_angle)
{
    const dso_t *dso = (dso_t*)obj;
    const dso_data_t *s = &dso->data;

    painter_t tmp_painter;
    tmp_painter.obs = obs;
    tmp_painter.proj = proj;
    compute_hint_transformation(&tmp_painter, s->ra, s->de, s->angle,
            s->smax, s->smin, s->symbol, win_pos, win_size, win_angle);
    win_size[0] /= 2.0;
    win_size[1] /= 2.0;
}

// Find the best name to display
static bool dso_get_short_name(const dso_data_t *s, char *out, int size)
{
    const char *names = s->names;
    if (!names)
        return false;

    char best_name[size];
    int best_name_len = size - 1;
    int len;

    best_name[0] = '\0';
    while (*names) {
        designation_cleanup(names, out, size, DSGN_TRANSLATE);
        len = strlen(out);
        if (len < 12) {
            return true;
        }
        if (len < best_name_len) {
            best_name_len = len;
            strncpy(best_name, out, size);
        }
        if (strncmp(names, "NAME ", 5) != 0) {
            break;
        }
        names += strlen(names) + 1;
    }
    strncpy(out, best_name, size);
    return true;

}


static void dso_render_label(const dso_data_t *s2, const dso_clip_data_t *s,
                             const painter_t *painter,
                             const double win_size[2], double win_angle)
{
    const bool selected = core->selection && s->oid == core->selection->oid;
    int effects = 0;
    double color[4], radius;
    char buf[128] = "";
    const float vmag = s->display_vmag;

    effects = TEXT_BOLD | TEXT_FLOAT;
    if (selected) {
        vec4_set(color, 1, 1, 1, 1);
        effects &= ~TEXT_FLOAT;
    } else {
        vec4_set(color, 0.83, 0.83, 1, 0.7);
    }
    radius = min(win_size[0] / 2, win_size[1] / 2) +
                 fabs(cos(win_angle)) *
                 fabs(win_size[0] / 2 - win_size[1] / 2);
    radius += 1;
    dso_get_short_name(s2, buf, sizeof(buf));
    if (buf[0]) {
        labels_add_3d(buf, FRAME_ASTROM, s->bounding_cap, true, radius,
                      FONT_SIZE_BASE - 2, color, 0, 0, effects,
                      -vmag, s->oid);
    }
}


// Render a DSO from its data.
static int dso_render_from_data(const dso_data_t *s2, const dso_clip_data_t *s,
                                const painter_t *painter, uint64_t hint)
{
    PROFILE(dso_render_from_data, PROFILE_AGGREGATE);
    double color[4];
    double win_pos[2], win_size[2], win_angle, hints_limit_mag;
    const bool selected = core->selection && s->oid == core->selection->oid;
    double opacity;
    painter_t tmp_painter;
    const float vmag = s->display_vmag;
    const double hints_mag_offset = g_dsos->hints_mag_offset - 0.8;

    hints_limit_mag = painter->hints_limit_mag - 0.5 + hints_mag_offset;

    // Allow to select DSO a bit fainter than the faintest star
    // as they tend to be more visible as they are extended objects.
    if (vmag > painter->stars_limit_mag + 1.5 || vmag > painter->hard_limit_mag)
        return 1;

    // Check that it's intersecting with current viewport
    if (painter_is_cap_clipped(painter, FRAME_ASTROM, s->bounding_cap))
        return 0;

    // Special case for Open Clusters, for which the limiting magnitude
    // is more like the one for a star.
    if (s2->symbol == SYMBOL_OPEN_GALACTIC_CLUSTER ||
        s2->symbol == SYMBOL_CLUSTER_OF_STARS ||
        s2->symbol == SYMBOL_MULTIPLE_DEFAULT) {
        hints_limit_mag = painter->hints_limit_mag - 2. + hints_mag_offset;
    }

    if (s2->smax == 0) {
        // DSO without shape don't need to have labels displayed unless they are
        // much zoomed or selected
        hints_limit_mag = painter->stars_limit_mag - 10 + hints_mag_offset;
    }

    if (selected)
        hints_limit_mag = 99;

    if (vmag > hints_limit_mag + 2)
        return 0;

    compute_hint_transformation(painter, s2->ra, s2->de, s2->angle,
            s2->smax, s2->smin, s2->symbol, win_pos, win_size,
            &win_angle);

    // Skip if 2D circle is outside screen (TODO intersect 2D ellipse instead)
    if (painter_is_2d_circle_clipped(painter, win_pos,
                                     max(win_size[0], win_size[1]) / 2))
        return 0;

    areas_add_ellipse(core->areas, win_pos, win_angle,
                      win_size[0] / 2, win_size[1] / 2, s->oid, hint);

    // Don't display when DSO global fader is off
    // But the previous steps are still necessary as we want to be able to
    // select them even without hints/names
    if (painter->color[3] < 0.01 && !selected)
        return 0;

    if (!g_dsos->hints_visible)
        return 0;

    if (vmag <= hints_limit_mag + 0.5) {
        tmp_painter = *painter;
        tmp_painter.lines.width = 2;
        if (selected) {
            // Smooth fade out when it's getting large, even when selected
            // for performance reasons
            opacity = smoothstep(800, 240, max(win_size[0], win_size[1]));
            vec4_set(color, 1, 1, 1, opacity);
        } else {
            // Smooth fade in when zooming
            opacity = smoothstep(hints_limit_mag + 0.5,
                                 hints_limit_mag - 0.5, vmag);
            // Smooth fade out when it's getting large
            opacity *= smoothstep(400, 120, max(win_size[0], win_size[1]));
            vec4_set(color, 0.45, 0.83, 1, 0.5* opacity);
        }
        if (color[3] > 0.05) {
            if (isnan(s2->angle) || s2->smin == 0 || s2->smin == s2->smax)
                win_angle = 0;
            symbols_paint(&tmp_painter, s2->symbol, win_pos, win_size, color,
                          win_angle);
        }
    }

    if (vmag <= hints_limit_mag - 1.) {
        dso_render_label(s2, s, painter, win_size, win_angle);
    }
    return 0;
}

static int dso_render(const obj_t *obj, const painter_t *painter)
{
    const dso_t *dso = (const dso_t*)obj;
    return dso_render_from_data(&dso->data, &dso->data.clip_data, painter, 0);
}

void dso_get_designations(
    const obj_t *obj, void *user,
    int (*f)(const obj_t *obj, void *user, const char *cat, const char *str))
{
    const dso_t *dso = (const dso_t*)obj;
    const dso_data_t *s = &dso->data;

    const char *names = s->names;
    char cat[128] = {};
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

static int render_visitor(int order, int pix, void *user)
{
    dsos_t *dsos = USER_GET(user, 0);
    painter_t painter = *(const painter_t*)USER_GET(user, 1);
    int *nb_tot = USER_GET(user, 2);
    int *nb_loaded = USER_GET(user, 3);
    survey_t *survey = USER_GET(user, 4);
    tile_t *tile;
    int i, ret;
    bool loaded;
    uint64_t hint;

    // Early exit if the tile is clipped.
    if (painter_is_healpix_clipped(&painter, FRAME_ICRF, order, pix, true))
        return 0;

    (*nb_tot)++;
    tile = get_tile(dsos, survey, order, pix, true, &loaded);
    if (loaded) (*nb_loaded)++;

    if (!tile) return 0;
    if (tile->mag_min > painter.stars_limit_mag + 1.5) return 0;

    hint = pix_to_nuniq(order, pix);
    for (i = 0; i < tile->nb; i++) {
        ret = dso_render_from_data(&tile->sources[i], &tile->sources_quick[i],
                                   &painter, hint);
        if (ret)
            break;
    }
    if (tile->mag_max > painter.stars_limit_mag + 1.5) return 0;
    return 1;
}

static int dsos_update(obj_t *obj, double dt)
{
    dsos_t *dsos = (dsos_t*)obj;
    return fader_update(&dsos->visible, dt);
}

static int dsos_render(const obj_t *obj, const painter_t *painter_)
{
    PROFILE(dsos_render, 0);
    dsos_t *dsos = (dsos_t*)obj;
    int nb_tot = 0, nb_loaded = 0;
    painter_t painter = *painter_;
    survey_t *survey;

    painter.color[3] *= dsos->visible.value;
    DL_FOREACH(dsos->surveys, survey) {
        hips_traverse(USER_PASS(dsos, &painter, &nb_tot, &nb_loaded, survey),
                      render_visitor);
    }
    progressbar_report("DSO", "DSO", nb_loaded, nb_tot, -1);
    return 0;
}

static int dsos_get_visitor(int order, int pix, void *user)
{
    int i;
    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } *d = user;
    tile_t *tile = NULL;
    survey_t *survey;

    DL_FOREACH(d->dsos->surveys, survey) {
        tile = get_tile(d->dsos, survey, order, pix, false, NULL);
        if (!tile) continue;
        for (i = 0; i < tile->nb; i++) {
            if (d->cat == 4 && tile->sources[i].oid == d->n) {
                d->ret = &dso_create(&tile->sources[i])->obj;
                return -1; // Stop the search.
            }
        }
    }
    return 1;
}

static obj_t *dsos_get(const obj_t *obj, const char *id, int flags)
{
    int r, cat;
    uint64_t n;
    regmatch_t matches[3];
    dsos_t *dsos = (dsos_t*)obj;

    r = regexec(&dsos->search_reg, id, 3, matches, 0);
    if (r) return NULL;
    n = strtoull(id + matches[2].rm_so, NULL, 10);
    if (strncasecmp(id, "m", 1) == 0) cat = 0;
    if (strncasecmp(id, "ngc", 3) == 0) cat = 1;
    if (strncasecmp(id, "ic", 2) == 0) cat = 2;

    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } d = {.dsos=(void*)obj, .cat=cat, .n=n};
    hips_traverse(&d, dsos_get_visitor);
    return d.ret;
}

static obj_t *dsos_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    int order, pix, i;
    dsos_t *dsos = (void*)obj;
    tile_t *tile = NULL;
    survey_t *survey = NULL;

    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } d = {.dsos=(void*)obj, .cat=4, .n=oid};

    if (!hint) {
        if (    !oid_is_catalog(oid, "NGC") &&
                !oid_is_catalog(oid, "IC") &&
                !oid_is_catalog(oid, "NDSO"))
            return NULL;
        hips_traverse(&d, dsos_get_visitor);
        return d.ret;
    }

    // Get tile from hint (as nuniq).
    nuniq_to_pix(hint, &order, &pix);

    // Try all surveys.
    DL_FOREACH(dsos->surveys, survey) {
        tile = get_tile(dsos, survey, order, pix, false, NULL);
        if (!tile) continue;
        for (i = 0; i < tile->nb; i++) {
            if (tile->sources[i].oid == oid) {
                return (obj_t*)dso_create(&tile->sources[i]);
            }
        }
    }
    return NULL;
}

static int dsos_list(const obj_t *obj, observer_t *obs,
                     double max_mag, uint64_t hint, const char *source,
                     void *user, int (*f)(void *user, obj_t *obj))
{
    int order, pix, i, r, nb = 0;
    dsos_t *dsos = (dsos_t*)obj;
    dso_t *dso;
    tile_t *tile;
    survey_t *survey;
    // Don't support listing without hint for the moment.
    if (!hint) return 0;
    // Get tile from hint (as nuniq).
    order = log2(hint / 4) / 2;
    pix = hint - 4 * (1 << (2 * (order)));

    DL_FOREACH(dsos->surveys, survey) {
        tile = get_tile(dsos, survey, order, pix, true, NULL);
        if (!tile) continue;
        for (i = 0; i < tile->nb; i++) {
            if (!f) continue;
            nb++;
            dso = dso_create(&tile->sources[i]);
            r = f(user, (obj_t*)dso);
            obj_release((obj_t*)dso);
            if (r) break;
        }
    }
    return nb;
}

static int dsos_add_data_source(obj_t *obj, const char *url, const char *key)
{
    dsos_t *dsos = (void*)obj;
    survey_t *survey;
    int idx;
    hips_settings_t survey_settings = {
        .create_tile = dsos_create_tile,
        .delete_tile = del_tile,
    };
    DL_COUNT(dsos->surveys, survey, idx);
    survey = calloc(1, sizeof(*survey));
    survey_settings.user = survey;
    survey->hips = hips_create(url, 0, &survey_settings);
    survey->idx = idx;
    if (key)
        snprintf(survey->key, sizeof(survey->key), "%s", key);
    DL_APPEND(dsos->surveys, survey);
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t dso_klass = {
    .id = "dso",
    .size = sizeof(dso_t),
    .init = dso_init,
    .get_info = dso_get_info,
    .render = dso_render,
    .get_designations = dso_get_designations,
    .get_2d_ellipse = dso_get_2d_ellipse,
};
OBJ_REGISTER(dso_klass)

static obj_klass_t dsos_klass = {
    .id     = "dsos",
    .size   = sizeof(dsos_t),
    .flags  = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init   = dsos_init,
    .update = dsos_update,
    .render = dsos_render,
    .get    = dsos_get,
    .get_by_oid  = dsos_get_by_oid,
    .list   = dsos_list,
    .add_data_source = dsos_add_data_source,
    .render_order = 25,
    .attributes = (attribute_t[]) {
        PROPERTY(visible, TYPE_BOOL, MEMBER(dsos_t, visible.target)),
        PROPERTY(hints_mag_offset, TYPE_FLOAT,
                 MEMBER(dsos_t, hints_mag_offset)),
        PROPERTY(hints_visible, TYPE_BOOL, MEMBER(dsos_t, hints_visible)),
        {}
    },
};
OBJ_REGISTER(dsos_klass)
