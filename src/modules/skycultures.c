/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include "ini.h"
#include "md4c.h"
#include "md4c-html.h"
#include <regex.h>

/*
 * Module that handles the skycultures list.
 */

/*
 * Enum of all the data files we need to parse.
 */
enum {
    SK_JSON                         = 1 << 0,
    SK_MD                           = 1 << 1,
};

/*
 * Type: skyculture_t
 * Represent an individual skyculture.
 */
typedef struct skyculture {
    obj_t           obj;
    char            *key;   // The key passed to add_data_source.
    char            *uri;
    int             nb_constellations;
    skyculture_name_t *names; // Hash table of identifier -> common names.
    constellation_infos_t *constellations;
    json_value      *imgs;
    int             parsed; // union of SK_ enum for each parsed file.
    json_value      *tour;

    // True if the common names for this sky culture should fallback to the
    // international names. Useful for cultures based on the western family.
    bool fallback_to_international_names;
    // True if this sky culture contains chinese bayer-like star names
    bool has_chinese_star_names;
    // True if the english reference name for translation should be the native
    // name.
    bool english_use_native_names;
    // True if we should display native names first for the current language.
    bool prefer_native_names;


    // The following strings are all english text, with a matching translation
    // in the associated sky culture translations files.
    char            *name;         // name (plain text)
    char            *region;       // region (plain text)
    char            *introduction; // introduction (html)
    char            *description;  // description if any (html)
    char            *references;   // references if any (html)
    char            *authors;      // authors (html)
    char            *licence;      // licence (html)
} skyculture_t;

/*
 * Enum: NAME_FORMAT_STYLE
 * Flags that specify how to format a sky culture common name.
 *
 * Values:
 *   NAME_AUTO          - Return the translated version of the english common
 *                        name, fallback to pronounciation if not available,
 *                        and native if not available.
 *                        In the case where the current language prefer to see
 *                        the native names, return the untranslated native name.
 *                        In the special case where english is the native
 *                        language, return the translated native name first.
 *
 *   NAME_NATIVE        - Return the native name if available, e.g. the chinese
 *                        character version of the name, fallback to
 *                        pronounciation if not available.
 *   NAME_NATIVE_AND_PRONOUNCE - Return native name + pronounciation if
 *                        available.
 */
enum {
    NAME_AUTO                  = 0,
    NAME_NATIVE                = 1 << 0,
    NAME_NATIVE_AND_PRONOUNCE  = 1 << 1
};

/*
 * Type: skycultures_t
 * The module, that maintains the list of skycultures.
 */
typedef struct skycultures_t {
    obj_t   obj;
    skyculture_t *current; // The current skyculture.
    int     name_format_style;
    regex_t chinese_re;
} skycultures_t;

// Static instance.
static skycultures_t *g_skycultures = NULL;

static void skyculture_deactivate(skyculture_t *cult)
{
    obj_t *constellations, *cst, *tmp;
    // Remove all the constellation objects.
    constellations = core_get_module("constellations");
    assert(constellations);
    DL_FOREACH_SAFE(constellations->children, cst, tmp) {
        module_remove(constellations, cst);
    }
}

static void skyculture_activate(skyculture_t *cult)
{
    int i;
    json_value *args;
    constellation_infos_t *cst;
    obj_t *constellations;

    // Create all the constellations object.
    constellations = core_get_module("constellations");
    assert(constellations);
    for (i = 0; i < cult->nb_constellations; i++) {
        cst = &cult->constellations[i];
        if (DEBUG) {
            char id[256];
            snprintf(id, sizeof(id), "CST %s", cst->id);
            assert(!module_get_child(constellations, id));
        }
        args = json_object_new(0);
        json_object_push(args, "info_ptr", json_integer_new((int64_t)cst));
        module_add_new(constellations, "constellation", args);
        json_builder_free(args);
    }

    // Set the current attribute of the skycultures manager object.
    obj_set_attr(cult->obj.parent, "current", cult);
    module_changed(cult->obj.parent, "current_id");
}

static int skyculture_update(obj_t *obj, double dt);
static skyculture_t *add_from_uri(skycultures_t *cults, const char *uri,
                                  const char *key)
{
    skyculture_t *cult;
    cult = (void*)module_add_new(&cults->obj, "skyculture", NULL);
    cult->key = strdup(key);
    cult->uri = strdup(uri);
    cult->obj.id = cult->key;
    skyculture_update((obj_t*)cult, 0);
    return cult;
}

