/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "utstring.h"
#include <regex.h>

// XXX: this very similar to stars.c.  I think we could merge most of the code.

#define URL_MAX_SIZE 4096
#define DSO_DEFAULT_VMAG 16.0


/*
 * Type: dso_data_t
 * Holds information data about a single DSO entry
 */
typedef struct {
    char        type[4];
    struct {
        int m;
        int ngc;
        int ic;
        uint64_t nsid;
        uint64_t oid;
    } id;
    double      vmag;
    double      ra;     // ra equ J2000
    double      de;     // de equ J2000

    double      smin;   // Angular size (rad)
    double      smax;   // Angular size (rad)
    double      angle;

    char short_name[64];
    // List of extra names, separated by '\0', terminated by two '\0'.
    char *names;
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
    dso_data_t *data;
} tile_t;

/*
 * Type: dsos_t
 * The module object.
 */
typedef struct {
    obj_t       obj;
    regex_t     search_reg;
    fader_t     visible;

    hips_t      *survey;
    char        survey_url[URL_MAX_SIZE - 256]; // Url of the DSO survey.
} dsos_t;

static char *make_id(const dso_data_t *data, char buff[128])
{
    assert(data->id.ngc || data->id.ic || data->id.nsid);
    if (data->id.ngc)
        sprintf(buff, "NGC %d", data->id.ngc);
    else if (data->id.ic)
        sprintf(buff, "IC %d", data->id.ic);
    else if (data->id.nsid)
        sprintf(buff, "NSID %" PRIu64, data->id.nsid);
    return buff;
}

static uint64_t make_oid(const dso_data_t *data)
{
    if (data->id.ngc) {
        return oid_create("NGC ", data->id.ngc);
    } else if (data->id.ic) {
        return oid_create("IC  ", data->id.ic);
    } else {
        return oid_create("NDSO", (uint32_t)data->id.nsid);
    }
}

static dso_t *dso_create(const dso_data_t *data)
{
    dso_t *dso;
    char buff[128];
    if (data->id.m)   sprintf(buff, "M %d",   data->id.m);
    if (data->id.ngc) sprintf(buff, "NGC %d", data->id.ngc);
    if (data->id.ic)  sprintf(buff, "IC %d",  data->id.ic);
    if (data->id.nsid)sprintf(buff, "NSID %" PRIu64,  data->id.nsid);
    dso = (dso_t*)obj_create("dso", make_id(data, buff), NULL, NULL);
    dso->data = *data;
    memcpy(&dso->obj.type, data->type, 4);
    dso->obj.nsid = data->id.nsid;
    dso->obj.oid = data->id.oid;
    return dso;
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
    // Support creating a dso using noctuasky model data json values.
    dso_t *dso = (dso_t*)obj;
    json_value *model, *names;
    model = json_get_attr(args, "model_data", json_object);
    if (model) {
        dso->data.ra = json_get_attr_f(model, "ra", 0) * DD2R;
        dso->data.de = json_get_attr_f(model, "de", 0) * DD2R;
        dso->data.vmag = json_get_attr_f(model, "Vmag", NAN);
        if (isnan(dso->data.vmag))
            dso->data.vmag = json_get_attr_f(model, "Bmag", NAN);
        dso->data.angle = json_get_attr_f(model, "angle", NAN) * DD2R;
        dso->data.smax = json_get_attr_f(model, "dimx", NAN) * DAM2R;
        dso->data.smin = json_get_attr_f(model, "dimy", NAN) * DAM2R;
    }
    dso->data.id.oid = make_oid(&dso->data);

    names = json_get_attr(args, "names", json_array);
    if (names)
        dso->data.names = parse_json_names(names);

    return 0;
}

static int dso_update(obj_t *obj, const observer_t *obs, double dt)
{
    dso_t *dso = (dso_t*)obj;
    eraS2c(dso->data.ra, dso->data.de, obj->pos.pvg[0]);
    obj->vmag = dso->data.vmag;
    compute_coordinates(obs, obj->pos.pvg[0], &obj->pos.ra, &obj->pos.dec);
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
    tile_t *tile = data;
    free(tile->data->names);
    free(tile->data);
    free(tile);
    return 0;
}

