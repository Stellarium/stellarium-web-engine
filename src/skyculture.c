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
    end = strchrnul(*str, '\n');
    len = end - *str;
    if (len >= size) len = size - 1;
    strncpy(line, *str, len);
    line[len] = '\0';
    *str = end;
    if (**str == '\n') (*str)++;
    return true;
}

int skyculture_parse_names(const char *data, char *(*names)[2])
{
    char line[512], id[32], name[128];
    const char *tmp;
    regex_t reg;
    regmatch_t m[4];
    int r, i = 0, hd, hip, nb = 0;

    // Count the number of lines in the file.
    for (tmp = data; *tmp; tmp = strchr(tmp, '\n') + 1) nb++;
    if (!names) return nb;

    regcomp(&reg, "(HIP|HD)? *([0-9]+) *\\| *(.+)", REG_EXTENDED);
    while (iter_lines(&data, line, sizeof(line))) {
        if (str_startswith(line, "#")) continue;
        r = regexec(&reg, line, 4, m, 0);
        if (r) goto error;
        sprintf(name, "%*s", (int)(m[3].rm_eo - m[3].rm_so), line + m[3].rm_so);
        *id = '\0';
        if (strncmp(line + m[1].rm_so, "HD", 2) == 0) {
            hd = strtoul(line + m[2].rm_so, NULL, 10);
            sprintf(id, "HD %d", hd);
        }
        if (strncmp(line + m[1].rm_so, "HIP", 3) == 0) {
            hip = strtoul(line + m[2].rm_so, NULL, 10);
            sprintf(id, "HIP %d", hip);
        }
        if (*id) {
            names[i][0] = strdup(id);
            names[i][1] = strdup(name);
            i++;
        }
        continue;
error:
        LOG_W("Cannot parse star name: %s", line);
    }
    regfree(&reg);
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
    int hd;
    char bayer_id[32];
    if (tok[0] >= '0' && tok[1] <= '9') { // A HD number
        sscanf(tok, "%d", &hd);
        return hd;
    }
    // Otherwise a bayer id.
    sprintf(bayer_id, "%s %s", tok, cst);
    tok = (char*)identifiers_search(bayer_id);
    assert(tok && str_startswith(tok, "HD "));
    tok += 3;
    sscanf(tok, "%d", &hd);
    return hd;
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

static void parse_edges(const char *edges, constellation_infos_t *csts)
{
    constellation_infos_t *info = NULL;
    const char *line;
    char cst[2][8];
    int i, ra1_h, ra1_m, ra1_s, ra2_h, ra2_m, ra2_s;
    char dec1_sign, dec2_sign;
    int dec1_d, dec1_m, dec1_s, dec2_d, dec2_m, dec2_s;
    double ra1, dec1, ra2, dec2;
    const int MAX_EDGES = ARRAY_SIZE(info->edges);

    for (line = edges; *line; line = strchr(line, '\n') + 1) {
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
}


constellation_infos_t *skyculture_parse_constellations(
        const char *consts, const char *edges, int *nb_cst)
{
    char *data, *line, *tmp = NULL, *tok;
    bool linked;
    int star, last_star = 0, nb = 0;
    constellation_infos_t *ret, *cons;

    data = strdup(consts);

    // Count the number of lines in the file.
    for (line = data; *line; line = strchr(line, '\n') + 1) nb++;

    ret = calloc(nb + 1, sizeof(*ret));
    nb = 0;
    for (line = strtok_r(data, "\n", &tmp); line;
          line = strtok_r(NULL, "\n", &tmp)) {
        if (*line == '\0') continue;
        if (*line == '#') continue;
        cons = &ret[nb];
        strcpy(cons->id, strtok(line, "|"));
        strcpy(cons->name, strtok(NULL, "|"));
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

    if (edges) parse_edges(edges, ret);
    return ret;
}
