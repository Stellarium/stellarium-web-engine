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


static constellation_infos_t *get_constellation(
        constellation_infos_t *csts, int size, const char *id)
{
    int i;

    // Small fix for SER1 and SER2!
    if (strcasecmp(id, "SER1") == 0 || strcasecmp(id, "SER2") == 0)
        id = "SER";

    for (i = 0; i < size; i++) {
        if (strcasecmp(csts[i].iau, id) == 0)
            return &csts[i];
    }
    return NULL;
}

/*
 * Function: skyculture_parse_edge
 * Parse constellation edges.
 *
 * Parameters:
 *   edges  - json array of the edges (see western skyculture json).
 *   infos  - Constellation info to update with the edge data.
 *   size   - Size of the infos array.
 *
 * Return:
 *   The number of lines parsed, or -1 in case of error.
 */
int skyculture_parse_edges(const json_value *edges,
                           constellation_infos_t *csts, int size)
{
    constellation_infos_t *info = NULL;
    const json_value *line;
    char cst[2][8];
    int i, j, ra1_h, ra1_m, ra1_s, ra2_h, ra2_m, ra2_s, nb = 0;
    char dec1_sign, dec2_sign;
    int dec1_d, dec1_m, dec1_s, dec2_d, dec2_m, dec2_s;
    double ra1, dec1, ra2, dec2;
    const int MAX_EDGES = ARRAY_SIZE(info->edges);

    for (i = 0; i < edges->u.array.length; i++) {
        line = edges->u.array.values[i];
        if (line->type != json_string) {
            LOG_W("Cannot parse skyculture edge line");
            continue;
        }
        if (sscanf(line->u.string.ptr,
                         "%*s %*s"
                         "%d:%d:%d %c%d:%d:%d "
                         "%d:%d:%d %c%d:%d:%d "
                         "%s %s",
                         &ra1_h, &ra1_m, &ra1_s,
                         &dec1_sign, &dec1_d, &dec1_m, &dec1_s,
                         &ra2_h, &ra2_m, &ra2_s,
                         &dec2_sign, &dec2_d, &dec2_m, &dec2_s,
                         cst[0], cst[1]) != 16) {
            LOG_W("Cannot parse skyculture edge line: %.16s...",
                  line->u.string.ptr);
            continue;
        }
        eraTf2a('+', ra1_h, ra1_m, ra1_s, &ra1);
        eraTf2a('+', ra2_h, ra2_m, ra2_s, &ra2);
        eraAf2a(dec1_sign, dec1_d, dec1_m, dec1_s, &dec1);
        eraAf2a(dec2_sign, dec2_d, dec2_m, dec2_s, &dec2);
        for (j = 0; j < 2; j++) {
            info = get_constellation(csts, size, cst[j]);
            if (!info) continue;
            if (info->nb_edges >= MAX_EDGES) {
                LOG_E("Too many bounds in constellation %s (%d)",
                      cst, info->nb_edges);
                continue;
            }
            info->edges[info->nb_edges][0][0] = ra1;
            info->edges[info->nb_edges][0][1] = dec1;
            info->edges[info->nb_edges][1][0] = ra2;
            info->edges[info->nb_edges][1][1] = dec2;
            info->nb_edges++;
        }
        nb++;
    }
    return nb;
}

static int parse_lines_json(const json_value *v,
                            constellation_line_t lines[static 64])
{
    int i, j, n, nb = 0;
    const json_value *seg;
    uint8_t line_weight;
    json_value **values;

    if (v->type != json_array) return -1;
    for (i = 0; i < v->u.array.length; i++) {
        seg = v->u.array.values[i];
        if (seg->type != json_array) return -1;
        n = seg->u.array.length - 1;
        line_weight = LINE_WEIGHT_NORMAL;
        values = seg->u.array.values;

        for (j = 0; j < n; j++) {
            // The first value can be a string defining the line style.
            if (j == 0 && values[0]->type == json_string) {
                if (strcmp(values[0]->u.string.ptr, "thin") == 0)
                    line_weight = LINE_WEIGHT_THIN;
                else if (strcmp(values[0]->u.string.ptr, "bold") == 0)
                    line_weight = LINE_WEIGHT_BOLD;
                continue;
            }
            lines[nb].hip[0] = values[j]->u.integer;
            lines[nb].hip[1] = values[j + 1]->u.integer;
            lines[nb].line_weight = line_weight;
            nb++;
        }
    }
    return nb;
}