static int on_file_tile_loaded(const char type[4],
                               const void *data, int size, void *user)
{
    tile_t *tile;
    dso_data_t *d;
    int nb, i, version, data_ofs = 0, flags, row_size, order, pix;
    char buff[16];
    double bmag, temp_mag;
    void *tile_data;
    const double DAM2R = DD2R / 60.0; // arcmin to rad.
    eph_table_column_t columns[10] = {
        {"nsid", 'Q'},
        {"type", 's', .size=4},
        {"vmag", 'f', EPH_VMAG},
        {"bmag", 'f', EPH_VMAG},
        {"ra",   'f', EPH_DEG},
        {"de",   'f', EPH_DEG},
        {"smax", 'f', EPH_ARCMIN},
        {"smin", 'f', EPH_ARCMIN},
        {"angl", 'f', EPH_DEG},
        {"snam", 's', .size=64},
    };

    if (strncmp(type, "DSO ", 4) != 0) return 0;

    eph_read_tile_header(data, size, &data_ofs, &version, &order, &pix);
    nb = eph_read_table_header(
            version, data, size, &data_ofs, &row_size, &flags,
            ARRAY_SIZE(columns), columns);
    if (nb < 0) {
        LOG_E("Cannot parse file");
        *(tile_t**)user = NULL;
        return -1;
    }
    tile_data = eph_read_compressed_block(data, size, &data_ofs, &size);
    if (!tile_data) return -1;
    data_ofs = 0;
    if (flags & 1) {
        eph_shuffle_bytes(tile_data + data_ofs, row_size, nb);
    }

    tile = calloc(1, sizeof(*tile));
    tile->mag_min = +INFINITY;
    tile->mag_max = -INFINITY;
    tile->nb = nb;

    tile->data = calloc(tile->nb, sizeof(*tile->data));

    for (i = 0; i < tile->nb; i++) {
        d = &tile->data[i];
        eph_read_table_row(tile_data, size, &data_ofs, 10, columns,
                           &d->id.nsid, d->type,
                           &d->vmag, &bmag, &d->ra, &d->de,
                           &d->smax, &d->smin, &d->angle,
                           d->short_name);

        assert(d->id.nsid);
        d->ra *= DD2R;
        d->de *= DD2R;
        d->smax *= DAM2R;
        d->smin *= DAM2R;
        if (!d->smin && d->smax) d->smin = d->smax;
        d->angle *= DD2R;
        // For the moment use bmag as fallback vmag value
        if (isnan(d->vmag)) d->vmag = bmag;
        strip_type(d->type);
        temp_mag = isnan(d->vmag) ? DSO_DEFAULT_VMAG : d->vmag;
        tile->mag_min = min(tile->mag_min, temp_mag);
        tile->mag_max = max(tile->mag_max, temp_mag);
        d->id.oid = make_oid(d);

        // Add the identifiers.
        if (d->id.m) {
            sprintf(buff, "M %d", d->id.m);
            identifiers_add(d->id.oid, "M", buff + 2, buff, buff);
        }
        if (d->id.ngc) {
            sprintf(buff, "NGC %d", d->id.ngc);
            identifiers_add(d->id.oid, "NGC", buff + 4, buff, buff);
        }
        if (d->id.ic) {
            sprintf(buff, "IC %d", d->id.ic);
            identifiers_add(d->id.oid, "IC", buff + 3, buff, buff);
        }
    }
    free(tile_data);
    *(tile_t**)user = tile;
    return 0;
}

static const void *dsos_create_tile(void *user, int order, int pix, void *data,
                                    int size, int *cost, int *transparency)
{
    tile_t *tile;
    eph_load(data, size, &tile, on_file_tile_loaded);
    if (tile) *cost = tile->nb * sizeof(*tile->data);
    return tile;
}

static int dsos_init(obj_t *obj, json_value *args)
{
    dsos_t *dsos = (dsos_t*)obj;
    hips_settings_t survey_settings = {
        .create_tile = dsos_create_tile,
        .delete_tile = del_tile,
    };

    fader_init(&dsos->visible, false);


    // Bundled DSO if there is any (shouldn't be)
    /*
    ASSET_ITER("asset://dso/", path) {
        data = asset_get_data(path, &size, NULL);
        eph_load(data, size, dsos, on_file_tile_loaded);
        asset_release(path);
    }
    */

    regcomp(&dsos->search_reg, "(m|ngc|ic|nsid) *([0-9]+)",
            REG_EXTENDED | REG_ICASE);

    sprintf(dsos->survey_url, "https://data.stellarium.org/surveys/dso");
    dsos->survey = hips_create(dsos->survey_url, 0, &survey_settings);
    return 0;
}

// Exactly the same that stars.c get_tile function...
static tile_t *get_tile(dsos_t *dsos, int order, int pix, bool load,
                        bool *loading_complete)
{
    int code, flags = 0;
    tile_t *tile;
    if (!load) flags |= HIPS_CACHED_ONLY;
    tile = hips_get_tile(dsos->survey, order, pix, flags, &code);
    if (loading_complete) *loading_complete = (code != 0);
    return tile;
}