static void process_output(const MD_CHAR* str, MD_SIZE size, void* user)
{
    UT_string *instr = (UT_string*)user;
    UT_string s;
    utstring_init(&s);
    utstring_bincpy(&s, str, size);
    utstring_concat(instr, &s);
    utstring_done(&s);
}

static void utstring_replace(UT_string *s, int start, int end, const char* rep)
{
    UT_string s2;
    const char* in_str = utstring_body(s);
    utstring_init(&s2);
    utstring_bincpy(&s2, in_str, start);
    utstring_bincpy(&s2, rep, strlen(rep));
    utstring_bincpy(&s2, in_str + end, strlen(in_str + end));
    utstring_clear(s);
    utstring_concat(s, &s2);
    utstring_done(&s2);
}

char *skycultures_md_2_html(const char *md)
{
    UT_string s, s2;
    char *ret = NULL;
    const char* str;
    char tmp[256];
    int n;
    regex_t re;
    regmatch_t m[2];

    utstring_init(&s);
    utstring_printf(&s, "%s", md);

    // Apply custom rules for references
    // Replace reference list with proper HTML refs, i.e.:
    // replace - [#1]: XXXX   by linkable span
    regcomp(&re, "^ - \\[#([0-9]+)\\]: ", REG_EXTENDED | REG_NEWLINE);
    while (regexec(&re, utstring_body(&s), 2, m, 0) == 0) {
        str = utstring_body(&s);
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, str + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
        n = atoi(tmp);
        snprintf(tmp, sizeof(tmp), " - <span id=\"cite_%d\">[%d]</span> ",
                 n, n);
        utstring_replace(&s, m[0].rm_so, m[0].rm_eo, tmp);
    }
    regfree(&re);

    utstring_init(&s2);
    md_html(utstring_body(&s), utstring_len(&s), process_output, &s2,
            MD_DIALECT_GITHUB, 0);

    // Replace reference links with proper HTML links
    regcomp(&re, " ?\\[#([0-9]+)\\]", REG_EXTENDED | REG_NEWLINE);
    while (regexec(&re, utstring_body(&s2), 2, m, 0) == 0) {
        str = utstring_body(&s2);
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, str + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
        n = atoi(tmp);
        snprintf(tmp, sizeof(tmp), "<sup>[%d]</sup>", n);
        utstring_replace(&s2, m[0].rm_so, m[0].rm_eo, tmp);
    }
    regfree(&re);

    ret = strdup(utstring_body(&s2));
    utstring_done(&s);
    utstring_done(&s2);

    return ret;
}

static char *to_buf(const char *text, int size)
{
    if (size <= 0) return NULL;
    char *buf = malloc(size + 1);
    if (!buf) return NULL;
    memcpy(buf, text, size);
    buf[size] = '\0';
    return buf;

}

static const char *skyculture_cst_id_for_name(const char *name,
                                              const skyculture_t *cult)
{
    skyculture_name_t *item, *tmp;
    HASH_ITER(hh, cult->names, item, tmp) {
        if ((item->name_english && strcmp(item->name_english, name) == 0) ||
            (item->name_native && strcmp(item->name_native, name) == 0) ||
            (item->name_pronounce && strcmp(item->name_pronounce, name) == 0)) {
            return item->main_id;
        }
    }
    return NULL;
}

static constellation_infos_t *skyculture_cst_info_for_id(const char *id,
                                                    const skyculture_t *cult)
{
    int i;
    for (i = 0; i < cult->nb_constellations; ++i) {
        constellation_infos_t *cst_info = &cult->constellations[i];
        if (strcmp(cst_info->id, id) == 0)
            return cst_info;
    }
    return NULL;
}

static void set_constellation_md_data(const char *cstname, const char *content,
                                      int size, skyculture_t *cult)
{
    constellation_infos_t *cst_info;
    const char* cst_id = skyculture_cst_id_for_name(cstname, cult);
    if (!cst_id)
        return;
    cst_info = skyculture_cst_info_for_id(cst_id, cult);
    if (!cst_info)
        return;

    while (*(content + size - 1) == '\n' && size)
        size--;
    cst_info->description = to_buf(content, size);
}

