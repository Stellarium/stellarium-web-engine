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

/*
 * Function that iters a txt buffer line by line, taking care of the case
 * when the file doesn't end with a \n.
 *
 * XXX: is there a simpler way to do that??
 */
static bool iter_lines(const char **str, char *line, int size)
{
    const char *end;
    int len;
    if (!**str) return false;
    end = strchr(*str, '\n') ?: *str + strlen(*str);
    len = end - *str;
    if (len >= size) len = size - 1;
    strncpy(line, *str, len);
    line[len] = '\0';
    if (len && line[len - 1] == '\r') line[len - 1] = '\0';
    *str = end;
    if (**str == '\n') (*str)++;
    return true;
}

skyculture_name_t *skyculture_parse_names(const char *data)
{
    char line[512], name[128];
    regex_t reg;
    regmatch_t m[4];
    int r, hd, hip;
    skyculture_name_t *ret = NULL, *entry;
    uint64_t oid;

    regcomp(&reg, "(HIP|HD)? *([0-9]+) *\\| *(.+)", REG_EXTENDED);
    while (iter_lines(&data, line, sizeof(line))) {
        if (str_startswith(line, "#")) continue;
        r = regexec(&reg, line, 4, m, 0);
        if (r) goto error;
        snprintf(name, sizeof(name) - 1,
                 "%*s", (int)(m[3].rm_eo - m[3].rm_so), line + m[3].rm_so);
        oid = 0;
        if (strncmp(line + m[1].rm_so, "HD", 2) == 0) {
            hd = strtoul(line + m[2].rm_so, NULL, 10);
            oid = oid_create("HD", hd);
        }
        if (strncmp(line + m[1].rm_so, "HIP", 3) == 0) {
            hip = strtoul(line + m[2].rm_so, NULL, 10);
            oid = oid_create("HIP", hip);
        }
        if (!oid) continue;
        // Ignore alternative names for the moment!
        HASH_FIND(hh, ret, &oid, sizeof(oid), entry);
        if (entry) continue;
        entry = calloc(1, sizeof(*entry));
        entry->oid = oid;
        strcpy(entry->name, name); // XXX: check size!
        HASH_ADD(hh, ret, oid, sizeof(oid), entry);
        continue;
error:
        LOG_W("Cannot parse star name: %s", line);
    }
    regfree(&reg);
    return ret;
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