static void dso_render_name(const painter_t *painter, const dso_data_t *d,
                            const double pos[2], double size, double vmag,
                            int anchor)
{
    char buff[128] = "";
    if (d->short_name[0])
        strcpy(buff, d->short_name);
    else if (d->id.m)
        sprintf(buff, "M %d", d->id.m);
    else if (d->id.ngc)
        sprintf(buff, "NGC %d", d->id.ngc);
    else if (d->id.ic)
        sprintf(buff, "IC %d", d->id.ic);
    if (buff[0])
        labels_add(buff, pos, size, 13, painter->color, 0, anchor, -vmag);
}

// Project from UV to the annotation contour ellipse.
// XXX: Should probably add a painter functions to render shapes from ICRS
// coordinate + size so that we don't have to do this everywhere!
static void contour_project(const projection_t *proj, int flags,
                            const double *v, double *out)
{
    // XXX: most of the mat computation should be done in advance.
    const dso_data_t *data = proj->user;
    double theta, r, mat[3][3], p[4] = {1, 0, 0, 0};
    theta = v[0] * 2 * M_PI;
    double smax = data->smax;
    double smin = data->smin;
    double angle = data->angle;
    r = v[1] * smax / 2;

    if (isnan(angle) || isnan(smin)) {
        angle = 0.0;
        smin = smax;
    }

    mat3_set_identity(mat);
    mat3_rz(data->ra, mat, mat);
    mat3_ry(-data->de, mat, mat);
    mat3_rx(-angle, mat, mat);
    mat3_iscale(mat, 1.0, smin / smax, 1.0);
    mat3_rx(-theta, mat, mat);
    mat3_rz(r, mat, mat);

    mat3_mul_vec3(mat, p, p);
    out[3] = 0;
    vec4_copy(p, out);
}

static void render_contour(const dso_data_t *data,
                           const painter_t *painter_,
                           double p[4])
{
    double v[2] = {0, 1}, tmp[4], c[4], a[4], b[4], angle;
    painter_t painter = *painter_;
    projection_t proj;
    painter.lines_stripes = 10.0;
    proj = (projection_t) {
        .backward   = contour_project,
        .user       = data,
    };
    paint_quad_contour(&painter, FRAME_ICRS, &proj, 64, 4);

    // Find the new position for the label:
    if (p) {
        for (v[0] = 0; v[0] < 1; v[0] += 1.0 / 16) {
            contour_project(&proj, 0, v, tmp);
            convert_coordinates(painter.obs, FRAME_ICRS, FRAME_VIEW, 0,
                                tmp, tmp);
            project(painter.proj, PROJ_TO_NDC_SPACE, 2, tmp, tmp);
            if (tmp[1] > p[1]) vec2_copy(tmp, p);
        }
    }

    // Compute ellipse in screen frame and add it to the clickable areas.
    // XXX: All of this should be done in the painter.
    //
    // 1. Center
    v[0] = v[1] = 0;
    contour_project(&proj, 0, v, c);
    convert_coordinates(painter.obs, FRAME_ICRS, FRAME_VIEW, 0, c, c);
    project(painter.proj, PROJ_TO_NDC_SPACE, 2, c, c);
    c[0] = (+c[0] + 1) / 2 * core->win_size[0];
    c[1] = (-c[1] + 1) / 2 * core->win_size[1];
    // 2. Semi major.
    v[1] = 1;
    v[0] = 0;
    contour_project(&proj, 0, v, a);
    convert_coordinates(painter.obs, FRAME_ICRS, FRAME_VIEW, 0, a, a);
    project(painter.proj, PROJ_TO_NDC_SPACE, 2, a, a);
    a[0] = (+a[0] + 1) / 2 * core->win_size[0];
    a[1] = (-a[1] + 1) / 2 * core->win_size[1];
    // 3. Semi minor.
    v[1] = 1;
    v[0] = 0.25;
    contour_project(&proj, 0, v, b);
    convert_coordinates(painter.obs, FRAME_ICRS, FRAME_VIEW, 0, b, b);
    project(painter.proj, PROJ_TO_NDC_SPACE, 2, b, b);
    b[0] = (+b[0] + 1) / 2 * core->win_size[0];
    b[1] = (-b[1] + 1) / 2 * core->win_size[1];

    angle = atan2(a[1] - c[1], a[0] - c[0]);
    areas_add_ellipse(core->areas, c, angle,
                      vec2_dist(a, c), vec2_dist(b, c), data->id.oid, 0);
}

/*
 * Return the dso angle in screen coordinates.
 */