static void load_constellation_md_data(const char *md, skyculture_t *cult)
{
    char cid[256];
    cid[0] = '\0';
    regex_t re;
    regmatch_t m[2];
    const char *cur = md;

    regcomp(&re, "^##### +(.+)$", REG_EXTENDED | REG_NEWLINE);
    while (regexec(&re, cur, 2, m, 0) == 0) {
        if (cid[0]) {
            set_constellation_md_data(cid, cur, m[0].rm_so, cult);
        }
        snprintf(cid, sizeof(cid), "%.*s", (int)(m[1].rm_eo - m[1].rm_so),
                 cur + m[1].rm_so);
        cur += m[0].rm_eo + 1;
        while (*cur == '\n')
            cur ++;
    }
    regfree(&re);
    if (cid[0]) {
        set_constellation_md_data(cid, cur, md + strlen(md) - cur, cult);
    }
}

static void add_section(const char *section_name, const char *content,
                        int size, skyculture_t *cult)
{
    const char* cst_md_data;
    while (*(content + size - 1) == '\n' && size)
        size--;
    if (strcmp(section_name, "Introduction") == 0) {
        cult->introduction = to_buf(content, size);
    } else if (strcmp(section_name, "Description") == 0) {
        cult->description = to_buf(content, size);
    } else if (strcmp(section_name, "References") == 0) {
        cult->references = to_buf(content, size);
    } else if (strcmp(section_name, "Authors") == 0) {
        cult->authors = to_buf(content, size);
    } else if (strcmp(section_name, "License") == 0) {
        cult->licence = to_buf(content, size);
    } else if (strcmp(section_name, "Constellations") == 0) {
        cst_md_data = to_buf(content, size);
        load_constellation_md_data(cst_md_data, cult);
        free(cst_md_data);
    } else if (strcmp(section_name, "Extras") == 0) {
        // Currently ignores this section
    } else {
        LOG_W("Error in sky culture %s: ", cult->key);
        LOG_W("unknown level 2 section: %s", section_name);
    }
}

static void add_markdown(const char *md, skyculture_t *cult)
{
    regex_t re;
    regcomp(&re, "^# +(.+)$", REG_EXTENDED | REG_NEWLINE);
    regmatch_t m[2];
    char section_name[256];
    section_name[0] = '\0';
    if (regexec(&re, md, 2, m, 0) == 0) {
        snprintf(section_name, sizeof(section_name), "%.*s",
                 (int)(m[1].rm_eo - m[1].rm_so), md + m[1].rm_so);
        cult->name = strdup(section_name);
    } else {
        LOG_E("Error in sky culture %s: ", cult->key);
        LOG_E("markdown must start with # Sky Culture Name");
        cult->name = strdup("Unknown");
    }
    regfree(&re);

    section_name[0] = '\0';
    regcomp(&re, "^## +(.+)$", REG_EXTENDED | REG_NEWLINE);
    const char *cur = md;

    while (regexec(&re, cur, 2, m, 0) == 0) {
        if (section_name[0]) {
            add_section(section_name, cur, m[0].rm_so, cult);
        }
        snprintf(section_name, sizeof(section_name), "%.*s",
                 (int)(m[1].rm_eo - m[1].rm_so), cur + m[1].rm_so);
        cur += m[0].rm_eo + 1;
        while (*cur == '\n')
            cur ++;
    }
    regfree(&re);
    if (section_name[0]) {
        add_section(section_name, cur, md + strlen(md) - cur, cult);
    }
}

static int skyculture_load_md(skyculture_t *cult) {
    char path[1024];
    const char *md;
    int code;
    const skycultures_t *cults = (skycultures_t*)cult->obj.parent;
    bool active = (cult == cults->current);

    // JSON is already parsed, load the markdown
    snprintf(path, sizeof(path), "%s/%s", cult->uri, "description.md");
    md = asset_get_data2(path, ASSET_USED_ONCE, NULL, &code);
    if (!code) return 0;
    cult->parsed |= SK_MD;

    if (!md) {
        LOG_E("Failed to download skyculture markdown file");
        return -1;
    }

    add_markdown(md, cult);

    // Once all has been parsed, we can activate the skyculture.
    if (active) skyculture_activate(cult);
    return 0;
}

