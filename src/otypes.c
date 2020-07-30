/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// Simbad object type database.
// Generated from:
// http://simbad.u-strasbg.fr/simbad/sim-display?data=otypes

#include "otypes.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "otypes.inl"

// N_ macro for gettext parsing.
#ifndef N_
#define N_(s) s
#endif

typedef struct entry
{
    uint8_t n[4];   // 4 digits number.
    char    id[5];  // 4 bytes id (use 5 to ensure null termination).
    char    *str;   // Long name.
} entry_t;

static const entry_t ENTRIES[];

static const entry_t *otype_get(const char *id)
{
    int idx;
    idx = otypes_hash_search(id, strnlen(id, 4));
    return idx != -1 ? &ENTRIES[idx] : NULL;
}

// Return informations about a type.
const char *otype_get_str(const char *id)
{
    const entry_t *e;
    e = otype_get(id);
    return e ? e->str : NULL;
}

// Return the parent condensed id of an otype.
const char *otype_get_parent(const char *id)
{
    int i;
    uint8_t n[4];
    const entry_t *e;

    e = otype_get(id);
    if (!e) return NULL;
    // We get the parent number by setting the last non zero digit to zero.
    memcpy(n, e->n, 4);
    for (i = 0; i < 4; i++) {
        if (n[i] == 0) break;
    }
    if (i == 0) return NULL;
    n[i - 1] = 0;
    for (e = &ENTRIES[0]; e->id[0]; e++) {
        if (memcmp(e->n, n, 4) == 0)
            return e->id;
    }
    return NULL;
}

static bool otype_get_digits(const char *id, uint8_t out[4])
{
    const entry_t *e;
    e = otype_get(id);
    if (!e) return false;
    memcpy(out, e->n, 4);
    return true;
}

/*
 * Function: otype_match
 * Test if an otype matches an other otype
 *
 * Return true if the type is equal to or is a subclass of the other.
 */
bool otype_match(const char *otype, const char *match)
{
    uint8_t o[4], m[4];
    int i;
    if (strncmp(otype, match, 4) == 0) return true;
    if (!otype_get_digits(otype, o)) return false;
    if (!otype_get_digits(match, m)) return false;
    for (i = 0; i < 4; i++) {
        if (m[i] == 0) return true;
        if (o[i] != m[i]) return false;
    }
    return true;
}

// STYLE-CHECK OFF

