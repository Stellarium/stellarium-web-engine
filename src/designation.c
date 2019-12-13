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
    {"ξ", "ksi", "Xi", "Xi"},
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

static const char *CSTS[88] = {
    "Aql", "And", "Scl", "Ara", "Lib", "Cet", "Ari", "Sct", "Pyx", "Boo",
    "Cae", "Cha", "Cnc", "Cap", "Car", "Cas", "Cen", "Cep", "Com", "CVn",
    "Aur", "Col", "Cir", "Crt", "CrA", "CrB", "Crv", "Cru", "Cyg", "Del",
    "Dor", "Dra", "Nor", "Eri", "Sge", "For", "Gem", "Cam", "CMa", "UMa",
    "Gru", "Her", "Hor", "Hya", "Hyi", "Ind", "Lac", "Mon", "Lep", "Leo",
    "Lup", "Lyn", "Lyr", "Ant", "Mic", "Mus", "Oct", "Aps", "Oph", "Ori",
    "Pav", "Peg", "Pic", "Per", "Equ", "CMi", "LMi", "Vul", "UMi", "Phe",
    "Psc", "PsA", "Vol", "Pup", "Ret", "Sgr", "Sco", "Ser", "Sex", "Men",
    "Tau", "Tel", "Tuc", "Tri", "TrA", "Aqr", "Vir", "Vel"};

/*
 * Function: designation_parse_bayer
 * Get the bayer info for a given designation.
 *
 * Params:
 *   dsgn   - A designation (eg: '* alf Aqr')
 *   cst    - Output constellation short name.
 *   bayer  - Output bayer number (1 -> α, 2 -> β, etc.)
 *
 * Return:
 *   False if the designation doesn't match a bayer name.
 */
bool designation_parse_bayer(const char *dsgn, char cst[5], int *bayer)
{
    int i;

    *bayer = 0;
    cst[0] = '\0';

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
    if (*dsgn == ' ') dsgn++;

    *bayer = i + 1;

    // Parse constellation.
    for (i = 0; i < 88; i++) {
        if (strncasecmp(CSTS[i], dsgn, strlen(CSTS[i])) == 0)
            break;
    }
    if (i == 88) return false;
    snprintf(cst, 5, "%s", CSTS[i]);
    return true;
}

/*
 * Function: designation_parse_flamsteed
 * Get the Flamsteed info for a given designation.
 *
 * Params:
 *   dsgn       - A designation (eg: '* 49 Aqr')
 *   cst        - Output constellation short name.
 *   flamsteed  - Output Flamsteed number.
 *
 * Return:
 *   False if the designation doesn't match a flamsteed name
 */
bool designation_parse_flamsteed(const char *dsgn, char cst[5], int *flamsteed)
{
    int i;
    char *endptr;

    *flamsteed = 0;
    cst[0] = '\0';

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
        if (strncasecmp(CSTS[i], dsgn, strlen(CSTS[i])) == 0)
            break;
    }
    if (i == 88) return false;
    snprintf(cst, 5, "%s", CSTS[i]);
    return true;
}

/*
 * Function: designation_cleanup
 * Create a printable version of a designation
 *
 * This can be used for example to compute the label to render for an object.
 */
void designation_cleanup(const char *dsgn, char *out, int size, int flags)
{
    char cst[5];
    int i, n;
    const char *remove[] = {"NAME ", "Cl ", "Cl* ", "** ", "MPC "};

    if (strncmp(dsgn, "V* ", 3) == 0)
        dsgn++;

    if (designation_parse_bayer(dsgn, cst, &n)) {
        if (flags & BAYER_CONST_SHORT)
            snprintf(out, size, "%s %s", GREEK[n - 1][0], cst);
        else
            snprintf(out, size, "%s", GREEK[n - 1][0]);
        return;
    }
    if (designation_parse_flamsteed(dsgn, cst, &n)) {
        if (flags & BAYER_CONST_SHORT)
            snprintf(out, size, "%d %s", n, cst);
        else
            snprintf(out, size, "%d", n);
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
    char cst[5], buf[128];
    int n;
    bool r;

    r = designation_parse_bayer("* alf Aqr", cst, &n);
    assert(r && strcmp(cst, "Aqr") == 0 && n == 1);
    r = designation_parse_flamsteed("* 10 Aqr", cst, &n);
    assert(r && strcmp(cst, "Aqr") == 0 && n == 10);

    designation_cleanup("NAME Polaris", buf, sizeof(buf), 0);
    assert(strcmp(buf, "Polaris") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "α") == 0);
    designation_cleanup("* 104 Aqr", buf, sizeof(buf), 0);
    assert(strcmp(buf, "104") == 0);
    designation_cleanup("* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
    designation_cleanup("V* alf Aqr", buf, sizeof(buf), BAYER_CONST_SHORT);
    assert(strcmp(buf, "α Aqr") == 0);
}

TEST_REGISTER(NULL, test_designations, TEST_AUTO);

#endif
