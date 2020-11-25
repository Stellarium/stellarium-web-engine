/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "designation.h"
#include "swe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *GREEK[25][4] = {
    {"α", "alf", "Alf", "Alpha"},
    {"β", "bet", "Bet", "Beta"},
    {"γ", "gam", "Gam", "Gamma"},
    {"δ", "del", "Del", "Delta"},
    {"ε", "eps", "Eps", "Epsilon"},
    {"ζ", "zet", "Zet", "Zeta"},
    {"η", "eta", "Eta", "Eta"},
    {"θ", "tet", "Tet", "Theta"},
    {"ι", "iot", "Iot", "Iota"},
    {"κ", "kap", "Kap", "Kappa"},
    {"λ", "lam", "Lam", "Lambda"},
    {"μ", "mu" , "Mu" , "Mu"},
    {"ν", "nu" , "Nu" , "Nu"},
    {"ξ", "xi" , "Xi" , "Xi"},
    {"ξ", "ksi", "Xi" , "Xi"},
    {"ο", "omi", "Omi", "Omicron"},
    {"π", "pi" , "Pi" , "Pi"},
    {"ρ", "rho", "Rho", "Rho"},
    {"σ", "sig", "Sig", "Sigma"},
    {"τ", "tau", "Tau", "Tau"},
    {"υ", "ups", "Ups", "Upsilon"},
    {"φ", "phi", "Phi", "Phi"},
    {"χ", "chi", "Chi", "Chi"},
    {"ψ", "psi", "Psi", "Psi"},
    {"ω", "ome", "Ome", "Omega"}
};

static const char *CSTS[88][2] = {
    {"And", "Andromedae"},
    {"Ant", "Antliae"},
    {"Aps", "Apodis"},
    {"Aqr", "Aquarii"},
    {"Aql", "Aquilae"},
    {"Ara", "Arae"},
    {"Ari", "Arietis"},
    {"Aur", "Aurigae"},
    {"Boo", "Boötis"},
    {"Cae", "Caeli"},
    {"Cam", "Camelopardalis"},
    {"Cnc", "Cancri"},
    {"CVn", "Canum Venaticorum"},
    {"CMa", "Canis Majoris"},
    {"CMi", "Canis Minoris"},
    {"Cap", "Capricorni"},
    {"Car", "Carinae"},
    {"Cas", "Cassiopeiae"},
    {"Cen", "Centauri"},
    {"Cep", "Cephei"},
    {"Cet", "Ceti"},
    {"Cha", "Chamaeleontis"},
    {"Cir", "Circini"},
    {"Col", "Columbae"},
    {"Com", "Comae Berenices"},
    {"CrA", "Coronae Australis"},
    {"CrB", "Coronae Borealis"},
    {"Crv", "Corvi"},
    {"Crt", "Crateris"},
    {"Cru", "Crucis"},
    {"Cyg", "Cygni"},
    {"Del", "Delphini"},
    {"Dor", "Doradus"},
    {"Dra", "Draconis"},
    {"Equ", "Equulei"},
    {"Eri", "Eridani"},
    {"For", "Fornacis"},
    {"Gem", "Geminorum"},
    {"Gru", "Gruis"},
    {"Her", "Herculis"},
    {"Hor", "Horologii"},
    {"Hya", "Hydrae"},
    {"Hyi", "Hydri"},
    {"Ind", "Indi"},
    {"Lac", "Lacertae"},
    {"Leo", "Leonis"},
    {"LMi", "Leonis Minoris"},
    {"Lep", "Leporis"},
    {"Lib", "Librae"},
    {"Lup", "Lupi"},
    {"Lyn", "Lyncis"},
    {"Lyr", "Lyrae"},
    {"Men", "Mensae"},
    {"Mic", "Microscopii"},
    {"Mon", "Monocerotis"},
    {"Mus", "Muscae"},
    {"Nor", "Normae"},
    {"Oct", "Octantis"},
    {"Oph", "Ophiuchi"},
    {"Ori", "Orionis"},
    {"Pav", "Pavonis"},
    {"Peg", "Pegasi"},
    {"Per", "Persei"},
    {"Phe", "Phoenicis"},
    {"Pic", "Pictoris"},
    {"Psc", "Piscium"},
    {"PsA", "Piscis Austrini"},
    {"Pup", "Puppis"},
    {"Pyx", "Pyxidis"},
    {"Ret", "Reticuli"},
    {"Sge", "Sagittae"},
    {"Sgr", "Sagittarii"},
    {"Sco", "Scorpii"},
    {"Scl", "Sculptoris"},
    {"Sct", "Scuti"},
    {"Ser", "Serpentis"},
    {"Sex", "Sextantis"},
    {"Tau", "Tauri"},
    {"Tel", "Telescopii"},
    {"Tri", "Trianguli"},
    {"TrA", "Trianguli Australis"},
    {"Tuc", "Tucanae"},
    {"UMa", "Ursae Majoris"},
    {"UMi", "Ursae Minoris"},
    {"Vel", "Velorum"},
    {"Vir", "Virginis"},
    {"Vol", "Volantis"},
    {"Vul", "Vulpeculae"}
};

