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
 *   bayer  - Output bayer number (1 -> α, 2 -> β, etc.)
 *   nb     - Output extra number to use as exponent. 0 means no exponent.
 *
 * Return:
 *   False if the designation doesn't match a bayer name.
 */
static bool designation_parse_bayer(const char *dsgn, int *cst, int *bayer,
                                    int *nb)
{
    int i;
    char *endptr;

    *bayer = 0;
    *nb = 0;

    if (!dsgn) return false;
    // Parse the '* '
    if (strlen(dsgn) < 4) return false;
    if (strncmp(dsgn, "* ", 2) != 0) return false;
    dsgn += 2;

    // Parse greek letter.
    for (i = 0; i < 25; i++) {
        if (strncasecmp(GREEK[i][1], dsgn, strlen(GREEK[i][1])) == 0)
            break;
    }
    if (i == 25) return false;
    dsgn += strlen(GREEK[i][1]);
    if (*dsgn == '.') dsgn++;

    *nb = strtol(dsgn, &endptr, 10);
    if (*nb != 0)
        dsgn = endptr;

    if (*dsgn == ' ') dsgn++;

    *bayer = i + 1;

    // Parse constellation.
    for (i = 0; i < 88; i++) {
        if (strncasecmp(CSTS[i][0], dsgn, strlen(CSTS[i][0])) == 0)
            break;
    }
    if (i == 88) return false;
    *cst = i;
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
 *
 * Return:
 *   False if the designation doesn't match a flamsteed name
 */
static bool designation_parse_flamsteed(const char *dsgn, int *cst,
                                        int *flamsteed)
{
    int i;
    char *endptr;

    *flamsteed = 0;

    if (!dsgn) return false;
    // Parse the '* '
    if (strlen(dsgn) < 4) return false;
    if (strncmp(dsgn, "* ", 2) != 0) return false;
    dsgn += 2;

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
void designation_cleanup(const char *dsgn, char *out, int size, int flags)
{
    int cst;
    int i, g, nb;
    const char *remove[] = {"NAME ", "* ", "Cl ", "Cl* ", "** ", "MPC "};
    const char *greek;
    const char *cstname;
    char tmp[64];
    char exponent[256];

    if (strncmp(dsgn, "V* ", 3) == 0)
        dsgn++;

    if (designation_parse_bayer(dsgn, &cst, &g, &nb)) {
        exponent[0] = 0;
        tmp[0] = 0;
        greek = (flags & BAYER_LATIN_SHORT) ? GREEK[g - 1][2] :
                (flags & BAYER_LATIN_LONG) ? GREEK[g - 1][3] : GREEK[g - 1][0];
        if (nb) {
            snprintf(tmp, sizeof(tmp), "%d", nb);
            for (i = 0; i < strlen(tmp); ++i)
                strncat(exponent, to_exponent(tmp[i]),
                        sizeof(exponent) - strlen(exponent) - 1);
        }
        if (flags & BAYER_CONST_SHORT || flags & BAYER_CONST_LONG) {
            cstname = (flags & BAYER_CONST_SHORT) ? CSTS[cst][0] : CSTS[cst][1];
            snprintf(out, size, "%s%s %s", greek, exponent, cstname);
        } else
            snprintf(out, size, "%s%s", greek, exponent);
        return;
    }
    if (designation_parse_flamsteed(dsgn, &cst, &g)) {
        if (flags & BAYER_CONST_SHORT || flags & BAYER_CONST_LONG) {
            cstname = (flags & BAYER_CONST_SHORT) ? CSTS[cst][0] : CSTS[cst][1];
            snprintf(out, size, "%d %s", g, cstname);
        } else
            snprintf(out, size, "%d", g);
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

#if COMPILE_TESTS

static void test_designations(void)
{
    char buf[128];
    int n, cst, nb;
    bool r;

    r = designation_parse_bayer("* alf Aqr", &cst, &n, &nb);
    assert(r && strcmp(CSTS[cst][0], "Aqr") == 0 && n == 1);
    r = designation_parse_flamsteed("* 10 Aqr", &cst, &n);
    assert(r && strcmp(CSTS[cst][0], "Aqr") == 0 && n == 10);

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
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_SHORT);
    assert(strcmp(buf, "Alf") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_LONG);
    assert(strcmp(buf, "Alpha") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_LATIN_LONG |
                        BAYER_CONST_LONG);
    assert(strcmp(buf, "Alpha Aquarii") == 0);
    designation_cleanup("* 104 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "104") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
    designation_cleanup("V* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
}

TEST_REGISTER(NULL, test_designations, TEST_AUTO);

#endif