static double get_screen_angle(const dso_data_t *d, const painter_t *painter)
{
    projection_t proj;
    double v[2], c[4], a[4];
    proj = (projection_t) {
        .backward   = contour_project,
        .user       = d,
    };
    // 1. Center
    v[0] = v[1] = 0;
    contour_project(&proj, 0, v, c);
    convert_coordinates(painter->obs, FRAME_ICRS, FRAME_VIEW, 0, c, c);
    project(painter->proj, PROJ_TO_NDC_SPACE, 2, c, c);
    c[0] = (+c[0] + 1) / 2 * core->win_size[0];
    c[1] = (-c[1] + 1) / 2 * core->win_size[1];
    // 2. Semi major.
    v[1] = 1;
    v[0] = 0;
    contour_project(&proj, 0, v, a);
    convert_coordinates(painter->obs, FRAME_ICRS, FRAME_VIEW, 0, a, a);
    project(painter->proj, PROJ_TO_NDC_SPACE, 2, a, a);
    a[0] = (+a[0] + 1) / 2 * core->win_size[0];
    a[1] = (-a[1] + 1) / 2 * core->win_size[1];
    return atan2(a[1] - c[1], a[0] - c[0]);
}

// Render a DSO from its data.
static int dso_render_from_data(const dso_data_t *d,
                                const char *id,
                                const painter_t *painter_)
{
    double p[4] = {}, size, luminance, mag, temp_mag;
    point_t point;
    const char *symbol;
    painter_t painter = *painter_;
    double min_circle_size, circle_size = 0, angle;
    int label_anchor = ANCHOR_AROUND;
    bool show_contour;

    min_circle_size = core->fov / 20;
    temp_mag = isnan(d->vmag) ? DSO_DEFAULT_VMAG : d->vmag;

    if (temp_mag > painter.hint_mag_max) return 0;

    eraS2c(d->ra, d->de, p);
    convert_coordinates(painter.obs, FRAME_ICRS, FRAME_OBSERVED, 0, p, p);
    // Skip if below horizon.
    if ((painter.flags & PAINTER_HIDE_BELOW_HORIZON) && p[2] < 0)
        return 0;

    mag = core_get_observed_mag(temp_mag);
    core_get_point_for_mag(mag, &size, &luminance);

    convert_coordinates(painter.obs, FRAME_OBSERVED, FRAME_VIEW, 0, p, p);
    if (!project(painter.proj,
                 PROJ_ALREADY_NORMALIZED | PROJ_TO_NDC_SPACE, 2, p, p))
        return 0;

    show_contour = !isnan(d->smax) && d->smax > min_circle_size;
    if (show_contour) {
        vec4_set(painter.color, 0.9, 0.6, 0.6, 0.9);
        render_contour(d, &painter, p);
        label_anchor = ANCHOR_BOTTOM | ANCHOR_HCENTER | ANCHOR_FIXED;
    } else {
        vec4_set(painter.color, 1, 1, 1, 0.5);
        symbol = d->type;
        while (symbol && !symbols_get(symbol, NULL))
            symbol = otypes_get_parent(symbol);
        symbol = symbol ?: "ISM";
        angle = 0;
        if (strcmp(symbol, "G") == 0 && !isnan(d->angle) && d->smax > 0)
            angle = 45 * DD2R - get_screen_angle(d, &painter);
        symbols_paint(&painter, symbol, p, 12.0, NULL, angle);
        // Add the dso in the global list of rendered objects.
        // XXX: we could move this into symbols_paint.
        point = (point_t) {
            .pos = {
                (+p[0] + 1) / 2 * core->win_size[0],
                (-p[1] + 1) / 2 * core->win_size[1],
            },
            .size = 8,
            .oid = d->id.oid,
        };
        areas_add_circle(core->areas, point.pos, point.size, point.oid, 0);
    }

    if (temp_mag <= painter.label_mag_max || show_contour)
        dso_render_name(&painter, d, p, max(size, circle_size), mag,
                        label_anchor);

    return 0;
}

static int dso_render(const obj_t *obj, const painter_t *painter)
{
    const dso_t *dso = (dso_t*)obj;
    return dso_render_from_data(&dso->data, obj->id, painter);
}

void dso_get_designations(
    const obj_t *obj, void *user,
    int (*f)(void *user, const char *cat, const char *str))
{
    const dso_t *dso = (dso_t*)obj;
    const dso_data_t *d = &dso->data;
    const char *names = d->names;
    char cat[128] = {};
    while (names && *names) {
        strncpy(cat, names, sizeof(cat) - 1);
        if (!strchr(cat, ' ')) continue;
        *strchr(cat, ' ') = '\0';
        f(user, cat, names + strlen(cat) + 1);
        names += strlen(names) + 1;
    }
}