/*
 * Function: designation_parse_bayer
 * Get the bayer info for a given designation.
 *
 * Params:
 *   dsgn   - A designation (eg: '* alf Aqr')
 *   cst    - Output constellation id
 *   bayer  - Output bayer number (1 -> α, 2 -> β, etc.)  Can also be set
 *            to a single ASCII value of a letter.
 *   nb     - Output extra number to use as exponent. 0 means no exponent.
 *   suffix - Output extra suffix to append after the bayer name.
 *
 * Return:
 *   False if the designation doesn't match a bayer name.
 */
static bool designation_parse_bayer(const char *dsgn, int *cst, int *bayer,
                                    int *nb, const char **suffix)
{
    int i;
    char *endptr;

    *bayer = 0;
    *nb = 0;
    *suffix = NULL;

    if (!dsgn) return false;
    // Parse the '* '
    if (strlen(dsgn) < 4) return false;
    if (strncmp(dsgn, "* ", 2) == 0) {
        dsgn += 2;
    } else if (strncmp(dsgn, "V* ", 3) == 0) {
        dsgn += 3;
    } else {
        return false;
    }

    // Parse greek letter.
    for (i = 0; i < 25; i++) {
        if (strncasecmp(GREEK[i][1], dsgn, strlen(GREEK[i][1])) == 0)
            break;
    }
    if (i == 25) {
        // No greek letter were found, try a letter
        if (*dsgn == 'V') return false; // Variable star.
        if ((*dsgn >= 'a' && *dsgn <= 'z') || (*dsgn >= 'A' && *dsgn <= 'Z')) {
            *bayer = *dsgn;
            dsgn++;
        } else {
            return false;
        }
    } else {
        *bayer = i + 1;
        dsgn += strlen(GREEK[i][1]);
    }

    if (*dsgn == '.') dsgn++;

    *nb = strtol(dsgn, &endptr, 10);
    if (*nb != 0)
        dsgn = endptr;

    if (*dsgn == ' ') dsgn++;

    // Parse constellation.
    for (i = 0; i < 88; i++) {
        if (strncasecmp(CSTS[i][0], dsgn, strlen(CSTS[i][0])) == 0)
            break;
    }
    if (i == 88) return false;
    *cst = i;

    dsgn += strlen(CSTS[i][0]);
    *suffix = dsgn;
    return true;
}

/*
 * Function: designation_parse_flamsteed
 * Get the Flamsteed info for a given designation.
 *
 * Params:
 *   dsgn       - A designation (eg: '* 49 Aqr')
 *   cst        - Output constellation id
 *   flamsteed  - Output Flamsteed number.
 *   suffix     - Output extra suffix to append after the bayer name.
 *
 * Return:
 *   False if the designation doesn't match a flamsteed name
 */
static bool designation_parse_flamsteed(const char *dsgn, int *cst,
                                        int *flamsteed, const char **suffix)
{
    int i;
    char *endptr;

    *flamsteed = 0;
    *suffix = NULL;

    if (!dsgn) return false;
    // Parse the '* '
    if (strlen(dsgn) < 4) return false;
    if (strncmp(dsgn, "* ", 2) == 0) {
        dsgn += 2;
    } else if (strncmp(dsgn, "V* ", 3) == 0) {
        dsgn += 3;
    } else {
        return false;
    }

    *flamsteed = strtol(dsgn, &endptr, 10);
    if (*flamsteed == 0) return false;
    dsgn = endptr;
    if (*dsgn == ' ') dsgn++;

    // Parse constellation.
    for (i = 0; i < 88; i++) {
        if (strncasecmp(CSTS[i][0], dsgn, strlen(CSTS[i][0])) == 0)
            break;
    }
    if (i == 88) return false;
    *cst = i;

    dsgn += strlen(CSTS[i][0]);
    *suffix = dsgn;
    return true;
}

/*
 * Function: designation_parse_variable_star
 * Parse 'V*' designations, like 'V* VX Sgr'.
 *
 * https://en.wikipedia.org/wiki/Variable_star_designation
 * We match any string of the form 'V* <ANY> <CST>'
 *
 * Parameters:
 *   dsgn   - A designation (eg: 'V* VX Sgr')
 *   cst    - Output constellation id
 *   var    - Variable star id (eg: 'VX')
 *   suffix - Point to the end of the input dsgn.
 *
 * Return:
 *   False if the designation doesn't match a variable star.
 */
