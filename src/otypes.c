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

#include <stdint.h>
#include <string.h>

typedef struct entry
{
    uint8_t n[4];   // 4 digits number.
    char    id[5];  // 4 bytes id (use 5 to ensure null termination).
    char    *str;   // Long name.
} entry_t;

static const entry_t ENTRIES[];

static const entry_t *otype_get(const char *id)
{
    char t[4];
    int i;
    const entry_t *e;
    strncpy(t, id, 4);
    // Remove the spaces so that we can search for both '** ' and '**'.
    for (i = 0; i < 4; i++) if (t[i] == ' ') t[i] = '\0';
    for (e = &ENTRIES[0]; e->id[0]; e++) {
        if (memcmp(t, e->id, 4) == 0) return e;
    }
    return NULL;
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

// STYLE-CHECK OFF

// The actual database.  See 'tools/makeotype.py'.
static const entry_t ENTRIES[] = {
#define T(n0, n1, n2, n3, id, str) {{n0, n1, n2, n3}, id, str},
T( 0, 0, 0, 0, "?"  , "Object of unknown nature")
T( 0, 2, 0, 0, "ev" ,   "transient event")
T( 1, 0, 0, 0, "Rad", "Radio-source")
T( 1, 2, 0, 0, "mR" ,   "metric Radio-source")
T( 1, 4, 0, 0, "cm" ,   "centimetric Radio-source")
T( 1, 6, 0, 0, "mm" ,   "millimetric Radio-source")
T( 1, 8, 0, 0, "smm",   "sub-millimetric source")
T( 1,11, 0, 0, "HI" ,   "HI (21cm) source")
T( 1,12, 0, 0, "rB" ,   "radio Burst")
T( 1,14, 0, 0, "Mas",   "Maser")
T( 2, 0, 0, 0, "IR" , "Infra-Red source")
T( 2, 2, 0, 0, "FIR",   "Far-IR source (λ >= 30 µm)")
T( 2, 4, 0, 0, "NIR",   "Near-IR source (λ < 10 µm)")
T( 3, 0, 0, 0, "red", "Very red source")
T( 3, 3, 0, 0, "ERO",   "Extremely Red Object")
T( 4, 0, 0, 0, "blu", "Blue object")
T( 5, 0, 0, 0, "UV" , "UV-emission source")
T( 6, 0, 0, 0, "X"  , "X-ray source")
T( 6, 2, 0, 0, "UX?",   "Ultra-luminous X-ray candidate")
T( 6,10, 0, 0, "ULX",   "Ultra-luminous X-ray source")
T( 7, 0, 0, 0, "gam", "gamma-ray source")
T( 7, 3, 0, 0, "gB" ,   "gamma-ray Burst")
T( 8, 0, 0, 0, "err", "Not an object (error, artefact, ...)")
T( 9, 0, 0, 0, "grv", "Gravitational Source")
T( 9, 3, 0, 0, "Lev",   "(Micro)Lensing Event")
T( 9, 6, 0, 0, "LS?",   "Possible gravitational lens System")
T( 9, 7, 0, 0, "Le?",   "Possible gravitational lens")
T( 9, 8, 0, 0, "LI?",   "Possible gravitationally lensed image")
T( 9, 9, 0, 0, "gLe",   "Gravitational Lens")
T( 9,11, 0, 0, "gLS",   "Gravitational Lens System (lens+images)")
T( 9,12, 0, 0, "GWE",   "Gravitational Wave Event")
T(10, 0, 0, 0, "..?", "Candidate objects")
T(10, 1, 0, 0, "G?" ,   "Possible Galaxy")
T(10, 2, 0, 0, "SC?",   "Possible Supercluster of Galaxies")
T(10, 3, 0, 0, "C?G",   "Possible Cluster of Galaxies")
T(10, 4, 0, 0, "Gr?",   "Possible Group of Galaxies")
T(10, 6, 0, 0, "As?",   "Possible Association of Stars")
T(10,11, 0, 0, "**?",   "Physical Binary Candidate")
T(10,11, 1, 0, "EB?",     "Eclipsing Binary Candidate")
T(10,11,10, 0, "Sy?",     "Symbiotic Star Candidate")
T(10,11,11, 0, "CV?",     "Cataclysmic Binary Candidate")
T(10,11,11, 6, "No?",       "Nova Candidate")
T(10,11,12, 0, "XB?",     "X-ray binary Candidate")
T(10,11,12, 2, "LX?",       "Low-Mass X-ray binary Candidate")
T(10,11,12, 3, "HX?",       "High-Mass X-ray binary Candidate")
T(10,12, 0, 0, "Pec?",   "Possible Peculiar Star")
T(10,12, 1, 0, "Y*?",     "Young Stellar Object Candidate")
T(10,12, 2, 0, "pr?",     "Pre-main sequence Star Candidate")
T(10,12, 2, 3, "TT?",       "T Tau star Candidate")
T(10,12, 3, 0, "C*?",     "Possible Carbon Star")
T(10,12, 4, 0, "S*?",     "Possible S Star")
T(10,12, 5, 0, "OH?",     "Possible Star with envelope of OH/IR type")
T(10,12, 6, 0, "CH?",     "Possible Star with envelope of CH type")
T(10,12, 7, 0, "WR?",     "Possible Wolf-Rayet Star")
T(10,12, 8, 0, "Be?",     "Possible Be Star")
T(10,12, 9, 0, "Ae?",     "Possible Herbig Ae/Be Star")
T(10,12,11, 0, "HB?",     "Possible Horizontal Branch Star")
T(10,12,11, 2, "RR?",       "Possible Star of RR Lyr type")
T(10,12,11, 3, "Ce?",       "Possible Cepheid")
T(10,12,12, 0, "RB?",     "Possible Red Giant Branch star")
T(10,12,13, 0, "sg?",     "Possible Supergiant star")
T(10,12,13, 3, "s?r",       "Possible Red supergiant star")
T(10,12,13, 4, "s?y",       "Possible Yellow supergiant star")
T(10,12,13, 5, "s?b",       "Possible Blue supergiant star")
T(10,12,14, 0, "AB?",     "Asymptotic Giant Branch Star candidate")
T(10,12,14, 1, "LP?",       "Long Period Variable candidate")
T(10,12,14, 2, "Mi?",       "Mira candidate")
T(10,12,14, 3, "sv?",       "Semi-regular variable candidate")
T(10,12,15, 0, "pA?",     "Post-AGB Star Candidate")
T(10,12,16, 0, "BS?",     "Candidate blue Straggler Star")
T(10,12,17, 0, "HS?",     "Hot subdwarf candidate")
T(10,12,18, 0, "WD?",     "White Dwarf Candidate")
T(10,12,20, 0, "N*?",     "Neutron Star Candidate")
T(10,12,22, 0, "BH?",     "Black Hole Candidate")
T(10,12,23, 0, "SN?",     "SuperNova Candidate")
T(10,12,24, 0, "LM?",     "Low-mass star candidate")
T(10,12,26, 0, "BD?",     "Brown Dwarf Candidate")
T(12, 0, 0, 0, "mul", "Composite object")
T(12, 1, 0, 0, "reg",   "Region defined in the sky")
T(12, 1, 5, 0, "vid",     "Underdense region of the Universe")
T(12, 2, 0, 0, "SCG",   "Supercluster of Galaxies")
T(12, 3, 0, 0, "ClG",   "Cluster of Galaxies")
T(12, 4, 0, 0, "GrG",   "Group of Galaxies")
T(12, 4, 5, 0, "CGG",     "Compact Group of Galaxies")
T(12, 5, 0, 0, "PaG",   "Pair of Galaxies")
T(12, 5, 5, 0, "IG" ,     "Interacting Galaxies")
T(12, 9, 0, 0, "C?*",   "Possible (open) star cluster")
T(12,10, 0, 0, "Gl?",   "Possible Globular Cluster")
T(12,11, 0, 0, "Cl*",   "Cluster of Stars")
T(12,11, 1, 0, "GlC",     "Globular Cluster")
T(12,11, 2, 0, "OpC",     "Open Cluster")
T(12,12, 0, 0, "As*",   "Association of Stars")
T(12,12, 1, 0, "St*",     "Stellar Stream")
T(12,12, 2, 0, "MGr",     "Moving Group")
T(12,13, 0, 0, "**" ,   "Double or multiple star")
T(12,13, 1, 0, "EB*",     "Eclipsing binary")
T(12,13, 1, 1, "Al*",       "Eclipsing binary of Algol type (detached)")
T(12,13, 1, 2, "bL*",       "Eclipsing binary of beta Lyr type (semi-detached)")
T(12,13, 1, 3, "WU*",       "Eclipsing binary of W UMa type (contact binary)")
T(12,13, 1, 8, "EP*",       "Star showing eclipses by its planet")
T(12,13, 2, 0, "SB*",     "Spectroscopic binary")
T(12,13, 5, 0, "El*",     "Ellipsoidal variable Star")
T(12,13,10, 0, "Sy*",     "Symbiotic Star")
T(12,13,11, 0, "CV*",     "Cataclysmic Variable Star")
T(12,13,11, 2, "DQ*",       "CV DQ Her type (intermediate polar)")
T(12,13,11, 3, "AM*",       "CV of AM Her type (polar)")
T(12,13,11, 5, "NL*",       "Nova-like Star")
T(12,13,11, 6, "No*",       "Nova")
T(12,13,11, 7, "DN*",       "Dwarf Nova")
T(12,13,12, 0, "XB*",     "X-ray Binary")
T(12,13,12, 2, "LXB",       "Low Mass X-ray Binary")
T(12,13,12, 3, "HXB",       "High Mass X-ray Binary")
T(13, 0, 0, 0, "ISM", "Interstellar matter")
T(13, 1, 0, 0, "PoC",   "Part of Cloud")
T(13, 2, 0, 0, "PN?",   "Possible Planetary Nebula")
T(13, 3, 0, 0, "CGb",   "Cometary Globule")
T(13, 4, 0, 0, "bub",   "Bubble")
T(13, 6, 0, 0, "EmO",   "Emission Object")
T(13, 8, 0, 0, "Cld",   "Cloud")
T(13, 8, 3, 0, "GNe",     "Galactic Nebula")
T(13, 8, 4, 0, "BNe",     "Bright Nebula")
T(13, 8, 6, 0, "DNe",     "Dark Cloud (nebula)")
T(13, 8, 7, 0, "RNe",     "Reflection Nebula")
T(13, 8,12, 0, "MoC",     "Molecular Cloud")
T(13, 8,12, 3, "glb",       "Globule (low-mass dark cloud)")
T(13, 8,12, 6, "cor",       "Dense core")
T(13, 8,12, 8, "SFR",       "Star forming region")
T(13, 8,13, 0, "HVC",     "High-velocity Cloud")
T(13, 9, 0, 0, "HII",   "HII region")
T(13,10, 0, 0, "PN" ,   "Planetary Nebula")
T(13,11, 0, 0, "sh" ,   "HI shell")
T(13,12, 0, 0, "SR?",   "SuperNova Remnant Candidate")
T(13,13, 0, 0, "SNR",   "SuperNova Remnant")
T(13,14, 0, 0, "cir",   "CircumStellar matter")
T(13,14, 1, 0, "of?",     "Outflow candidate")
T(13,14,15, 0, "out",     "Outflow")
T(13,14,16, 0, "HH" ,     "Herbig-Haro Object")
T(14, 0, 0, 0, "*"  , "Star")
T(14, 1, 0, 0, "*iC",   "Star in Cluster")
T(14, 2, 0, 0, "*iN",   "Star in Nebula")
T(14, 3, 0, 0, "*iA",   "Star in Association")
T(14, 4, 0, 0, "*i*",   "Star in double system")
T(14, 5, 0, 0, "V*?",   "Star suspected of Variability")
T(14, 6, 0, 0, "Pe*",   "Peculiar Star")
T(14, 6, 1, 0, "HB*",     "Horizontal Branch Star")
T(14, 6, 2, 0, "Y*O",     "Young Stellar Object")
T(14, 6, 2, 4, "Ae*",       "Herbig Ae/Be star")
T(14, 6, 5, 0, "Em*",     "Emission-line Star")
T(14, 6, 5, 3, "Be*",       "Be Star")
T(14, 6, 6, 0, "BS*",     "Blue Straggler Star")
T(14, 6,10, 0, "RG*",     "Red Giant Branch star")
T(14, 6,12, 0, "AB*",     "Asymptotic Giant Branch Star (He-burning)")
T(14, 6,12, 3, "C*" ,       "Carbon Star")
T(14, 6,12, 6, "S*" ,       "S Star")
T(14, 6,13, 0, "sg*",     "Evolved supergiant star")
T(14, 6,13, 3, "s*r",       "Red supergiant star")
T(14, 6,13, 4, "s*y",       "Yellow supergiant star")
T(14, 6,13, 5, "s*b",       "Blue supergiant star")
T(14, 6,14, 0, "HS*",     "Hot subdwarf")
T(14, 6,15, 0, "pA*",     "Post-AGB Star (proto-PN)")
T(14, 6,16, 0, "WD*",     "White Dwarf")
T(14, 6,16, 1, "ZZ*",       "Pulsating White Dwarf")
T(14, 6,17, 0, "LM*",     "Low-mass star (M<1solMass)")
T(14, 6,18, 0, "BD*",     "Brown Dwarf (M<0.08solMass)")
T(14, 6,19, 0, "N*" ,     "Confirmed Neutron Star")
T(14, 6,23, 0, "OH*",     "OH/IR star")
T(14, 6,24, 0, "CH*",     "Star with envelope of CH type")
T(14, 6,25, 0, "pr*",     "Pre-main sequence Star")
T(14, 6,25, 3, "TT*",       "T Tau-type Star")
T(14, 6,30, 0, "WR*",     "Wolf-Rayet Star")
T(14, 7, 0, 0, "PM*",   "High proper-motion Star")
T(14, 8, 0, 0, "HV*",   "High-velocity Star")
T(14, 9, 0, 0, "V*" ,   "Variable Star")
T(14, 9, 1, 0, "Ir*",     "Variable Star of irregular type")
T(14, 9, 1, 1, "Or*",       "Variable Star of Orion Type")
T(14, 9, 1, 2, "RI*",       "Variable Star with rapid variations")
T(14, 9, 3, 0, "Er*",     "Eruptive variable Star")
T(14, 9, 3, 1, "Fl*",       "Flare Star")
T(14, 9, 3, 2, "FU*",       "Variable Star of FU Ori type")
T(14, 9, 3, 4, "RC*",       "Variable Star of R CrB type")
T(14, 9, 3, 5, "RC?",       "Variable Star of R CrB type candiate")
T(14, 9, 4, 0, "Ro*",     "Rotationally variable Star")
T(14, 9, 4, 1, "a2*",       "Variable Star of alpha2 CVn type")
T(14, 9, 4, 3, "Psr",       "Pulsar")
T(14, 9, 4, 4, "BY*",       "Variable of BY Dra type")
T(14, 9, 4, 5, "RS*",       "Variable of RS CVn type")
T(14, 9, 5, 0, "Pu*",     "Pulsating variable Star")
T(14, 9, 5, 2, "RR*",       "Variable Star of RR Lyr type")
T(14, 9, 5, 3, "Ce*",       "Cepheid variable Star")
T(14, 9, 5, 5, "dS*",       "Variable Star of delta Sct type")
T(14, 9, 5, 6, "RV*",       "Variable Star of RV Tau type")
T(14, 9, 5, 7, "WV*",       "Variable Star of W Vir type")
T(14, 9, 5, 8, "bC*",       "Variable Star of beta Cep type")
T(14, 9, 5, 9, "cC*",       "Classical Cepheid (delta Cep type)")
T(14, 9, 5,10, "gD*",       "Variable Star of gamma Dor type")
T(14, 9, 5,11, "SX*",       "Variable Star of SX Phe type (subdwarf)")
T(14, 9, 6, 0, "LP*",     "Long-period variable star")
T(14, 9, 6, 1, "Mi*",       "Variable Star of Mira Cet type")
T(14, 9, 6, 4, "sr*",       "Semi-regular pulsating Star")
T(14, 9, 8, 0, "SN*",     "SuperNova")
T(14,14, 0, 0, "su*",   "Sub-stellar object")
T(14,14, 2, 0, "Pl?",     "Extra-solar Planet Candidate")
T(14,14,10, 0, "Pl" ,     "Extra-solar Confirmed Planet")
T(15, 0, 0, 0, "G"  , "Galaxy")
T(15, 1, 0, 0, "PoG",   "Part of a Galaxy")
T(15, 2, 0, 0, "GiC",   "Galaxy in Cluster of Galaxies")
T(15, 2, 2, 0, "BiC",     "Brightest galaxy in a Cluster (BCG)")
T(15, 3, 0, 0, "GiG",   "Galaxy in Group of Galaxies")
T(15, 4, 0, 0, "GiP",   "Galaxy in Pair of Galaxies")
T(15, 5, 0, 0, "HzG",   "Galaxy with high redshift")
T(15, 6, 0, 0, "ALS",   "Absorption Line system")
T(15, 6, 1, 0, "LyA",     "Ly alpha Absorption Line system")
T(15, 6, 2, 0, "DLA",     "Damped Ly-alpha Absorption Line system")
T(15, 6, 3, 0, "mAL",     "metallic Absorption Line system")
T(15, 6, 5, 0, "LLS",     "Lyman limit system")
T(15, 6, 8, 0, "BAL",     "Broad Absorption Line system")
T(15, 7, 0, 0, "rG" ,   "Radio Galaxy")
T(15, 8, 0, 0, "H2G",   "HII Galaxy")
T(15, 9, 0, 0, "LSB",   "Low Surface Brightness Galaxy")
T(15,10, 0, 0, "AG?",   "Possible Active Galaxy Nucleus")
T(15,10, 7, 0, "Q?" ,     "Possible Quasar")
T(15,10,11, 0, "Bz?",     "Possible Blazar")
T(15,10,17, 0, "BL?",     "Possible BL Lac")
T(15,11, 0, 0, "EmG",   "Emission-line galaxy")
T(15,12, 0, 0, "SBG",   "Starburst Galaxy")
T(15,13, 0, 0, "bCG",   "Blue compact Galaxy")
T(15,14, 0, 0, "LeI",   "Gravitationally Lensed Image")
T(15,14, 1, 0, "LeG",     "Gravitationally Lensed Image of a Galaxy")
T(15,14, 7, 0, "LeQ",     "Gravitationally Lensed Image of a Quasar")
T(15,15, 0, 0, "AGN",   "Active Galaxy Nucleus")
T(15,15, 1, 0, "LIN",     "LINER-type Active Galaxy Nucleus")
T(15,15, 2, 0, "SyG",     "Seyfert Galaxy")
T(15,15, 2, 1, "Sy1",       "Seyfert 1 Galaxy")
T(15,15, 2, 2, "Sy2",       "Seyfert 2 Galaxy")
T(15,15, 3, 0, "Bla",     "Blazar")
T(15,15, 3, 1, "BLL",       "BL Lac - type object")
T(15,15, 3, 2, "OVV",       "Optically Violently Variable object")
T(15,15, 4, 0, "QSO",     "Quasar")

// Extra fields for Solar system objects.
T(16, 0, 0, 0, "SSO", "Solar System Object")
T(16, 1, 0, 0, "Sun",   "Sun")
T(16, 2, 0, 0, "Pla",   "Planet")
T(16, 3, 0, 0, "Moo",   "Moon")
T(16, 4, 0, 0, "Asa",   "Artifical Earth Satellite")
T(16, 5, 0, 0, "MPl",   "Minor Planet")
T(16, 5, 1, 0, "DPl",   "Dwarf Planet")
T(16, 5, 2, 0, "Com",   "Comet")
T(16, 5, 2, 1, "PCo",     "Periodic Comet")
T(16, 5, 2, 2, "CCo",     "Non Periodic Comet")
T(16, 5, 2, 3, "XCo",     "Unreliable (Historical) Comet")
T(16, 5, 2, 4, "DCo",     "Disappeared Comet")
T(16, 5, 2, 5, "ACo",     "Minor  Planet")
T(16, 5, 2, 6, "ISt",     "Interstellar Object")
T(16, 5, 3, 0, "NEO",   "Near Earth Object")
T(16, 5, 3, 1, "Ati",     "Atira Asteroid")
T(16, 5, 3, 2, "Ate",     "Aten Asteroid")
T(16, 5, 3, 3, "Apo",     "Apollo Asteroid")
T(16, 5, 3, 4, "Amo",     "Amor Asteroid")
T(16, 5, 4, 0, "Hun",   "Hungaria Asteroid")
T(16, 5, 5, 0, "Pho",   "Phocaea Asteroid")
T(16, 5, 6, 0, "Hil",   "Hilda Asteroid")
T(16, 5, 7, 0, "JTA",   "Jupiter Trojan Asteroid")
T(16, 5, 8, 0, "DOA",   "Distant Object Asteroid")
T(16, 5, 9, 0, "MBA",   "Main Belt Asteroid")
T(16, 6, 0, 0, "IPS",   "Interplanetary Spacecraft")

// Extra fields for Cultural Sky Representation
T(17, 0, 0, 0, "Cul", "Cultural Sky Representation")
T(17, 1, 0, 0, "Con",   "Constellation")
T(17, 2, 0, 0, "Ast",   "Asterism")

{}
};