static int skyculture_update(obj_t *obj, double dt)
{
    const char *json;
    skyculture_t *cult = (skyculture_t*)obj;
    char path[1024], *name, *region, *id;
    int code, r;
    unsigned int i;
    json_value *doc;
    const json_value *names = NULL, *features = NULL,
                     *tour = NULL, *edges = NULL,
                     *langs_use_native_names = NULL;
    const char *description = NULL, *introduction = NULL,
               *references = NULL, *authors = NULL, *licence = NULL;
    const char* langname;

    constellation_infos_t *cst_info;

    if (cult->parsed & SK_MD)
        return 0;

    if (cult->parsed & SK_JSON) {
        return skyculture_load_md(cult);
    }

    snprintf(path, sizeof(path), "%s/%s", cult->uri, "index.json");
    json = asset_get_data2(path, ASSET_USED_ONCE, NULL, &code);
    if (!code) return 0;
    cult->parsed |= SK_JSON;

    if (!json) {
        LOG_E("Failed to download skyculture json file");
        return -1;
    }

    doc = json_parse(json, strlen(json));
    if (!doc) {
        LOG_E("Cannot parse skyculture json (%s)", path);
        return -1;
    }

    r = jcon_parse(doc, "{",
        "id", JCON_STR(id),
        "?name", JCON_STR(name),
        "region", JCON_STR(region),
        "?fallback_to_international_names",
                   JCON_BOOL(cult->fallback_to_international_names, 0),
        "?langs_use_native_names", JCON_VAL(langs_use_native_names),
        "?common_names", JCON_VAL(names),
        "?constellations", JCON_VAL(features),
        "?introduction", JCON_STR(introduction),
        "?description", JCON_STR(description),
        "?references", JCON_STR(references),
        "?authors", JCON_STR(authors),
        "?licence", JCON_STR(licence),
        "?edges", JCON_VAL(edges),
        "?tour", JCON_VAL(tour),
    "}");
    if (r) {
        LOG_E("Cannot parse skyculture json (%s)", path);
        json_value_free(doc);
        return -1;
    }

    assert(strcmp(id, cult->key) == 0); // For the moment.
    cult->has_chinese_star_names = strncmp(id, "chinese", 7) == 0;
    if (name)
        cult->name = strdup(name);
    cult->region = strdup(region);
    if (introduction)
        cult->introduction = strdup(introduction);
    if (description)
        cult->description = strdup(description);
    if (references)
        cult->references = strdup(references);
    if (authors)
        cult->authors = strdup(authors);
    if (licence)
        cult->licence = strdup(licence);
    if (names) cult->names = skyculture_parse_names_json(names);
    if (tour) cult->tour = json_copy(tour);

    if (langs_use_native_names) {
        for (i = 0; i < langs_use_native_names->u.array.length; i++) {
            langname = langs_use_native_names->u.array.values[i]->u.string.ptr;
            if (strcmp(langname, "en") == 0)
                cult->english_use_native_names = true;
            if (strcmp(sys_get_lang(), langname) == 0)
                cult->prefer_native_names = true;
        }
    }

    if (!features) goto end;
    cult->constellations = calloc(features->u.array.length,
                                  sizeof(*cult->constellations));
    for (i = 0; i < features->u.array.length; i++) {
        cst_info = &cult->constellations[cult->nb_constellations];
        r = skyculture_parse_feature_json(
                &cult->names,
                features->u.array.values[i],
                cst_info);
        cst_info->base_path = cult->uri;
        if (r) continue;
        cult->nb_constellations++;
    }

    if (edges) {
        skyculture_parse_edges(edges, cult->constellations,
                               cult->nb_constellations);
    }

end:
    json_value_free(doc);

    // Immediately tries to load the md file if available
    return skyculture_load_md(cult);
}

static void skycultures_gui(obj_t *obj, int location)
{
    skyculture_t *cult;
    skycultures_t *cults = (void*)obj;
    bool active = false;
    bool res;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Skycultures")) {
        MODULE_ITER(obj, cult, "skyculture") {
            if (!cult->name) continue;
            if (strcmp(cult->name, cults->current->name) == 0) {
                active = true;
            } else {
                active = false;
            }
            res = gui_toggle(cult->name, &active);
            if (res) {
                obj_set_attr((obj_t*)cults, "current_id", cult->key);
            }
        }
        gui_tab_end();
    }
}

static int skycultures_update(obj_t *obj, double dt)
{
    obj_t *skyculture;
    MODULE_ITER(obj, skyculture, "skyculture") {
        skyculture_update(skyculture, dt);
    }
    return 0;
}

static int skycultures_init(obj_t *obj, json_value *args)
{
    skycultures_t *scs = (skycultures_t*)obj;
    regcomp(&scs->chinese_re, " [MDCLXVI]+$", REG_EXTENDED);
    assert(!g_skycultures);
    g_skycultures = scs;
    return 0;
}

