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


static int count_lines(const char *str)
{
    int nb = 0;
    while (str) {
        nb++;
        str = strchr(str, '\n');
        if (str) str++;
    }
    return nb;
}

static void trim_right_spaces(char *s)
{
    int l = strlen(s);
    while (s[l - 1] == ' ') {
        s[l - 1] = '\0';
        l--;
    }
}

static int parse_star(const char *cst, const char *tok)
{
    int hip;
    if (tok[0] >= '0' && tok[1] <= '9') { // A HIP number
        sscanf(tok, "%d", &hip);
        return hip;
    } else {
        return -1; // Error
    }
}

static constellation_infos_t *get_constellation(
        constellation_infos_t *csts, const char *id)
{
    int i;
    for (i = 0; *(csts[i].id); i++) {
        if (strcasecmp(csts[i].id, id) == 0)
            return &csts[i];
    }
    return NULL;
}

/*
 * Function: skyculture_parse_edge
 * Parse a constellation edge file.
 *
 * Parameters:
 *   data   - Text data in the edge file format.
 *   infos  - Constellation info to update with the edge data.
 *
 * Return:
 *   The number of lines parsed, or -1 in case of error.
 */
int skyculture_parse_edges(const char *edges, constellation_infos_t *csts)
{
    constellation_infos_t *info = NULL;
    const char *line;
    char cst[2][8];
    int i, ra1_h, ra1_m, ra1_s, ra2_h, ra2_m, ra2_s, nb;
    char dec1_sign, dec2_sign;
    int dec1_d, dec1_m, dec1_s, dec2_d, dec2_m, dec2_s;
    double ra1, dec1, ra2, dec2;
    const int MAX_EDGES = ARRAY_SIZE(info->edges);

    for (line = edges, nb = 0; *line; line = strchr(line, '\n') + 1, nb++) {
        if (str_startswith(line, "//")) continue;
        if (*line == '\n') continue;
        if (sscanf(line, "%*s %*s"
                         "%d:%d:%d %c%d:%d:%d "
                         "%d:%d:%d %c%d:%d:%d "
                         "%s %s",
                         &ra1_h, &ra1_m, &ra1_s,
                         &dec1_sign, &dec1_d, &dec1_m, &dec1_s,
                         &ra2_h, &ra2_m, &ra2_s,
                         &dec2_sign, &dec2_d, &dec2_m, &dec2_s,
                         cst[0], cst[1]) != 16) {
            LOG_W("Cannot parse skyculture edge line: %.16s...", line);
            continue;
        }
        eraTf2a('+', ra1_h, ra1_m, ra1_s, &ra1);
        eraTf2a('+', ra2_h, ra2_m, ra2_s, &ra2);
        eraAf2a(dec1_sign, dec1_d, dec1_m, dec1_s, &dec1);
        eraAf2a(dec2_sign, dec2_d, dec2_m, dec2_s, &dec2);
        for (i = 0; i < 2; i++) {
            info = get_constellation(csts, cst[i]);
            if (!info) continue;
            if (info->nb_edges >= MAX_EDGES) {
                LOG_E("Too many bounds in constellation %s", cst);
                continue;
            }
            info->edges[info->nb_edges][0][0] = ra1;
            info->edges[info->nb_edges][0][1] = dec1;
            info->edges[info->nb_edges][1][0] = ra2;
            info->edges[info->nb_edges][1][1] = dec2;
            info->nb_edges++;
        }
    }
    return nb;
}


constellation_infos_t *skyculture_parse_constellations(
        const char *consts, int *nb_cst)
{
    char *data, *line, *tmp = NULL, *tok;
    bool linked;
    int star, last_star = 0, nb = 0;
    constellation_infos_t *ret, *cons;

    data = strdup(consts);
    ret = calloc(count_lines(consts) + 1, sizeof(*ret));
    for (line = strtok_r(data, "\r\n", &tmp); line;
          line = strtok_r(NULL, "\r\n", &tmp)) {
        if (*line == '\0') continue;
        if (*line == '#') continue;
        cons = &ret[nb];
        tok = strtok(line, "|");
        if (!tok) goto error;
        if (strlen(tok) > sizeof(cons->id)) goto error;
        strcpy(cons->id, tok);
        tok = strtok(NULL, "|");
        if (!tok) goto error;
        strcpy(cons->name, tok);
        trim_right_spaces(cons->name);
        while ((tok = strtok(NULL, " -"))) {
            // Check if the last separator was a '-'.
            linked = consts[tok - 1 - data] == '-';
            if (linked) assert(last_star);
            star = parse_star(cons->id, tok);
            if (linked) {
                cons->lines[cons->nb_lines][0] = last_star;
                cons->lines[cons->nb_lines][1] = star;
                cons->nb_lines++;
            }
            last_star = star;
        }
        nb++;
    }
    free(data);
    *nb_cst = nb;
    return ret;

error:
    LOG_W("Could not parse constellations data");
    *nb_cst = 0;
    free(data);
    free(ret);
    return NULL;
}