static int dso_render_pointer(const obj_t *obj, const painter_t *painter)
{
    const dso_t *dso = (dso_t*)obj;
    double min_circle_size;

    min_circle_size = core->fov / 20;
    if (isnan(dso->data.smax) || dso->data.smax <= min_circle_size) return 1;
    render_contour(&dso->data, painter, NULL);
    return 0;
}

static int render_visitor(int order, int pix, void *user)
{
    dsos_t *dsos = USER_GET(user, 0);
    painter_t painter = *(const painter_t*)USER_GET(user, 1);
    int *nb_tot = USER_GET(user, 2);
    int *nb_loaded = USER_GET(user, 3);
    tile_t *tile;
    int i;
    bool loaded;

    // Early exit if the tile is clipped.
    // (don't test at order 0 because is_tile_clipped doesn't work very
    //  well in that case!).
    if (    order > 0 &&
            painter_is_tile_clipped(&painter, FRAME_ICRS, order, pix, true))
        return 0;

    (*nb_tot)++;
    tile = get_tile(dsos, order, pix, true, &loaded);
    if (loaded) (*nb_loaded)++;

    if (!tile) return 0;
    if (tile->mag_min > painter.mag_max) return 0;

    for (i = 0; i < tile->nb; i++) {
        dso_render_from_data(&tile->data[i], NULL, &painter);
    }
    if (tile->mag_max > painter.mag_max) return 0;
    return 1;
}

static int dsos_update(obj_t *obj, const observer_t *obs, double dt)
{
    dsos_t *dsos = (dsos_t*)obj;
    return fader_update(&dsos->visible, dt);
}

static int dsos_render(const obj_t *obj, const painter_t *painter_)
{
    dsos_t *dsos = (dsos_t*)obj;
    int nb_tot = 0, nb_loaded = 0;
    painter_t painter = *painter_;
    painter.color[3] *= dsos->visible.value;
    if (painter.color[3] == 0) return 0;
    hips_traverse(USER_PASS(dsos, &painter, &nb_tot, &nb_loaded),
                  render_visitor);
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
    tile_t *tile;
    tile = get_tile(d->dsos, order, pix, false, NULL);
    if (!tile) return 0;
    for (i = 0; i < tile->nb; i++) {
        if (    (d->cat == 0 && tile->data[i].id.m    == d->n) ||
                (d->cat == 1 && tile->data[i].id.ngc  == d->n) ||
                (d->cat == 2 && tile->data[i].id.ic   == d->n) ||
                (d->cat == 3 && tile->data[i].id.nsid == d->n) ||
                (d->cat == 4 && tile->data[i].id.oid == d->n)) {
            d->ret = &dso_create(&tile->data[i])->obj;
            return -1; // Stop the search.
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
    if (strncasecmp(id, "nsid", 4) == 0) cat = 3;

    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } d = {.dsos=(void*)obj, .cat=cat, .n=n};
    hips_traverse(&d, dsos_get_visitor);
    return d.ret;
}

static obj_t *dsos_get_by_nsid(const obj_t *obj, uint64_t nsid)
{
    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } d = {.dsos=(void*)obj, .cat=3, .n=nsid};
    hips_traverse(&d, dsos_get_visitor);
    return d.ret;
}

static obj_t *dsos_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    struct {
        dsos_t      *dsos;
        obj_t       *ret;
        int         cat;
        uint64_t    n;
    } d = {.dsos=(void*)obj, .cat=4, .n=oid};
    if (    !oid_is_catalog(oid, "NGC ") &&
            !oid_is_catalog(oid, "IC  ") &&
            !oid_is_catalog(oid, "NDSO"))
        return NULL;
    hips_traverse(&d, dsos_get_visitor);
    return d.ret;
}

/*
 * Meta class declarations.
 */

static obj_klass_t dso_klass = {
    .id = "dso",
    .size = sizeof(dso_t),
    .init = dso_init,
    .update = dso_update,
    .render = dso_render,
    .get_designations = dso_get_designations,
    .render_pointer = dso_render_pointer,
    .attributes = (attribute_t[]) {
        // Default properties.
        PROPERTY("name"),
        PROPERTY("distance"),
        PROPERTY("ra"),
        PROPERTY("dec"),
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
    .get_by_nsid = dsos_get_by_nsid,
    .render_order = 25,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(dsos_t, visible.target)),
        PROPERTY("survey_url", "S", MEMBER(dsos_t, survey_url)),
        {}
    },
};
OBJ_REGISTER(dsos_klass)