static int skycultures_add_data_source(
        obj_t *obj, const char *url, const char *key)
{
    skycultures_t *cults = (void*)obj;
    skyculture_t *cult;

    // Skip if we already have it.
    if (module_get_child(&cults->obj, key)) return -1;
    cult = add_from_uri(cults, url, key);
    if (!cult) LOG_W("Cannot add skyculture (%s)", url);
    // If it's the default skyculture (western) activate it immediatly.
    if (str_endswith(url, "western")) {
        obj_set_attr((obj_t*)cults, "current_id", "western");
        skyculture_update((obj_t*)cult, 0);
    }
    return 0;
}

/*
 * Function: skycultures_get_name_info
 * Get the names of a sky object in the current skyculture.
 *
 * Parameters:
 *   main_id        - the main ID of the sky object:
 *                     - for bright stars use "HIP XXXX"
 *                     - for constellations use "CON culture_name XXX"
 *                     - for planets use "NAME Planet"
 *                     - for DSO use the first identifier of the names list
 *
 * Return:
 *   NULL if no name was found, else a pointer to a skyculture_name_t struct.
 */
static const skyculture_name_t *skycultures_get_name_info(const char* main_id)
{
    const skyculture_t *cult = g_skycultures->current;
    const skyculture_name_t *entry;

    assert(main_id);

    if (!cult) return NULL;

    HASH_FIND_STR(cult->names, main_id, entry);
    return entry;
}

/*
 * Function: parse_roman_numeral
 * Parse a roman numeral string into an int
 *
 * Return 0 in case of error.
 */
static int parse_roman_numeral(const char *roman)
{
    int v = 0;
    const char *p = roman;

    /**/ if (strncmp(p, "XC",   2) == 0) { v += 90; p += 2; }
    else if (strncmp(p, "LXXX", 4) == 0) { v += 80; p += 4; }
    else if (strncmp(p, "LXX",  3) == 0) { v += 70; p += 3; }
    else if (strncmp(p, "LX",   2) == 0) { v += 60; p += 2; }
    else if (strncmp(p, "L",    1) == 0) { v += 50; p += 1; }
    else if (strncmp(p, "XL",   2) == 0) { v += 40; p += 2; }
    else if (strncmp(p, "XXX",  3) == 0) { v += 30; p += 3; }
    else if (strncmp(p, "XX",   2) == 0) { v += 20; p += 2; }
    else if (strncmp(p, "X",    1) == 0) { v += 10; p += 1; }

    /**/ if (strncmp(p, "IX",   2) == 0) { v += 9;  p += 2; }
    else if (strncmp(p, "VIII", 4) == 0) { v += 8;  p += 4; }
    else if (strncmp(p, "VII",  3) == 0) { v += 7;  p += 3; }
    else if (strncmp(p, "VI",   2) == 0) { v += 6;  p += 2; }
    else if (strncmp(p, "V",    1) == 0) { v += 5;  p += 1; }
    else if (strncmp(p, "IV",   2) == 0) { v += 4;  p += 2; }
    else if (strncmp(p, "III",  3) == 0) { v += 3;  p += 3; }
    else if (strncmp(p, "II",   2) == 0) { v += 2;  p += 2; }
    else if (strncmp(p, "I",    1) == 0) { v += 1;  p += 1; }

    (void)p; // Fix clang warning.
    return v;
}

/*
 * Function: roman_to_chinese
 * Convert a Roman numeral to Chinese
 */
static void roman_to_chinese(const char *roman, char *out, int len)
{
    int v, d;
    const char *chars = "十一二三四五六七八九";
    assert(strlen(chars) == 30); // 3 bytes per character.
    assert(len > 64);

    v = parse_roman_numeral(roman);
    assert(v < 100);

    d = v / 10;
    v = v % 10;
    if (d >= 2) {
        memcpy(out, chars + d * 3, 3);
        out += 3;
        d = 1;
    }
    if (d == 1) {
        memcpy(out, chars, 3);
        out += 3;
    }
    if (v) {
        memcpy(out, chars + v * 3, 3);
    }
}

/*
 * Function: skycultures_translate_english_name
 * Translate a sky object cultural english name in the current locale.
 *
 * Sky object in a chinese sky culture need a special translation function
 * because they have the following format:
 * "Constellation_name [Added] [number]" and only the first part
 * (Constellation_name) is stored in the translation DB.
 *
 * Parameters:
 *   name           - the english name as stored in a skyculture_name_t
 *   out            - A text buffer that get filled with the name.
 *   out_size       - size of the out buffer.
 */