constellation_infos_t *skyculture_parse_stellarium_constellations(
        const char *data_, int *nb_out)
{
    int nb = 0, nb_segs, i, s1, s2;
    char *data, *line, *tmp, *tok;
    constellation_infos_t *ret, *cons;

    assert(data_);
    data = strdup(data_);

    // Count the number of lines in the file.
    ret = calloc(count_lines(data) + 1, sizeof(*ret));

    for (line = strtok_r(data, "\r\n", &tmp); line;
         line = strtok_r(NULL, "\r\n", &tmp))
    {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0') continue;
        if (*line == '#') continue;
        cons = &ret[nb];
        tok = strtok(line, " ");
        if (!tok) goto error;
        if (strlen(tok) > sizeof(cons->id)) goto error;
        strcpy(cons->id, tok);
        tok = strtok(NULL, " ");
        if (!tok) goto error;
        nb_segs = atoi(tok);
        for (i = 0; i < nb_segs; i++) {
            tok = strtok(NULL, " \t"); if (!tok) goto error;
            s1 = atoi(tok);
            tok = strtok(NULL, " \t"); if (!tok) goto error;
            s2 = atoi(tok);
            cons->lines[i][0] = s1;
            cons->lines[i][1] = s2;
            cons->nb_lines++;
        }
        nb++;
        continue;
error:
        LOG_W("Could not parse constellations data: %s", line);
    }
    free(data);
    *nb_out = nb;
    return ret;
}

/*
 * Function: skyculture_parse_stellarium_constellations_names
 * Parse a 'constellation_names.fab' file.
 *
 * Parameters:
 *   data   - Text data in the fab file format.
 *   infos  - Constellation info to update with the names.
 *
 * Return:
 *   The number of names parsed, or -1 in case of error.
 */
int skyculture_parse_stellarium_constellations_names(
        const char *data_, constellation_infos_t *infos)
{
    char *data, *line, *tmp = NULL;
    int r;
    constellation_infos_t *cons = NULL;
    regex_t reg;
    regmatch_t m[4];

    data = strdup(data_);
    // Regexp for:
    // <ID>   "<NAME>"      _("<TRANSLATED_NAME>")
    regcomp(&reg, "[ \t]*([.a-zA-Z0-9]+)"
                  "[ \t]+\"([^\"]*)\""
                  "[ \t]+_\\(\"([^\"]*)\"\\)", REG_EXTENDED);
    for (line = strtok_r(data, "\r\n", &tmp); line;
         line = strtok_r(NULL, "\r\n", &tmp))
    {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0') continue;
        if (*line == '#') continue;
        r = regexec(&reg, line, 4, m, 0);
        if (r) goto error;
        line[m[1].rm_eo] = '\0';
        cons = get_constellation(infos, line + m[1].rm_so);
        if (!cons) {
            LOG_W("Can not find constellation '%s'", line + m[1].rm_so);
            continue;
        }
        snprintf(cons->name, sizeof(cons->name),
                 "%.*s", (int)(m[2].rm_eo - m[2].rm_so), line + m[2].rm_so);
        snprintf(cons->name_translated, sizeof(cons->name_translated),
                 "%.*s", (int)(m[3].rm_eo - m[3].rm_so), line + m[3].rm_so);
        continue;
error:
        LOG_W("Could not parse constellation names: %s", line);
    }
    regfree(&reg);
    free(data);
    return 0;
}

constellation_art_t *skyculture_parse_stellarium_constellations_art(
        const char *data_, int *nb_out)
{
    int i, nb = 0;
    char *data, *line, *tmp = NULL, *tok;

    assert(data_);
    data = strdup(data_);
    constellation_art_t *ret, *art;

    ret = calloc(count_lines(data) + 1, sizeof(*ret));

    for (line = strtok_r(data, "\r\n", &tmp); line;
         line = strtok_r(NULL, "\r\n", &tmp))
    {
        if (*line == '\0') continue;
        if (*line == '#') continue;
        art = &ret[nb];
        art->uv_in_pixel = true;
        tok = strtok(line, " "); if (!tok) goto error;
        if (strlen(tok) >= sizeof(art->cst)) goto error;
        strncpy(art->cst, tok, sizeof(art->cst));
        tok = strtok(NULL, " "); if (!tok) goto error;
        if (strlen(tok) >= sizeof(art->img)) goto error;
        strncpy(art->img, tok, sizeof(art->img));
        for (i = 0; i < 3; i++) {
            tok = strtok(NULL, " "); if (!tok) goto error;
            art->anchors[i].uv[0] = atoi(tok);
            tok = strtok(NULL, " "); if (!tok) goto error;
            art->anchors[i].uv[1] = atoi(tok);
            tok = strtok(NULL, " "); if (!tok) goto error;
            art->anchors[i].hip = atoi(tok);
        }
        nb++;
        continue;
error:
        LOG_W("Cannot parse constellationart: %s", line);
    }
    free(data);
    if (nb_out) *nb_out = nb;
    return ret;

}

/*
 * Function: skyculture_parse_stellarium_star_names
 * Parse a skyculture star names file.
 */