int skyculture_parse_feature_json(skyculture_name_t** names_hash,
                                  const json_value *v,
                                  constellation_infos_t *feature)
{
    const char *id, *iau = NULL;
    const char *english = NULL, *native = NULL, *pronounce = NULL;
    const char *name_description = NULL, *description = NULL, *img;
    int r, i, w, h, x, y, hip;
    const json_value *lines = NULL, *common_name = NULL, *image = NULL;
    skyculture_name_t *entry;
    json_value *anchors[3];

    r = jcon_parse(v, "{",
        "id", JCON_STR(id),
        "?iau", JCON_STR(iau),
        "?common_name", JCON_VAL(common_name),
        "?lines", JCON_VAL(lines),
        "?description", JCON_STR(description),
        "?image", JCON_VAL(image),
    "}");
    if (r) goto error;

    snprintf(feature->id, sizeof(feature->id), "%s", id);

    // Loads common name directly in the names hash
    if (common_name) {
        r = jcon_parse(common_name, "{",
            "?english", JCON_STR(english),
            "?native", JCON_STR(native),
            "?pronounce", JCON_STR(pronounce),
            "?description", JCON_STR(name_description),
        "}");
        if (r) goto error;
        entry = calloc(1, sizeof(*entry));
        entry->main_id = strdup(id);
        if (english)
            entry->name_english = strdup(english);
        if (native)
            entry->name_native = strdup(native);
        if (pronounce)
            entry->name_pronounce = strdup(pronounce);
        if (name_description)
            entry->name_description = strdup(name_description);
        HASH_ADD_KEYPTR(hh, *names_hash, entry->main_id,
                        strlen(entry->main_id), entry);
    }
    if (description)
        feature->description = strdup(description);
    if (iau)
        snprintf(feature->iau, sizeof(feature->iau), "%s", iau);

    if (lines) {
        feature->nb_lines = parse_lines_json(lines, feature->lines);
        if (feature->nb_lines < 0) goto error;
    }

    if (image) {
        r = jcon_parse(image, "{",
            "file", JCON_STR(img),
            "size", "[", JCON_INT(w, 0), JCON_INT(h, 0), "]",
            "anchors", "[",
                JCON_VAL(anchors[0]),
                JCON_VAL(anchors[1]),
                JCON_VAL(anchors[2]),
            "]",
        "}");
        if (r) goto error;

        snprintf(feature->img, sizeof(feature->img), "%s", img);
        for (i = 0; i < 3; i++) {
            r = jcon_parse(anchors[i], "{",
                "pos", "[", JCON_INT(x, 0), JCON_INT(y, 0), "]",
                "hip", JCON_INT(hip, 0),
            "}");
            if (r) goto error;
            feature->anchors[i].hip = hip;
            feature->anchors[i].uv[0] = (double)x / w;
            feature->anchors[i].uv[1] = (double)y / h;
        }
    }
    return 0;

error:
    LOG_E("Cannot parse json feature");
    return -1;
}

skyculture_name_t *skyculture_parse_names_json(const json_value *v)
{
    int i, j, r;
    const char *key;
    skyculture_name_t *ret = NULL, *entry, *prev_entry;
    const json_value *names_obj;
    const char *english = NULL, *native = NULL, *pronounce = NULL;

    if (v->type != json_object) goto error;
    for (i = 0; i < v->u.object.length; i++) {
        key = v->u.object.values[i].name;

        if (v->u.object.values[i].value->type != json_array)
            goto error;

        for (j = 0; j < v->u.object.values[i].value->u.array.length; j++) {
            names_obj = v->u.object.values[i].value->u.array.values[j];

            r = jcon_parse(names_obj, "{",
                "?english", JCON_STR(english),
                "?native", JCON_STR(native),
                "?pronounce", JCON_STR(pronounce),
            "}");
            if (r) goto error;
            entry = calloc(1, sizeof(*entry));
            entry->main_id = strdup(key);
            if (english)
                entry->name_english = strdup(english);
            if (native)
                entry->name_native = strdup(native);
            if (pronounce)
                entry->name_pronounce = strdup(pronounce);

            if (j > 0) {
                prev_entry->alternative = entry;
            } else {
                HASH_ADD_KEYPTR(hh, ret, entry->main_id,
                                strlen(entry->main_id), entry);
            }
            prev_entry = entry;
        }
    }

    return ret;

error:

    LOG_E("Cannot parse skyculture names");
    return NULL;
}