static void skycultures_translate_english_name(const char* name, char *out,
                                        int out_size)
{
    const skyculture_t *cult = g_skycultures->current;
    regmatch_t m;
    const char *tr_name;
    const char *added, *number = NULL;
    char tmp[256], chinese_num[128] = {};

    assert(name);

    if (!cult || !cult->has_chinese_star_names) {
        tr_name = sys_translate("skyculture", name);
        snprintf(out, out_size, "%s", tr_name);
        return;
    }

    if (regexec(&g_skycultures->chinese_re, name, 1, &m, 0) == 0) {
        number = name + m.rm_so;
        // In Chinese we show Chinese number instead of Roman.
        if (strncmp(sys_get_lang(), "zh", 2) == 0) {
            roman_to_chinese(number + 1, chinese_num, sizeof(chinese_num));
            number = chinese_num;
        }
    }
    added = strstr(name, " Added");
    if (added) {
        snprintf(tmp, sizeof(tmp), "%.*s", (int)(added - name), name);
        tr_name = sys_translate("skyculture", tmp);
        snprintf(out, out_size, "%s %s%s", tr_name,
                 sys_translate("skyculture", "Added"),
                 number ?: "");
        return;
    } else if (number) {
        snprintf(tmp, sizeof(tmp), "%.*s", (int)m.rm_so, name);
        tr_name = sys_translate("skyculture", tmp);
        snprintf(out, out_size, "%s%s", tr_name, number);
        return;
    }

    tr_name = sys_translate("skyculture", name);
    snprintf(out, out_size, "%s", tr_name);
}

/*
 * Function: skycultures_get_label
 * Get the label of a sky object in the current skyculture, translated
 * for the current language.
 *
 * Parameters:
 *   main_id        - the main ID of the sky object:
 *                     - for bright stars use "HIP XXXX"
 *                     - for constellations use "CON culture_name XXX"
 *                     - for planets use "NAME Planet"
 *                     - for DSO use the first identifier of the names list
 *   out            - A text buffer that get filled with the name.
 *   out_size       - size of the out buffer.
 *
 * Return:
 *   NULL if no name was found.  A pointer to the passed buffer otherwise.
 */
const char *skycultures_get_label(const char* main_id, char *out, int out_size)
{
    const skyculture_t *cult = g_skycultures->current;
    const skyculture_name_t *entry = skycultures_get_name_info(main_id);

    if (!cult || !entry) return NULL;

    switch (g_skycultures->name_format_style) {
    case NAME_AUTO:
        if (cult->prefer_native_names) {
            if (entry->name_native) {
                snprintf(out, out_size, "%s", entry->name_native);
                return out;
            }
        } else if (cult->english_use_native_names && entry->name_native) {
            skycultures_translate_english_name(entry->name_native, out,
                                               out_size);
            return out;
        }
        if (entry->name_english) {
            skycultures_translate_english_name(entry->name_english, out,
                                               out_size);
            return out;
        }
        if (entry->name_pronounce) {
            snprintf(out, out_size, "%s", entry->name_pronounce);
            return out;
        }
        if (entry->name_native) {
            snprintf(out, out_size, "%s", entry->name_native);
            return out;
        }
        return NULL;
    case NAME_NATIVE_AND_PRONOUNCE:
        if (entry->name_native && entry->name_pronounce) {
            snprintf(out, out_size, "%s (%s)", entry->name_native,
                     entry->name_pronounce);
            return out;
        }
        // If not both are present fallback to NAME_NATIVE
    case NAME_NATIVE:
        if (entry->name_native) {
            snprintf(out, out_size, "%s", entry->name_native);
            return out;
        }
        if (entry->name_pronounce) {
            snprintf(out, out_size, "%s", entry->name_pronounce);
            return out;
        }
    }
    return NULL;
}