skyculture_name_t *skyculture_parse_stellarium_star_names(const char *data_)
{
    skyculture_name_t *ret = NULL, *entry;
    char *data, *line, *tmp, buf[128];
    uint64_t oid;
    int hip;

    assert(data_);
    data = strdup(data_);

    // Parse something of the form:
    // 677|_("Alpheratz") 1,2,5,6,11,12
    for (line = strtok_r(data, "\r\n", &tmp); line;
         line = strtok_r(NULL, "\r\n", &tmp))
    {
        while (*line == ' ') line++;
        if (*line == '\0') continue;
        if (*line == '#') continue;
        if (sscanf(line, " %d | _(\"%[^\"]\")", &hip, buf) != 2) {
            LOG_W("Cannot parse star name: '%s'", line);
            continue;
        }
        oid = oid_create("HIP", hip);
        // Ignore alternative names for the moment!
        HASH_FIND(hh, ret, &oid, sizeof(oid), entry);
        if (entry) continue;
        entry = calloc(1, sizeof(*entry));
        entry->oid = oid;
        snprintf(entry->name, sizeof(entry->name), "%s", buf);
        HASH_ADD(hh, ret, oid, sizeof(entry->oid), entry);
    }
    free(data);
    return ret;
}

static int parse_lines_json(const json_value *v, int lines[64][2])
{
    int i, j, n, nb = 0;
    const json_value *seg;

    if (v->type != json_array) return -1;
    for (i = 0; i < v->u.array.length; i++) {
        seg = v->u.array.values[i];
        if (seg->type != json_array) return -1;
        n = seg->u.array.length - 1;
        for (j = 0; j < n; j++) {
            lines[nb][0] = seg->u.array.values[j]->u.integer;
            lines[nb][1] = seg->u.array.values[j + 1]->u.integer;
            nb++;
        }
    }
    return nb;
}

int skyculture_parse_feature_json(const json_value *v,
                                  constellation_infos_t *feature)
{
    const char *id, *name;
    int r;
    const json_value *lines, *description;

    r = jcon_parse(v, "{",
        "!id", JCON_STR(id),
        "name", JCON_STR(name),
        "lines", JCON_VAL(lines),
        "description", JCON_VAL(description),
    "}");
    if (r) goto error;

    snprintf(feature->id, sizeof(feature->id), "%s", id);
    if (name)
        snprintf(feature->name, sizeof(feature->name), "%s", name);
    if (description)
        feature->description = json_to_string(description);

    if (lines) {
        feature->nb_lines = parse_lines_json(lines, feature->lines);
        if (feature->nb_lines < 0) goto error;
    }

    return 0;

error:
    LOG_E("Cannot parse json feature");
    return -1;
}

// Eventually to be merged with skyculture_parse_feature_json.
int skyculture_parse_feature_art_json(const json_value *v,
                                      constellation_art_t *art)
{
    int i, r, w, h, x, y, hip;
    json_value *anchors[3];
    const char *id, *img;

    id = json_get_attr_s(v, "id");
    if (!id) goto error;
    snprintf(art->cst, sizeof(art->cst), "%s", id);

    v = json_get_attr(v, "image", json_object);
    if (!v) return -1;

    r = jcon_parse(v, "{",
        "!file", JCON_STR(img),
        "!size", "[", JCON_INT(w), JCON_INT(h), "]",
        "!anchors", "[",
            JCON_VAL(anchors[0]),
            JCON_VAL(anchors[1]),
            JCON_VAL(anchors[2]),
        "]",
    "}");
    if (r) goto error;

    snprintf(art->img, sizeof(art->img), "%s", img);
    art->uv_in_pixel = false;

    for (i = 0; i < 3; i++) {
        r = jcon_parse(anchors[i], "{",
            "pos", "[", JCON_INT(x), JCON_INT(y), "]",
            "hip", JCON_INT(hip),
        "}");
        if (r) goto error;
        art->anchors[i].hip = hip;
        art->anchors[i].uv[0] = (double)x / w;
        art->anchors[i].uv[1] = (double)y / h;
    }
    return 0;

error:
    LOG_E("Cannot parse json feature");
    return -1;
}

skyculture_name_t *skyculture_parse_names_json(const json_value *v)
{
    int i, j;
    const char *key, *name;
    uint64_t oid;
    skyculture_name_t *ret = NULL, *entry;

    if (v->type != json_object) goto error;
    for (i = 0; i < v->u.object.length; i++) {
        oid = 0;
        key = v->u.object.values[i].name;
        if (strncmp(key, "HIP ", 4) == 0)
            oid = oid_create("HIP", atoi(key + 4));
        if (!oid) goto error;

        if (v->u.object.values[i].value->type != json_array)
            goto error;

        for (j = 0; j < v->u.object.values[i].value->u.array.length; j++) {
            name = v->u.object.values[i].value->u.array.values[j]->u.string.ptr;
            entry = calloc(1, sizeof(*entry));
            entry->oid = oid;
            snprintf(entry->name, sizeof(entry->name), "%s", name);
            HASH_ADD(hh, ret, oid, sizeof(entry->oid), entry);

            // Ignore alternative names for the moment!
            break;
        }
    }

    return ret;

error:

    LOG_E("Cannot parse skyculture names");
    return NULL;
}
