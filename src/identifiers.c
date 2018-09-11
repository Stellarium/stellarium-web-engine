/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "identifiers.h"

#include "utarray.h"
#include "uthash.h"
#include "utlist.h"
#include "utils/utils.h"
#include "utils/utf8.h"
#include "tests.h"

#include <assert.h>
#include <regex.h>

/*
 *                                          (cannonical)
 *      oid             CAT   value         search_value   show_value
 *      ----------      ---   ---------     ------------   ----------
 *      hd 8890         hd    8890          HD 8890        HD 8890
 *      hd 8890         name  Polaris       POLARIS        Polaris
 *      hd 8890         bayer alf Umi       ALF UMI        Alpha Umi
 *      ngc 2632        m     44            M 44           M 44
 *
 */

typedef struct entry entry_t;
struct entry {
    entry_t *next, *prev; // Global id list, grouped by id.
    UT_hash_handle hh;    // Hash of id to first matching entry.
    uint64_t oid;
    char *cat;
    char *value;
    char *search_value;
    char *show_value;
};

static entry_t *g_entries = NULL; // Global name entries array.
static entry_t *g_idx = NULL;     // Id index hash.

// Regex used to split string into tokens.
static const char *TOKEN_RE = "([0-9]+)|([^[:blank:]0-9]+)";
static regex_t g_token_reg;


static const char *ACCENTS =
    "āA áA ǎA àA "
    "ĀA ÁA ǍA ÀA "
    "ēE éE ěE èE "
    "ĒE ÉE ĚE ÈE "
    "īI íI ǐI ìI "
    "ĪI ÍI ǏI ÌI "
    "ōO óO ǒO òO "
    "ŌO ÓO ǑO ÒO "
    "ūU úU ǔU ùU ǖU ǘU ǚU ǜU üU "
    "ŪU ÚU ǓU ÙU ǕU ǗU ǙU ǛU ÜU ";


// Replacement rules for canonical names.
static const char *CANONICAL_RULES[][2] = {
    {"α", "ALF"}, {"β", "BET"}, {"γ", "GAM"}, {"δ", "DEL"}, {"ε", "EPS"},
    {"ζ", "ZET"}, {"η", "ETA"}, {"θ", "TET"}, {"ι", "IOT"}, {"κ", "KAP"},
    {"λ", "LAM"}, {"μ", "MU"}, {"ν", "NU"}, {"ξ", "XI"}, {"ο", "OMI"},
    {"π", "PI"}, {"ρ", "RHO"}, {"σ", "SIG"}, {"τ", "TAU"}, {"υ", "UPS"},
    {"φ", "PHI"}, {"χ", "CHI"}, {"ψ", "PSI"}, {"ω", "OME"},

    {"ALPHA", "ALF"}, {"BETA", "BET"}, {"GAMMA", "GAM"}, {"DELTA", "DEL"},
    {"EPSILON", "EPS"},
    {"ZETA", "ZET"}, {"THETA", "TET"}, {"IOTA", "IOT"}, {"KAPPA", "KAP"},
    {"LAMBDA", "LAM"}, {"OMICRON", "OMI"}, {"SIGMA", "SIG"},
    {"UPSILON", "UPS"}, {"OMEGA", "OME"},
};

// Upper case, but also replace accent by non accentuated characters.
static int canonical_upper(const char *s, char *out, int n)
{
    int len;
    int r = 0;
    const char *p;
    while (*s) {
        len = u8_char_len(s);
        if (len >= n) {r = 1; break;}
        if (len == 1) { // Ascii character.
            if (*s >= 'a' && *s <= 'z')
                *out++ = *s - 'a' + 'A';
            else
                *out++ = *s;
        } else if (len == 2) { // Possible accent.
            for (p = ACCENTS; *p; p += 4) {
                if (memcmp(p, s, 2) == 0) {
                    *out++ = *(p + 2);
                    n += 1;
                    break;
                }
            }
            if (!*p) {
                memcpy(out, s, 2);
                out += 2;
            }
        } else { // Other utf-8 characters.
            memcpy(out, s, len);
            out += len;
        }
        n -= len;
        s += len;
    }
    *out = '\0';
    return r;
}

static const char *canonical_replace(const char *v, int *len)
{
    int i;
    const char **r;
    for (i = 0; i < ARRAY_SIZE(CANONICAL_RULES); i++) {
        r = CANONICAL_RULES[i];
        if (*len == strlen(r[0]) && strncmp(v, r[0], *len) == 0) {
            *len = strlen(r[1]);
            return r[1];
        }
    }
    return v;
}

void identifiers_init(void)
{
    if (g_entries) return;
    regcomp(&g_token_reg, TOKEN_RE, REG_EXTENDED);
}

int identifiers_make_canonical(const char *v, char *out, int n)
{
    char buffer[n], *tok, *pout;
    const char *can_tok;
    int r, len, can_len;
    regmatch_t match;

    r = canonical_upper(v, buffer, n);
    if (r) return r;

    tok = buffer;
    pout = out;
    while (*tok) {
        r = regexec(&g_token_reg, tok, 1, &match, 0);
        if (r) break;
        if (pout != out) { // Add a space between tokens.
            *pout++ = ' ';
            n -= 1;
        }
        len = match.rm_eo - match.rm_so;
        tok += match.rm_so;
        can_len = len;
        can_tok = canonical_replace(tok, &can_len);
        if (n - 1 < can_len) {r = 1; break;}
        strncpy(pout, can_tok, can_len);
        tok += len;
        pout += can_len;
        n -= can_len;
    }
    *pout = '\0';
    return r;
}