static void add_one_sc_names_(const skyculture_name_t *entry, const obj_t *obj,
                             void *user, void (*f)(const obj_t *obj, void *user,
                                                   const char *dsgn))
{
    char tmp[256];
    const skyculture_t *cult = g_skycultures->current;

    if (!cult) return;
    if (cult->prefer_native_names) {
        if (entry->name_native) {
            f(obj, user, entry->name_native);
        }
        if (entry->name_english) {
            skycultures_translate_english_name(entry->name_english, tmp,
                                               sizeof(tmp));
            f(obj, user, tmp);
        }
        if (entry->name_pronounce) {
            f(obj, user, entry->name_pronounce);
        }
        if (entry->alternative) {
            add_one_sc_names_(entry->alternative, obj, user, f);
        }
        return;
    }

    if (cult->english_use_native_names) {
        if (entry->name_native) {
            skycultures_translate_english_name(entry->name_native, tmp,
                                               sizeof(tmp));
            f(obj, user, tmp);
            if (strcmp(tmp, entry->name_native) != 0)
                f(obj, user, entry->name_native);
        }
        if (entry->name_pronounce) {
            f(obj, user, entry->name_pronounce);
        }
        if (entry->alternative) {
            add_one_sc_names_(entry->alternative, obj, user, f);
        }
        return;
    }

    if (entry->name_english) {
        skycultures_translate_english_name(entry->name_english, tmp,
                                           sizeof(tmp));
        f(obj, user, tmp);
    }
    if (entry->name_pronounce) {
        f(obj, user, entry->name_pronounce);
    }
    if (entry->name_native) {
        f(obj, user, entry->name_native);
    }
    if (entry->alternative) {
        add_one_sc_names_(entry->alternative, obj, user, f);
    }
}

static void on_designation_(const obj_t *obj, void *user2, const char *dsgn)
{
    void *user = USER_GET(user2, 0);
    void (*f)(const obj_t *obj, void *user, const char *dsgn) =
            USER_GET(user2, 1);

    // Check if we get cultural names for this object.
    const skyculture_name_t *entry = skycultures_get_name_info(dsgn);
    if (entry)
        add_one_sc_names_(entry, obj, user, f);
}

static void on_designation_sky_(const obj_t *obj, void *user2, const char *dsgn)
{
    void *user = USER_GET(user2, 0);
    void (*f)(const obj_t *obj, void *user, const char *dsgn) =
            USER_GET(user2, 1);
    char buf[256];
    const char* i18n;

    // Translation for international sky objects
    if (strncmp(dsgn, "NAME ", 5) == 0) {
        i18n = sys_translate("sky", dsgn + 5);
        if (strcmp(i18n, dsgn + 5) != 0) {
            snprintf(buf, sizeof(buf), "NAME %s", i18n);
            f(obj, user, buf);
            return;
        }
    }

    f(obj, user, dsgn);
}

EMSCRIPTEN_KEEPALIVE
void skycultures_get_designations(const obj_t *obj, void *user,
                  void (*f)(const obj_t *obj, void *user, const char *dsgn))
{
    obj_get_designations(obj, USER_PASS(user, f), on_designation_);
    obj_get_designations(obj, USER_PASS(user, f), on_designation_sky_);
}

static void add_one_cn(const skyculture_name_t *entry, const obj_t *obj,
                             void *user, void (*f)(const obj_t *obj, void *user,
                                                   const cultural_name_t *cn))
{
    char tmp[256];
    const skyculture_t *cult = g_skycultures->current;

    if (!cult) return;

    cultural_name_t cn = {0};

    // User with the current language prefer to use native names
    cn.user_prefer_native = cult->prefer_native_names;
    cn.name_native = entry->name_native;
    cn.name_pronounce = entry->name_pronounce;
    cn.name_english = entry->name_english;
    tmp[0] = '\0';
    if (cult->prefer_native_names && entry->name_english) {
        skycultures_translate_english_name(entry->name_english, tmp,
                                           sizeof(tmp));
    } else if (cult->english_use_native_names && entry->name_native) {
        skycultures_translate_english_name(entry->name_native, tmp,
                                           sizeof(tmp));
    } else {
        if (entry->name_english)
            skycultures_translate_english_name(entry->name_english, tmp,
                                               sizeof(tmp));
    }
    cn.name_translated = tmp[0] ? tmp : NULL;
    f(obj, user, &cn);
    if (entry->alternative) {
        add_one_cn(entry->alternative, obj, user, f);
    }
}

static void on_designation_cn(const obj_t *obj, void *user2, const char *dsgn)
{
    void *user = USER_GET(user2, 0);
    void (*f)(const obj_t *obj, void *user, const cultural_name_t *cn) =
            USER_GET(user2, 1);

    // Check if we get cultural names for this object.
    const skyculture_name_t *entry = skycultures_get_name_info(dsgn);
    if (entry)
        add_one_cn(entry, obj, user, f);
}

void skycultures_get_cultural_names(const obj_t *obj, void *user,
              void (*f)(const obj_t *obj, void *user,
                        const cultural_name_t *cn))
{
    obj_get_designations(obj, USER_PASS(user, f), on_designation_cn);
}