static const entry_t ENTRIES[] = {
#define T(n0, n1, n2, n3, id, str) {{n0, n1, n2, n3}, id, str},
T( 0, 0, 0, 0, "?"  , N_("Object of unknown nature"))
T( 0, 2, 0, 0, "ev" ,   N_("transient event"))
T( 1, 0, 0, 0, "Rad", N_("Radio-source"))
T( 1, 2, 0, 0, "mR" ,   N_("metric Radio-source"))
T( 1, 4, 0, 0, "cm" ,   N_("centimetric Radio-source"))
T( 1, 6, 0, 0, "mm" ,   N_("millimetric Radio-source"))
T( 1, 8, 0, 0, "smm",   N_("sub-millimetric source"))
T( 1,11, 0, 0, "HI" ,   N_("HI source"))
T( 1,12, 0, 0, "rB" ,   N_("radio Burst"))
T( 1,14, 0, 0, "Mas",   N_("Maser"))
T( 2, 0, 0, 0, "IR" , N_("Infra-Red source"))
T( 2, 2, 0, 0, "FIR",   N_("Far-IR source"))
T( 2, 4, 0, 0, "NIR",   N_("Near-IR source"))
T( 3, 0, 0, 0, "red", N_("Very red source"))
T( 3, 3, 0, 0, "ERO",   N_("Extremely Red Object"))
T( 4, 0, 0, 0, "blu", N_("Blue object"))
T( 5, 0, 0, 0, "UV" , N_("UV-emission source"))
T( 6, 0, 0, 0, "X"  , N_("X-ray source"))
T( 6, 2, 0, 0, "UX?",   N_("Possible Ultra-luminous X-ray"))
T( 6,10, 0, 0, "ULX",   N_("Ultra-luminous X-ray source"))
T( 7, 0, 0, 0, "gam", N_("gamma-ray source"))
T( 7, 3, 0, 0, "gB" ,   N_("gamma-ray Burst"))
T( 8, 0, 0, 0, "err", N_("Not an object"))
T( 9, 0, 0, 0, "grv", N_("Gravitational Source"))
T( 9, 3, 0, 0, "Lev",   N_("(Micro)Lensing Event"))
T( 9, 6, 0, 0, "LS?",   N_("Possible gravitational lens System"))
T( 9, 7, 0, 0, "Le?",   N_("Possible gravitational lens"))
T( 9, 8, 0, 0, "LI?",   N_("Possible gravitationally lensed image"))
T( 9, 9, 0, 0, "gLe",   N_("Gravitational Lens"))
T( 9,11, 0, 0, "gLS",   N_("Gravitational Lens System"))
T( 9,12, 0, 0, "GWE",   N_("Gravitational Wave Event"))
T(10, 0, 0, 0, "..?", N_("Candidate objects"))
T(10, 1, 0, 0, "G?" ,   N_("Possible Galaxy"))
T(10, 2, 0, 0, "SC?",   N_("Possible Supercluster of Galaxies"))
T(10, 3, 0, 0, "C?G",   N_("Possible Cluster of Galaxies"))
T(10, 4, 0, 0, "Gr?",   N_("Possible Group of Galaxies"))
T(10, 6, 0, 0, "As?",   N_("Possible Association of Stars"))
T(10,11, 0, 0, "**?",   N_("Possible Physical Binary"))
T(10,11, 1, 0, "EB?",     N_("Possible Eclipsing Binary"))
T(10,11,10, 0, "Sy?",     N_("Possible Symbiotic Star"))
T(10,11,11, 0, "CV?",     N_("Possible Cataclysmic Binary"))
T(10,11,11, 6, "No?",       N_("Possible Nova"))
T(10,11,12, 0, "XB?",     N_("Possible X-ray binary"))
T(10,11,12, 2, "LX?",       N_("Possible Low-Mass X-ray binary"))
T(10,11,12, 3, "HX?",       N_("Possible High-Mass X-ray binary"))
T(10,12, 0, 0, "Pec?",   N_("Possible Peculiar Star"))
T(10,12, 1, 0, "Y*?",     N_("Possible Young Stellar Object"))
T(10,12, 2, 0, "pr?",     N_("Possible Pre-main sequence Star"))
T(10,12, 2, 3, "TT?",       N_("Possible T Tau star"))
T(10,12, 3, 0, "C*?",     N_("Possible Carbon Star"))
T(10,12, 4, 0, "S*?",     N_("Possible S Star"))
T(10,12, 5, 0, "OH?",     N_("Possible Star with envelope of OH/IR type"))
T(10,12, 6, 0, "CH?",     N_("Possible Star with envelope of CH type"))
T(10,12, 7, 0, "WR?",     N_("Possible Wolf-Rayet Star"))
T(10,12, 8, 0, "Be?",     N_("Possible Be Star"))
T(10,12, 9, 0, "Ae?",     N_("Possible Herbig Ae/Be Star"))
T(10,12,11, 0, "HB?",     N_("Possible Horizontal Branch Star"))
T(10,12,11, 2, "RR?",       N_("Possible Star of RR Lyr type"))
T(10,12,11, 3, "Ce?",       N_("Possible Cepheid"))
T(10,12,12, 0, "RB?",     N_("Possible Red Giant Branch star"))
T(10,12,13, 0, "sg?",     N_("Possible Supergiant star"))
T(10,12,13, 3, "s?r",       N_("Possible Red supergiant star"))
T(10,12,13, 4, "s?y",       N_("Possible Yellow supergiant star"))
T(10,12,13, 5, "s?b",       N_("Possible Blue supergiant star"))
T(10,12,14, 0, "AB?",     N_("Possible Asymptotic Giant Branch Star"))
T(10,12,14, 1, "LP?",       N_("Possible Long Period Variable"))
T(10,12,14, 2, "Mi?",       N_("Possible Mira"))
T(10,12,14, 3, "sv?",       N_("Possible Semi-regular variable"))
T(10,12,15, 0, "pA?",     N_("Possible Post-AGB Star"))
T(10,12,16, 0, "BS?",     N_("Possible Blue Straggler Star"))
T(10,12,17, 0, "HS?",     N_("Possible Hot subdwarf"))
T(10,12,18, 0, "WD?",     N_("Possible White Dwarf"))
T(10,12,20, 0, "N*?",     N_("Possible Neutron Star"))
T(10,12,22, 0, "BH?",     N_("Possible Black Hole"))
T(10,12,23, 0, "SN?",     N_("Possible SuperNova"))
T(10,12,24, 0, "LM?",     N_("Possible Low-mass star"))
T(10,12,26, 0, "BD?",     N_("Possible Brown Dwarf"))
T(12, 0, 0, 0, "mul", N_("Composite object"))
T(12, 1, 0, 0, "reg",   N_("Region defined in the sky"))
T(12, 1, 5, 0, "vid",     N_("Underdense region of the Universe"))
T(12, 2, 0, 0, "SCG",   N_("Supercluster of Galaxies"))
T(12, 3, 0, 0, "ClG",   N_("Cluster of Galaxies"))
T(12, 4, 0, 0, "GrG",   N_("Group of Galaxies"))
T(12, 4, 5, 0, "CGG",     N_("Compact Group of Galaxies"))
T(12, 5, 0, 0, "PaG",   N_("Pair of Galaxies"))
T(12, 5, 5, 0, "IG" ,     N_("Interacting Galaxies"))
T(12, 9, 0, 0, "C?*",   N_("Possible Star Cluster"))
T(12,10, 0, 0, "Gl?",   N_("Possible Globular Cluster"))
T(12,11, 0, 0, "Cl*",   N_("Cluster of Stars"))
T(12,11, 1, 0, "GlC",     N_("Globular Cluster"))
T(12,11, 2, 0, "OpC",     N_("Open Cluster"))
T(12,12, 0, 0, "As*",   N_("Association of Stars"))
T(12,12, 1, 0, "St*",     N_("Stellar Stream"))
T(12,12, 2, 0, "MGr",     N_("Moving Group"))
T(12,13, 0, 0, "**" ,   N_("Double or multiple star"))
T(12,13, 1, 0, "EB*",     N_("Eclipsing binary"))
T(12,13, 1, 1, "Al*",       N_("Eclipsing binary of Algol type"))
T(12,13, 1, 2, "bL*",       N_("Eclipsing binary of beta Lyr type"))
T(12,13, 1, 3, "WU*",       N_("Eclipsing binary of W UMa type"))
T(12,13, 1, 8, "EP*",       N_("Star showing eclipses by its planet"))
T(12,13, 2, 0, "SB*",     N_("Spectroscopic binary"))
T(12,13, 5, 0, "El*",     N_("Ellipsoidal variable Star"))
T(12,13,10, 0, "Sy*",     N_("Symbiotic Star"))
T(12,13,11, 0, "CV*",     N_("Cataclysmic Variable Star"))
T(12,13,11, 2, "DQ*",       N_("CV DQ Her type"))
T(12,13,11, 3, "AM*",       N_("CV of AM Her type"))
T(12,13,11, 5, "NL*",       N_("Nova-like Star"))
T(12,13,11, 6, "No*",       N_("Nova"))
T(12,13,11, 7, "DN*",       N_("Dwarf Nova"))
T(12,13,12, 0, "XB*",     N_("X-ray Binary"))
T(12,13,12, 2, "LXB",       N_("Low Mass X-ray Binary"))
T(12,13,12, 3, "HXB",       N_("High Mass X-ray Binary"))
T(13, 0, 0, 0, "ISM", N_("Interstellar matter"))
T(13, 1, 0, 0, "PoC",   N_("Part of Cloud"))
T(13, 2, 0, 0, "PN?",   N_("Possible Planetary Nebula"))
T(13, 3, 0, 0, "CGb",   N_("Cometary Globule"))
T(13, 4, 0, 0, "bub",   N_("Bubble"))
T(13, 6, 0, 0, "EmO",   N_("Emission Object"))
T(13, 8, 0, 0, "Cld",   N_("Interstellar Cloud"))
T(13, 8, 3, 0, "GNe",     N_("Galactic Nebula"))
T(13, 8, 4, 0, "BNe",     N_("Bright Nebula"))
T(13, 8, 6, 0, "DNe",     N_("Dark Cloud"))
T(13, 8, 7, 0, "RNe",     N_("Reflection Nebula"))
T(13, 8,12, 0, "MoC",     N_("Molecular Cloud"))
T(13, 8,12, 3, "glb",       N_("Globule"))
T(13, 8,12, 6, "cor",       N_("Dense core"))
T(13, 8,12, 8, "SFR",       N_("Star forming region"))
T(13, 8,13, 0, "HVC",     N_("High-velocity Cloud"))
T(13, 9, 0, 0, "HII",   N_("HII region"))
T(13,10, 0, 0, "PN" ,   N_("Planetary Nebula"))
T(13,11, 0, 0, "sh" ,   N_("HI shell"))
T(13,12, 0, 0, "SR?",   N_("Possible SuperNova Remnant"))
T(13,13, 0, 0, "SNR",   N_("SuperNova Remnant"))
T(13,14, 0, 0, "cir",   N_("CircumStellar matter"))
T(13,14, 1, 0, "of?",     N_("Possible Outflow"))
T(13,14,15, 0, "out",     N_("Outflow"))
T(13,14,16, 0, "HH" ,     N_("Herbig-Haro Object"))
T(14, 0, 0, 0, "*"  , N_("Star"))
T(14, 1, 0, 0, "*iC",   N_("Star in Cluster"))
T(14, 2, 0, 0, "*iN",   N_("Star in Nebula"))
T(14, 3, 0, 0, "*iA",   N_("Star in Association"))
T(14, 4, 0, 0, "*i*",   N_("Star in double system"))
T(14, 5, 0, 0, "V*?",   N_("Star suspected of Variability"))
T(14, 6, 0, 0, "Pe*",   N_("Peculiar Star"))
T(14, 6, 1, 0, "HB*",     N_("Horizontal Branch Star"))
T(14, 6, 2, 0, "Y*O",     N_("Young Stellar Object"))
T(14, 6, 2, 4, "Ae*",       N_("Herbig Ae/Be star"))
T(14, 6, 5, 0, "Em*",     N_("Emission-line Star"))
T(14, 6, 5, 3, "Be*",       N_("Be Star"))
T(14, 6, 6, 0, "BS*",     N_("Blue Straggler Star"))
T(14, 6,10, 0, "RG*",     N_("Red Giant Branch star"))
T(14, 6,12, 0, "AB*",     N_("Asymptotic Giant Branch Star"))
T(14, 6,12, 3, "C*" ,       N_("Carbon Star"))
T(14, 6,12, 6, "S*" ,       N_("S Star"))
T(14, 6,13, 0, "sg*",     N_("Evolved supergiant star"))
T(14, 6,13, 3, "s*r",       N_("Red supergiant star"))
T(14, 6,13, 4, "s*y",       N_("Yellow supergiant star"))
T(14, 6,13, 5, "s*b",       N_("Blue supergiant star"))
T(14, 6,14, 0, "HS*",     N_("Hot subdwarf"))
T(14, 6,15, 0, "pA*",     N_("Post-AGB Star"))
T(14, 6,16, 0, "WD*",     N_("White Dwarf"))
T(14, 6,16, 1, "ZZ*",       N_("Pulsating White Dwarf"))
T(14, 6,17, 0, "LM*",     N_("Low-mass star"))
T(14, 6,18, 0, "BD*",     N_("Brown Dwarf"))
T(14, 6,19, 0, "N*" ,     N_("Confirmed Neutron Star"))
T(14, 6,23, 0, "OH*",     N_("OH/IR star"))
T(14, 6,24, 0, "CH*",     N_("Star with envelope of CH type"))
T(14, 6,25, 0, "pr*",     N_("Pre-main sequence Star"))
T(14, 6,25, 3, "TT*",       N_("T Tau-type Star"))
T(14, 6,30, 0, "WR*",     N_("Wolf-Rayet Star"))
T(14, 7, 0, 0, "PM*",   N_("High proper-motion Star"))
T(14, 8, 0, 0, "HV*",   N_("High-velocity Star"))
T(14, 9, 0, 0, "V*" ,   N_("Variable Star"))
T(14, 9, 1, 0, "Ir*",     N_("Variable Star of irregular type"))
T(14, 9, 1, 1, "Or*",       N_("Variable Star of Orion Type"))
T(14, 9, 1, 2, "RI*",       N_("Variable Star with rapid variations"))
T(14, 9, 3, 0, "Er*",     N_("Eruptive variable Star"))
T(14, 9, 3, 1, "Fl*",       N_("Flare Star"))
T(14, 9, 3, 2, "FU*",       N_("Variable Star of FU Ori type"))
T(14, 9, 3, 4, "RC*",       N_("Variable Star of R CrB type"))
T(14, 9, 3, 5, "RC?",       N_("Variable Star of R CrB type candiate"))
T(14, 9, 4, 0, "Ro*",     N_("Rotationally variable Star"))
T(14, 9, 4, 1, "a2*",       N_("Variable Star of alpha2 CVn type"))
T(14, 9, 4, 3, "Psr",       N_("Pulsar"))
T(14, 9, 4, 4, "BY*",       N_("Variable of BY Dra type"))
T(14, 9, 4, 5, "RS*",       N_("Variable of RS CVn type"))
T(14, 9, 5, 0, "Pu*",     N_("Pulsating variable Star"))
T(14, 9, 5, 2, "RR*",       N_("Variable Star of RR Lyr type"))
T(14, 9, 5, 3, "Ce*",       N_("Cepheid variable Star"))
T(14, 9, 5, 5, "dS*",       N_("Variable Star of delta Sct type"))
T(14, 9, 5, 6, "RV*",       N_("Variable Star of RV Tau type"))
T(14, 9, 5, 7, "WV*",       N_("Variable Star of W Vir type"))
T(14, 9, 5, 8, "bC*",       N_("Variable Star of beta Cep type"))
T(14, 9, 5, 9, "cC*",       N_("Classical Cepheid"))
T(14, 9, 5,10, "gD*",       N_("Variable Star of gamma Dor type"))
T(14, 9, 5,11, "SX*",       N_("Variable Star of SX Phe type"))
T(14, 9, 6, 0, "LP*",     N_("Long-period variable star"))
T(14, 9, 6, 1, "Mi*",       N_("Variable Star of Mira Cet type"))
T(14, 9, 6, 4, "sr*",       N_("Semi-regular pulsating Star"))
T(14, 9, 8, 0, "SN*",     N_("SuperNova"))
T(14,14, 0, 0, "su*",   N_("Sub-stellar object"))
T(14,14, 2, 0, "Pl?",     N_("Possible Extra-solar Planet"))
T(14,14,10, 0, "Pl" ,     N_("Extra-solar Confirmed Planet"))
T(15, 0, 0, 0, "G"  , N_("Galaxy"))
T(15, 1, 0, 0, "PoG",   N_("Part of a Galaxy"))
T(15, 2, 0, 0, "GiC",   N_("Galaxy in Cluster of Galaxies"))
T(15, 2, 2, 0, "BiC",     N_("Brightest Galaxy in a Cluster"))
T(15, 3, 0, 0, "GiG",   N_("Galaxy in Group of Galaxies"))
T(15, 4, 0, 0, "GiP",   N_("Galaxy in Pair of Galaxies"))
T(15, 5, 0, 0, "HzG",   N_("Galaxy with high redshift"))
T(15, 6, 0, 0, "ALS",   N_("Absorption Line system"))
T(15, 6, 1, 0, "LyA",     N_("Ly alpha Absorption Line system"))
T(15, 6, 2, 0, "DLA",     N_("Damped Ly-alpha Absorption Line system"))
T(15, 6, 3, 0, "mAL",     N_("metallic Absorption Line system"))
T(15, 6, 5, 0, "LLS",     N_("Lyman limit system"))
T(15, 6, 8, 0, "BAL",     N_("Broad Absorption Line system"))
T(15, 7, 0, 0, "rG" ,   N_("Radio Galaxy"))
T(15, 8, 0, 0, "H2G",   N_("HII Galaxy"))
T(15, 9, 0, 0, "LSB",   N_("Low Surface Brightness Galaxy"))
T(15,10, 0, 0, "AG?",   N_("Possible Active Galaxy Nucleus"))
T(15,10, 7, 0, "Q?" ,     N_("Possible Quasar"))
T(15,10,11, 0, "Bz?",     N_("Possible Blazar"))
T(15,10,17, 0, "BL?",     N_("Possible BL Lac"))
T(15,11, 0, 0, "EmG",   N_("Emission-line Galaxy"))
T(15,12, 0, 0, "SBG",   N_("Starburst Galaxy"))
T(15,13, 0, 0, "bCG",   N_("Blue compact Galaxy"))
T(15,14, 0, 0, "LeI",   N_("Gravitationally Lensed Image"))
T(15,14, 1, 0, "LeG",     N_("Gravitationally Lensed Image of a Galaxy"))
T(15,14, 7, 0, "LeQ",     N_("Gravitationally Lensed Image of a Quasar"))
T(15,15, 0, 0, "AGN",   N_("Active Galaxy Nucleus"))
T(15,15, 1, 0, "LIN",     N_("LINER-type Active Galaxy Nucleus"))
T(15,15, 2, 0, "SyG",     N_("Seyfert Galaxy"))
T(15,15, 2, 1, "Sy1",       N_("Seyfert 1 Galaxy"))
T(15,15, 2, 2, "Sy2",       N_("Seyfert 2 Galaxy"))
T(15,15, 3, 0, "Bla",     N_("Blazar"))
T(15,15, 3, 1, "BLL",       N_("BL Lac - type object"))
T(15,15, 3, 2, "OVV",       N_("Optically Violently Variable object"))
T(15,15, 4, 0, "QSO",     N_("Quasar"))

// Extra fields for Solar system objects.
T(16, 0, 0, 0, "SSO", N_("Solar System Object"))
T(16, 1, 0, 0, "Sun",   N_("Sun"))
T(16, 2, 0, 0, "Pla",   N_("Planet"))
T(16, 3, 0, 0, "Moo",   N_("Moon"))
T(16, 4, 0, 0, "Asa",   N_("Artificial Satellite"))
T(16, 4, 1, 0, "AsC",     N_("Communication Satellite"))
T(16, 4, 1, 1, "AsA",       N_("Amateur Radio Satellite"))
T(16, 4, 2, 0, "AsS",     N_("Science Satellite"))
T(16, 4, 2, 1, "SsS",       N_("Space Science Satellite"))
T(16, 4, 2, 2, "AsE",       N_("Earth Science Satellite"))
T(16, 4, 2, 3, "AEd",       N_("Education Satellite"))
T(16, 4, 2, 4, "AEn",       N_("Engineering Satellite"))
T(16, 4, 3, 0, "RB",      N_("Rocket Body"))
T(16, 4, 3, 1, "RB1",       N_("Rocket First Stage"))
T(16, 4, 3, 2, "RB2",       N_("Rocket Second Stage"))
T(16, 4, 4, 0, "AsD",     N_("Satellite Debris"))
T(16, 4, 5, 0, "AsP",     N_("Satellite Platform"))
T(16, 4, 6, 0, "SpS",     N_("Space Station"))
T(16, 4, 7, 0, "AsN",     N_("Navigation Satellite"))
T(16, 5, 0, 0, "MPl",   N_("Minor Planet"))
T(16, 5, 1, 0, "DPl",     N_("Dwarf Planet"))
T(16, 5, 2, 0, "Com",     N_("Comet"))
T(16, 5, 2, 1, "PCo",       N_("Periodic Comet"))
T(16, 5, 2, 2, "CCo",       N_("Non Periodic Comet"))
T(16, 5, 2, 3, "XCo",       N_("Unreliable (Historical) Comet"))
T(16, 5, 2, 4, "DCo",       N_("Disappeared Comet"))
T(16, 5, 2, 6, "ISt",       N_("Interstellar Object"))
T(16, 5, 3, 0, "NEO",     N_("Near Earth Object"))
T(16, 5, 3, 1, "Ati",       N_("Atira Asteroid"))
T(16, 5, 3, 2, "Ate",       N_("Aten Asteroid"))
T(16, 5, 3, 3, "Apo",       N_("Apollo Asteroid"))
T(16, 5, 3, 4, "Amo",       N_("Amor Asteroid"))
T(16, 5, 4, 0, "Hun",     N_("Hungaria Asteroid"))
T(16, 5, 5, 0, "Pho",     N_("Phocaea Asteroid"))
T(16, 5, 6, 0, "Hil",     N_("Hilda Asteroid"))
T(16, 5, 7, 0, "JTA",     N_("Jupiter Trojan Asteroid"))
T(16, 5, 8, 0, "DOA",     N_("Distant Object Asteroid"))
T(16, 5, 9, 0, "MBA",     N_("Main Belt Asteroid"))
T(16, 6, 0, 0, "IPS",   N_("Interplanetary Spacecraft"))
T(16, 7, 0, 0, "MSh",   N_("Meteor Shower"))

// Extra fields for Cultural Sky Representation
T(17, 0, 0, 0, "Cul", N_("Cultural Sky Representation"))
T(17, 1, 0, 0, "Con",   N_("Constellation"))
T(17, 2, 0, 0, "Ast",   N_("Asterism"))

// Extra fields for Coordinates.
T(18, 0, 0, 0, "Coo", N_("Coordinates"))

{}
};

#if COMPILE_TESTS

#include "tests.h"

static void test_otypes_hash(void)
{
    int i;
    const entry_t *e;
    for (i = 0, e = &ENTRIES[0]; e->id[0]; e++, i++) {
        assert(otypes_hash_search(e->id, strnlen(e->id, 4)) == i);
    }
}

TEST_REGISTER(NULL, test_otypes_hash, TEST_AUTO);

#endif