static bool designation_parse_variable_star(
        const char *dsgn, int *cst, char var[8], const char **suffix)
{
    int i;

    if (!dsgn) return false;
    if (strncmp(dsgn, "V* ", 3) != 0) return false;
    dsgn += 3;

    // Parse letters.
    for (i = 0; i < 7; i++) {
        if (*dsgn == ' ') break;
        if (!((*dsgn >= 'A' && *dsgn <= 'Z') ||
              (*dsgn >= '0' && *dsgn <= '9'))) {
            return false;
        }
        var[i] = *dsgn++;
    }
    if (i == 7) return false;
    var[i] = '\0';
    dsgn++;

    // Parse constellation.
    for (i = 0; i < 88; i++) {
        if (strncasecmp(CSTS[i][0], dsgn, strlen(CSTS[i][0])) == 0)
            break;
    }
    if (i == 88) return false;
    *cst = i;

    dsgn += strlen(CSTS[i][0]);
    *suffix = dsgn;
    return true;
}

static const char * to_exponent(char c) {
    switch (c) {
    case 48:
        return "⁰";
    case 49:
        return "¹";
    case 50:
        return "²";
    case 51:
        return "³";
    case 52:
        return "⁴";
    case 53:
        return "⁵";
    case 54:
        return "⁶";
    case 55:
        return "⁷";
    case 56:
        return "⁸";
    case 57:
        return "⁹";
    };
    return "";
}

/*
 * Function: designation_cleanup
 * Create a printable version of a designation
 *
 * This can be used for example to compute the label to render for an object.
 */
EMSCRIPTEN_KEEPALIVE
void designation_cleanup(const char *dsgn, char *out, int size, int flags)
{
    int cst;
    int i, g, nb;
    const char *remove[] = {"NAME ", "* ", "Cl ", "Cl* ", "** ", "MPC "};
    const char *greek;
    const char *cstname;
    const char *suffix;
    char tmp[64], tmp_letter[32];
    char exponent[256], var[8];

    if (designation_parse_bayer(dsgn, &cst, &g, &nb, &suffix)) {
        exponent[0] = 0;
        tmp[0] = 0;
        if (g >= 'A' && g <= 'z') {
            snprintf(tmp_letter, sizeof(tmp_letter), "%c", g);
            greek = tmp_letter;
        } else {
            greek = (flags & BAYER_LATIN_SHORT) ? GREEK[g - 1][2] :
                    (flags & BAYER_LATIN_LONG) ? GREEK[g - 1][3] :
                    GREEK[g - 1][0];
        }
        if (nb) {
            snprintf(tmp, sizeof(tmp), "%d", nb);
            for (i = 0; i < strlen(tmp); ++i)
                strncat(exponent, to_exponent(tmp[i]),
                        sizeof(exponent) - strlen(exponent) - 1);
        }
        if (flags & BAYER_CONST_SHORT || flags & BAYER_CONST_LONG) {
            cstname = (flags & BAYER_CONST_SHORT) ? CSTS[cst][0] : CSTS[cst][1];
            snprintf(out, size, "%s%s %s%s", greek, exponent, cstname, suffix);
        } else
            snprintf(out, size, "%s%s%s", greek, exponent, suffix);
        return;
    }
    if (designation_parse_flamsteed(dsgn, &cst, &g, &suffix)) {
        if (flags & BAYER_CONST_SHORT || flags & BAYER_CONST_LONG) {
            cstname = (flags & BAYER_CONST_SHORT) ? CSTS[cst][0] : CSTS[cst][1];
            snprintf(out, size, "%d %s%s", g, cstname, suffix);
        } else
            snprintf(out, size, "%d%s", g, suffix);
        return;
    }
    if (designation_parse_variable_star(dsgn, &cst, var, &suffix)) {
        if (flags & (BAYER_CONST_SHORT | BAYER_CONST_LONG)) {
            cstname = (flags & BAYER_CONST_LONG) ? CSTS[cst][1] : CSTS[cst][0];
            snprintf(out, size, "%s %s%s", var, cstname, suffix);
        } else {
            snprintf(out, size, "%s%s", var, suffix);
        }
        return;
    }

    // At this point we shouldn't have any "*" or "V*" designations.
    if (strncmp(dsgn, "V* ", 3) == 0 || strncmp(dsgn, "* ", 2) == 0) {
        LOG_W_ONCE("Unmatched star designation: '%s'", dsgn);
    }

    // NAME designation with translation.
    if ((flags & DSGN_TRANSLATE) && strncmp(dsgn, "NAME ", 5) == 0) {
        snprintf(out, size, "%s", sys_translate("sky", dsgn + 5));
        return;
    }

    for (i = 0; i < ARRAY_SIZE(remove); i++) {
        if (strncmp(dsgn, remove[i], strlen(remove[i])) == 0) {
            snprintf(out, size, "%s", dsgn + strlen(remove[i]));
            return;
        }
    }

    snprintf(out, size, "%s", dsgn);
}