static void on_cultural_name_(const obj_t *obj, void *user,
                              const cultural_name_t *cn)
{
    json_value *ret = user;
    json_value *val = json_object_new(0);

    #define X(attr) if (cn->attr) \
        json_object_push(val, #attr, json_string_new(cn->attr));
    X(name_native);
    X(name_english);
    X(name_pronounce);
    X(name_translated);
    #undef X
    json_object_push(val, "user_prefer_native",
                     json_boolean_new(cn->user_prefer_native));
    json_array_push(ret, val);
}

// Same as skycultures_get_cultural_names but returns a json array.  Used
// for the js binding only.
EMSCRIPTEN_KEEPALIVE
char *skycultures_get_cultural_names_json(const obj_t *obj)
{
    json_value *jret;
    char *ret;
    int size;
    jret = json_array_new(0);
    skycultures_get_cultural_names(obj, jret, on_cultural_name_);
    size = json_measure(jret);
    ret = calloc(1, size);
    json_serialize(ret, jret);
    json_builder_free(jret);
    return ret;
}

/*
 * Function: skycultures_fallback_to_international_names
 * Return whether a sky culture includes the international sky objects names as
 * as fallback when no common names is explicitly specified for a given object.
 *
 * Return:
 *   True or false.
 */
bool skycultures_fallback_to_international_names()
{
    if (!g_skycultures->current)
        return false;
    return g_skycultures->current->fallback_to_international_names;
}

// Set/Get the current skyculture by id.
static json_value *skycultures_current_id_fn(
        obj_t *obj, const attribute_t *attr, const json_value *args)
{
    char id[128];
    skycultures_t *cults = (skycultures_t*)obj;
    skyculture_t *cult;
    if (args && args->u.array.length) {
        args_get(args, TYPE_STRING, id);
        MODULE_ITER(cults, cult, "skyculture") {
            if (strcmp(cult->key, id) == 0) {
                if (cults->current) skyculture_deactivate(cults->current);
                skyculture_activate(cult);
                break;
            }
        }
    }
    if (!cults->current) return args_value_new(TYPE_STRING, "");
    return args_value_new(TYPE_STRING, cults->current->key);
}

/*
 * Meta class declarations.
 */
static obj_klass_t skyculture_klass = {
    .id     = "skyculture",
    .size   = sizeof(skyculture_t),
    .flags  = 0,
    .update = skyculture_update,
    .attributes = (attribute_t[]) {
        PROPERTY(name, TYPE_STRING_PTR, MEMBER(skyculture_t, name)),
        PROPERTY(region, TYPE_STRING_PTR, MEMBER(skyculture_t, region)),
        PROPERTY(fallback_to_international_names, TYPE_BOOL,
                 MEMBER(skyculture_t, fallback_to_international_names)),
        PROPERTY(introduction, TYPE_STRING_PTR,
                 MEMBER(skyculture_t, introduction)),
        PROPERTY(description, TYPE_STRING_PTR,
                 MEMBER(skyculture_t, description)),
        PROPERTY(references, TYPE_STRING_PTR, MEMBER(skyculture_t, references)),
        PROPERTY(authors, TYPE_STRING_PTR, MEMBER(skyculture_t, authors)),
        PROPERTY(licence, TYPE_STRING_PTR, MEMBER(skyculture_t, licence)),
        PROPERTY(url, TYPE_STRING_PTR, MEMBER(skyculture_t, uri)),
        PROPERTY(tour, TYPE_JSON, MEMBER(skyculture_t, tour)),
        {}
    },
};
OBJ_REGISTER(skyculture_klass)


static obj_klass_t skycultures_klass = {
    .id             = "skycultures",
    .size           = sizeof(skycultures_t),
    .flags          = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .gui            = skycultures_gui,
    .init           = skycultures_init,
    .update         = skycultures_update,
    .add_data_source    = skycultures_add_data_source,
    .create_order   = 30, // After constellations.
    .attributes = (attribute_t[]) {
        PROPERTY(current, TYPE_OBJ, MEMBER(skycultures_t, current)),
        PROPERTY(current_id, TYPE_STRING, .fn = skycultures_current_id_fn),
        PROPERTY(name_format_style, TYPE_INT,
                 MEMBER(skycultures_t, name_format_style)),
        {}
    },
};
OBJ_REGISTER(skycultures_klass)