#ifndef NDEBUG
static bool is_valid_cat(const char *s)
{
    for (; *s; s++) {
        if (!(*s >= 'A' && *s <= 'Z')) return false;
    }
    return true;
}
#endif

void identifiers_add(uint64_t oid, const char *cat, const char *value,
                     const char *search_value, const char *show_value)
{
    entry_t *entry, *group;
    int len;

    assert(oid);
    assert(cat);
    assert(is_valid_cat(cat));
    assert(value);
    search_value = search_value ?: value;
    show_value = show_value ?: value;

    HASH_FIND(hh, g_idx, &oid, sizeof(oid), group);

    if (group) {
        // Skip if already present.
        for (   entry = group;
                entry && entry->oid == oid;
                entry = entry->next) {
            if (str_equ(entry->cat, cat) && str_equ(entry->value, value)) {
                return;
            }
        }
    }

    entry = calloc(1, sizeof(*entry));
    entry->oid = oid;
    entry->cat = strdup(cat);
    entry->value = strdup(value);
    entry->show_value = strdup(show_value);
    // Reserve some space for the canonical representation.
    // "gam1 And" => "gam 1 and".
    len = strlen(search_value) + 8;
    entry->search_value = calloc(1, len);
    identifiers_make_canonical(search_value, entry->search_value, len);

    if (group) {
        DL_APPEND_ELEM(g_entries, group, entry);
    } else {
        HASH_ADD(hh, g_idx, oid, sizeof(oid), entry);
        DL_APPEND(g_entries, entry);
    }
}

bool identifiers_iter_(uint64_t oid, const char *catalog,
                       uint64_t *roid,
                       const char **rcat,
                       const char **rvalue,
                       const char **rcan,
                       const char **rshow,
                       void **tmp)
{
    entry_t *e = NULL;
    assert(!catalog || is_valid_cat(catalog));
    if (*tmp == g_entries) return false; // End of list reached.
    if (!*tmp) {
        if (oid) HASH_FIND(hh, g_idx, &oid, sizeof(oid), e);
        else e = g_entries;
        *tmp = e;
    }
    e = *tmp;
    while (true) {
        if (!e) return false;
        if (oid && oid != e->oid) return false;
        if (!catalog || str_equ(e->cat, catalog)) break;
        e = e->next;
    }
    if (roid) *roid = e->oid;
    if (rcat) *rcat = e->cat;
    if (rvalue) *rvalue = e->value;
    if (rcan) *rcan = e->search_value;
    if (rshow) *rshow = e->show_value;
    e = e->next;
    // When we reach the end of the list, we cannot set tmp to NULL,
    // since NULL is the special value used for the first iteration.
    if (!e) e = g_entries;
    *tmp = e;
    return true;
}

const char *identifiers_get(uint64_t oid, const char *catalog)
{
    const char *value;
    assert(is_valid_cat(catalog));
    IDENTIFIERS_ITER(oid, catalog, NULL, NULL, &value, NULL, NULL) {
        return value;
    }
    return NULL;
}

uint64_t identifiers_search(const char *query)
{
    char can[128];
    entry_t *e = NULL;
    identifiers_make_canonical(query, can, sizeof(can));
    DL_FOREACH(g_entries, e) {
        if (str_equ(e->search_value, can)) {
            return e->oid;
        }
    }
    return 0;
}

/******* TESTS **********************************************************/

#if COMPILE_TESTS

static void test_identifiers(void)
{
    char buff[64] = {};
    int r;
    identifiers_init();
    // Test lower to uppercase.
    identifiers_make_canonical("test", buff, 64);
    test_str(buff, "TEST");
    // Test remove accents.
    identifiers_make_canonical("vénus", buff, 64);
    test_str(buff, "VENUS");
    // Test remove trailing spaces.
    identifiers_make_canonical(" Ab  ", buff, 64);
    test_str(buff, "AB");
    // Test split words.
    identifiers_make_canonical("HIP 1000", buff, 64);
    test_str(buff, "HIP 1000");
    identifiers_make_canonical("HIP1000", buff, 64);
    test_str(buff, "HIP 1000");
    // Test greek letter.
    identifiers_make_canonical("α UMi", buff, 64);
    test_str(buff, "ALF UMI");
    identifiers_make_canonical("alpha UMi", buff, 64);
    test_str(buff, "ALF UMI");
    identifiers_make_canonical("DELTA", buff, 64);
    test_str(buff, "DEL");
    identifiers_make_canonical("DE", buff, 64);
    test_str(buff, "DE");

    // Test buffer too small.
    r = identifiers_make_canonical("TEST", buff, 3);
    assert(r);
    r = identifiers_make_canonical("β Umi", buff, 6);
    assert(r);
}

TEST_REGISTER(NULL, test_identifiers, TEST_AUTO);

#endif