/*
 * Function: designations_get_tyc
 * Extract a TYC number from a designations list.
 *
 * Parameters:
 *   dsgns  - Null terminated list of null terminated strings.
 *
 * Return:
 *   True if a TYC was found.
 */
bool designations_get_tyc(const char *dsgns, int *tyc1, int *tyc2, int *tyc3)
{
    const char *dsgn;
    if (!dsgns) return false;
    for (dsgn = dsgns; *dsgn; dsgn = dsgn + strlen(dsgn) + 1) {
        if (strncmp(dsgn, "TYC ", 4) != 0) continue;
        if (sscanf(dsgn, "TYC %d-%d-%d", tyc1, tyc2, tyc3) == 3)
            return true;
    }
    return false;
}

#if COMPILE_TESTS

static void test_designations(void)
{
    char buf[128];
    const char *suffix;
    int n, cst, nb;
    int tyc1, tyc2, tyc3;
    bool r;

    r = designation_parse_bayer("* alf Aqr", &cst, &n, &nb, &suffix);
    assert(r && strcmp(CSTS[cst][0], "Aqr") == 0 && n == 1);
    r = designation_parse_flamsteed("* 10 Aqr", &cst, &n, &suffix);
    assert(r && strcmp(CSTS[cst][0], "Aqr") == 0 && n == 10);

    r = designation_parse_bayer("V* V2101 Cyg", &cst, &n, &nb, &suffix);
    assert(!r);

    r = designation_parse_variable_star("V* V2101 Cyg", &cst, buf, &suffix);
    assert(r);

    r = designation_parse_variable_star("V* VZ Sgr", &cst, buf, &suffix);
    assert(r);
    assert(strcmp(CSTS[cst][0], "Sgr") == 0);
    assert(strcmp(buf, "VZ") == 0);
    assert(strcmp(suffix, "") == 0);

    r = designation_parse_variable_star("V* YZ Cet X", &cst, buf, &suffix);
    assert(r);
    assert(strcmp(CSTS[cst][0], "Cet") == 0);
    assert(strcmp(buf, "YZ") == 0);
    assert(strcmp(suffix, " X") == 0);

    designation_cleanup("NAME Polaris", buf, sizeof(buf), 0);
    assert(strcmp(buf, "Polaris") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "α") == 0);
    designation_cleanup("* alf1 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "α¹") == 0);
    designation_cleanup("* alf0123456789 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "α¹²³⁴⁵⁶⁷⁸⁹") == 0);
    designation_cleanup("* alf04 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "α⁴") == 0);
    designation_cleanup("* s07 Aqr B", buf, sizeof(buf), 0);
    assert(strcmp(buf, "s⁷ B") == 0);
    designation_cleanup("* zet Aqr B", buf, sizeof(buf), BAYER_LATIN_LONG |
                        BAYER_CONST_LONG);
    assert(strcmp(buf, "Zeta Aquarii B") == 0);
    designation_cleanup("* b04 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "b⁴") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_SHORT);
    assert(strcmp(buf, "Alf") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_LONG);
    assert(strcmp(buf, "Alpha") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_LONG |
                        BAYER_CONST_LONG);
    assert(strcmp(buf, "Alpha Aquarii") == 0);
    designation_cleanup("* alf Aqr B", buf, sizeof(buf), BAYER_LATIN_LONG |
                        BAYER_CONST_LONG);
    assert(strcmp(buf, "Alpha Aquarii B") == 0);
    designation_cleanup("* 104 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "104") == 0);
    designation_cleanup("* 104 Aqr B", buf, sizeof(buf), 0);
    assert(strcmp(buf, "104 B") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
    designation_cleanup("V* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
    designation_cleanup("* A Pup", buf, sizeof(buf), BAYER_CONST_LONG);
    assert(strcmp(buf, "A Puppis") == 0);
    designation_cleanup("* K Vel", buf, sizeof(buf), BAYER_CONST_LONG);
    assert(strcmp(buf, "K Velorum") == 0);

    r = designations_get_tyc("TYC 8841-489-2\0", &tyc1, &tyc2, &tyc3);
    assert(r && tyc1 == 8841 && tyc2 == 489 && tyc3 == 2);
}

TEST_REGISTER(NULL, test_designations, TEST_AUTO);

#endif
