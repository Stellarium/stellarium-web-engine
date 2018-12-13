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

#include "swe.h"

typedef struct entry
{
    uint8_t n1, n2, n3, n4;
    char cond[4];
    char *name;
    char *explanation;
} entry_t;

static const entry_t ENTRIES[];

// Return informations about a type.
int otypes_lookup(const char *condensed,
                  const char **name,
                  const char **explanation,
                  int ns[4])
{
    entry_t *e;
    char t[4] = {};
    int i;

    // Remove the spaces so that we can search for both '** ' and '**'.
    strncpy(t, condensed, 4);
    for (i = 0; i < 4; i++) if (t[i] == ' ') t[i] = '\0';

    for (e = &ENTRIES[0]; e->cond[0]; e++) {
        if (strncmp(t, e->cond, 4) == 0) {
            if (name) *name = e->name;
            if (explanation) *explanation = e->explanation;
            if (ns) {
                ns[0] = e->n1;
                ns[1] = e->n2;
                ns[2] = e->n3;
                ns[3] = e->n4;
            }
            return 0;
        }
    }
    return -1;
}

// Return the parent condensed id of an otype.
const char *otypes_get_parent(const char *condensed)
{
    int i, r, ns[4];
    entry_t *e;
    r = otypes_lookup(condensed, NULL, NULL, ns);
    if (r != 0) return NULL;
    // We get the parent number by setting the last non zero digit to zero.
    for (i = 0; i < 4; i++) {
        if (ns[i] == 0) break;
    }
    if (i == 0) return NULL;
    ns[i - 1] = 0;
    for (e = &ENTRIES[0]; e->cond[0]; e++) {
        if (    e->n1 == ns[0] &&
                e->n2 == ns[1] &&
                e->n3 == ns[2] &&
                e->n4 == ns[3])
            return e->cond;
    }
    return NULL;
}

// STYLE-CHECK OFF

// The actual database.  See 'tools/makeotype.py'.
static const entry_t ENTRIES[] = {
#define T(...) {__VA_ARGS__},
T( 0, 0, 0, 0, "?"  , "Unknown", "Object of unknown nature")
T( 0, 2, 0, 0, "ev" ,   "Transient", "transient event")
T( 1, 0, 0, 0, "Rad", "Radio", "Radio-source")
T( 1, 2, 0, 0, "mR" ,   "Radio(m)", "metric Radio-source")
T( 1, 4, 0, 0, "cm" ,   "Radio(cm)", "centimetric Radio-source")
T( 1, 6, 0, 0, "mm" ,   "Radio(mm)", "millimetric Radio-source")
T( 1, 8, 0, 0, "smm",   "Radio(sub-mm)", "sub-millimetric source")
T( 1,11, 0, 0, "HI" ,   "HI", "HI (21cm) source")
T( 1,12, 0, 0, "rB" ,   "radioBurst", "radio Burst")
T( 1,14, 0, 0, "Mas",   "Maser", "Maser")
T( 2, 0, 0, 0, "IR" , "IR", "Infra-Red source")
T( 2, 2, 0, 0, "FIR",   "IR>30um", "Far-IR source  (λ >= 30 µm)")
T( 2, 4, 0, 0, "NIR",   "IR<10um", "Near-IR source (λ < 10 µm)")
T( 3, 0, 0, 0, "red", "Red", "Very red source")
T( 3, 3, 0, 0, "ERO",   "RedExtreme", "Extremely Red Object")
T( 4, 0, 0, 0, "blu", "Blue", "Blue object")
T( 5, 0, 0, 0, "UV" , "UV", "UV-emission source")
T( 6, 0, 0, 0, "X"  , "X", "X-ray source")
T( 6, 2, 0, 0, "UX?",   "ULX?", "Ultra-luminous X-ray candidate")
T( 6,10, 0, 0, "ULX",   "ULX", "Ultra-luminous X-ray source")
T( 7, 0, 0, 0, "gam", "gamma", "gamma-ray source")
T( 7, 3, 0, 0, "gB" ,   "gammaBurst", "gamma-ray Burst")
T( 8, 0, 0, 0, "err", "Inexistent", "Not an object (error, artefact, ...)")
T( 9, 0, 0, 0, "grv", "Gravitation", "Gravitational Source")
T( 9, 3, 0, 0, "Lev",   "LensingEv", "(Micro)Lensing Event")
T( 9, 6, 0, 0, "LS?",   "Candidate_LensSystem", "Possible gravitational lens System")
T( 9, 7, 0, 0, "Le?",   "Candidate_Lens", "Possible gravitational lens")
T( 9, 8, 0, 0, "LI?",   "Possible_lensImage", "Possible gravitationally lensed image")
T( 9, 9, 0, 0, "gLe",   "GravLens", "Gravitational Lens")
T( 9,11, 0, 0, "gLS",   "GravLensSystem", "Gravitational Lens System (lens+images)")
T( 9,12, 0, 0, "GWE",   "GravWaveEvent", "Gravitational Wave Event")
T(10, 0, 0, 0, "..?", "Candidates", "Candidate objects")
T(10, 1, 0, 0, "G?" ,   "Possible_G", "Possible Galaxy")
T(10, 2, 0, 0, "SC?",   "Possible_SClG", "Possible Supercluster of Galaxies")
T(10, 3, 0, 0, "C?G",   "Possible_ClG", "Possible Cluster of Galaxies")
T(10, 4, 0, 0, "Gr?",   "Possible_GrG", "Possible Group of Galaxies")
T(10, 6, 0, 0, "As?",   "Possible_As*", "")
T(10,11, 0, 0, "**?",   "Candidate_**", "Physical Binary Candidate")
T(10,11, 1, 0, "EB?",     "Candidate_EB*", "Eclipsing Binary Candidate")
T(10,11,10, 0, "Sy?",     "Candidate_Symb*", "Symbiotic Star Candidate")
T(10,11,11, 0, "CV?",     "Candidate_CV*", "Cataclysmic Binary Candidate")
T(10,11,11, 6, "No?",       "Candidate_Nova", "Nova Candidate")
T(10,11,12, 0, "XB?",     "Candidate_XB*", "X-ray binary Candidate")
T(10,11,12, 2, "LX?",       "Candidate_LMXB", "Low-Mass X-ray binary Candidate")
T(10,11,12, 3, "HX?",       "Candidate_HMXB", "High-Mass X-ray binary Candidate")
T(10,12, 0, 0, "Pec?",   "Candidate_Pec*", "Possible Peculiar Star")
T(10,12, 1, 0, "Y*?",     "Candidate_YSO", "Young Stellar Object Candidate")
T(10,12, 2, 0, "pr?",     "Candidate_pMS*", "Pre-main sequence Star Candidate")
T(10,12, 2, 3, "TT?",       "Candidate_TTau*", "T Tau star Candidate")
T(10,12, 3, 0, "C*?",     "Candidate_C*", "Possible Carbon Star")
T(10,12, 4, 0, "S*?",     "Candidate_S*", "Possible S Star")
T(10,12, 5, 0, "OH?",     "Candidate_OH", "Possible Star with envelope of OH/IR type")
T(10,12, 6, 0, "CH?",     "Candidate_CH", "Possible Star with envelope of CH type")
T(10,12, 7, 0, "WR?",     "Candidate_WR*", "Possible Wolf-Rayet Star")
T(10,12, 8, 0, "Be?",     "Candidate_Be*", "Possible Be Star")
T(10,12, 9, 0, "Ae?",     "Candidate_Ae*", "Possible Herbig Ae/Be Star")
T(10,12,11, 0, "HB?",     "Candidate_HB*", "Possible Horizontal Branch Star")
T(10,12,11, 2, "RR?",       "Candidate_RRLyr", "Possible Star of RR Lyr type")
T(10,12,11, 3, "Ce?",       "Candidate_Cepheid", "Possible Cepheid")
T(10,12,12, 0, "RB?",     "Candidate_RGB*", "Possible Red Giant Branch star")
T(10,12,13, 0, "sg?",     "Candidate_SG*", "Possible Supergiant star")
T(10,12,13, 3, "s?r",       "Candidate_RSG*", "Possible Red supergiant star")
T(10,12,13, 4, "s?y",       "Candidate_YSG*", "Possible Yellow supergiant star")
T(10,12,13, 5, "s?b",       "Candidate_BSG*", "Possible Blue supergiant star")
T(10,12,14, 0, "AB?",     "Candidate_AGB*", "Asymptotic Giant Branch Star candidate")
T(10,12,14, 1, "LP?",       "Candidate_LP*", "Long Period Variable candidate")
T(10,12,14, 2, "Mi?",       "Candidate_Mi*", "Mira candidate")
T(10,12,14, 3, "sv?",       "Candiate_sr*", "Semi-regular variable candidate")
T(10,12,15, 0, "pA?",     "Candidate_post-AGB*", "Post-AGB Star Candidate")
T(10,12,16, 0, "BS?",     "Candidate_BSS", "Candidate blue Straggler Star")
T(10,12,17, 0, "HS?",     "Candidate_Hsd", "Hot subdwarf candidate")
T(10,12,18, 0, "WD?",     "Candidate_WD*", "White Dwarf Candidate")
T(10,12,20, 0, "N*?",     "Candidate_NS", "Neutron Star Candidate")
T(10,12,22, 0, "BH?",     "Candidate_BH", "Black Hole Candidate")
T(10,12,23, 0, "SN?",     "Candidate_SN*", "SuperNova Candidate")
T(10,12,24, 0, "LM?",     "Candidate_low-mass*", "Low-mass star candidate")
T(10,12,26, 0, "BD?",     "Candidate_brownD*", "Brown Dwarf Candidate")
T(12, 0, 0, 0, "mul", "multiple_object", "Composite object")
T(12, 1, 0, 0, "reg",   "Region", "Region defined in the sky")
T(12, 1, 5, 0, "vid",     "Void", "Underdense region of the Universe")
T(12, 2, 0, 0, "SCG",   "SuperClG", "Supercluster of Galaxies")
T(12, 3, 0, 0, "ClG",   "ClG", "Cluster of Galaxies")
T(12, 4, 0, 0, "GrG",   "GroupG", "Group of Galaxies")
T(12, 4, 5, 0, "CGG",     "Compact_Gr_G", "Compact Group of Galaxies")
T(12, 5, 0, 0, "PaG",   "PairG", "Pair of Galaxies")
T(12, 5, 5, 0, "IG" ,     "IG", "Interacting Galaxies")
T(12, 9, 0, 0, "C?*",   "Cl*?", "Possible (open) star cluster")
T(12,10, 0, 0, "Gl?",   "GlCl?", "Possible Globular Cluster")
T(12,11, 0, 0, "Cl*",   "Cl*", "Cluster of Stars")
T(12,11, 1, 0, "GlC",     "GlCl", "Globular Cluster")
T(12,11, 2, 0, "OpC",     "OpCl", "Open (galactic) Cluster")
T(12,12, 0, 0, "As*",   "Assoc*", "Association of Stars")
T(12,12, 1, 0, "St*",     "Stream*", "Stellar Stream")
T(12,12, 2, 0, "MGr",     "MouvGroup", "Moving Group")
T(12,13, 0, 0, "**" ,   "**", "Double or multiple star")
T(12,13, 1, 0, "EB*",     "EB*", "Eclipsing binary")
T(12,13, 1, 1, "Al*",       "EB*Algol", "Eclipsing binary of Algol type (detached)")
T(12,13, 1, 2, "bL*",       "EB*betLyr", "Eclipsing binary of beta Lyr type (semi-detached)")
T(12,13, 1, 3, "WU*",       "EB*WUMa", "Eclipsing binary of W UMa type (contact binary)")
T(12,13, 1, 8, "EP*",       "EB*Planet", "Star showing eclipses by its planet")
T(12,13, 2, 0, "SB*",     "SB*", "Spectroscopic binary")
T(12,13, 5, 0, "El*",     "EllipVar", "Ellipsoidal variable Star")
T(12,13,10, 0, "Sy*",     "Symbiotic*", "Symbiotic Star")
T(12,13,11, 0, "CV*",     "CataclyV*", "Cataclysmic Variable Star")
T(12,13,11, 2, "DQ*",       "DQHer", "CV DQ Her type (intermediate polar)")
T(12,13,11, 3, "AM*",       "AMHer", "CV of AM Her type (polar)")
T(12,13,11, 5, "NL*",       "Nova-like", "Nova-like Star")
T(12,13,11, 6, "No*",       "Nova", "Nova")
T(12,13,11, 7, "DN*",       "DwarfNova", "Dwarf Nova")
T(12,13,12, 0, "XB*",     "XB", "X-ray Binary")
T(12,13,12, 2, "LXB",       "LMXB", "Low Mass X-ray Binary")
T(12,13,12, 3, "HXB",       "HMXB", "High Mass X-ray Binary")
T(13, 0, 0, 0, "ISM", "ISM", "Interstellar matter")
T(13, 1, 0, 0, "PoC",   "PartofCloud", "Part of Cloud")
T(13, 2, 0, 0, "PN?",   "PN?", "Possible Planetary Nebula")
T(13, 3, 0, 0, "CGb",   "ComGlob", "Cometary Globule")
T(13, 4, 0, 0, "bub",   "Bubble", "Bubble")
T(13, 6, 0, 0, "EmO",   "EmObj", "Emission Object")
T(13, 8, 0, 0, "Cld",   "Cloud", "Cloud")
T(13, 8, 3, 0, "GNe",     "GalNeb", "Galactic Nebula")
T(13, 8, 4, 0, "BNe",     "BrNeb", "Bright Nebula")
T(13, 8, 6, 0, "DNe",     "DkNeb", "Dark Cloud (nebula)")
T(13, 8, 7, 0, "RNe",     "RfNeb", "Reflection Nebula")
T(13, 8,12, 0, "MoC",     "MolCld", "Molecular Cloud")
T(13, 8,12, 3, "glb",       "Globule", "Globule (low-mass dark cloud)")
T(13, 8,12, 6, "cor",       "denseCore", "Dense core")
T(13, 8,12, 8, "SFR",       "SFregion", "Star forming region")
T(13, 8,13, 0, "HVC",     "HVCld", "High-velocity Cloud")
T(13, 9, 0, 0, "HII",   "HII", "HII (ionized) region")
T(13,10, 0, 0, "PN" ,   "PN", "Planetary Nebula")
T(13,11, 0, 0, "sh" ,   "HIshell", "HI shell")
T(13,12, 0, 0, "SR?",   "SNR?", "SuperNova Remnant Candidate")
T(13,13, 0, 0, "SNR",   "SNR", "SuperNova Remnant")
T(13,14, 0, 0, "cir",   "Circumstellar", "CircumStellar matter")
T(13,14, 1, 0, "of?",     "outflow?", "Outflow candidate")
T(13,14,15, 0, "out",     "Outflow", "Outflow")
T(13,14,16, 0, "HH" ,     "HH", "Herbig-Haro Object")
T(14, 0, 0, 0, "*"  , "Star", "Star")
T(14, 1, 0, 0, "*iC",   "*inCl", "Star in Cluster")
T(14, 2, 0, 0, "*iN",   "*inNeb", "Star in Nebula")
T(14, 3, 0, 0, "*iA",   "*inAssoc", "Star in Association")
T(14, 4, 0, 0, "*i*",   "*in**", "Star in double system")
T(14, 5, 0, 0, "V*?",   "V*?", "Star suspected of Variability")
T(14, 6, 0, 0, "Pe*",   "Pec*", "Peculiar Star")
T(14, 6, 1, 0, "HB*",     "HB*", "Horizontal Branch Star")
T(14, 6, 2, 0, "Y*O",     "YSO", "Young Stellar Object")
T(14, 6, 2, 4, "Ae*",       "Ae*", "Herbig Ae/Be star")
T(14, 6, 5, 0, "Em*",     "Em*", "Emission-line Star")
T(14, 6, 5, 3, "Be*",       "Be*", "Be Star")
T(14, 6, 6, 0, "BS*",     "BlueStraggler", "Blue Straggler Star")
T(14, 6,10, 0, "RG*",     "RGB*", "Red Giant Branch star")
T(14, 6,12, 0, "AB*",     "AGB*", "Asymptotic Giant Branch Star (He-burning)")
T(14, 6,12, 3, "C*" ,       "C*", "Carbon Star")
T(14, 6,12, 6, "S*" ,       "S*", "S Star")
T(14, 6,13, 0, "sg*",     "SG*", "Evolved supergiant star")
T(14, 6,13, 3, "s*r",       "RedSG*", "Red supergiant star")
T(14, 6,13, 4, "s*y",       "YellowSG*", "Yellow supergiant star")
T(14, 6,13, 5, "s*b",       "BlueSG*", "Blue supergiant star")
T(14, 6,14, 0, "HS*",     "HotSubdwarf", "Hot subdwarf")
T(14, 6,15, 0, "pA*",     "post-AGB*", "Post-AGB Star (proto-PN)")
T(14, 6,16, 0, "WD*",     "WD*", "White Dwarf")
T(14, 6,16, 1, "ZZ*",       "pulsWD*", "Pulsating White Dwarf")
T(14, 6,17, 0, "LM*",     "low-mass*", "Low-mass star (M<1solMass)")
T(14, 6,18, 0, "BD*",     "brownD*", "Brown Dwarf (M<0.08solMass)")
T(14, 6,19, 0, "N*" ,     "Neutron*", "Confirmed Neutron Star")
T(14, 6,23, 0, "OH*",     "OH/IR", "OH/IR star")
T(14, 6,24, 0, "CH*",     "CH", "Star with envelope of CH type")
T(14, 6,25, 0, "pr*",     "pMS*", "Pre-main sequence Star")
T(14, 6,25, 3, "TT*",       "TTau*", "T Tau-type Star")
T(14, 6,30, 0, "WR*",     "WR*", "Wolf-Rayet Star")
T(14, 7, 0, 0, "PM*",   "PM*", "High proper-motion Star")
T(14, 8, 0, 0, "HV*",   "HV*", "High-velocity Star")
T(14, 9, 0, 0, "V*" ,   "V*", "Variable Star")
T(14, 9, 1, 0, "Ir*",     "Irregular_V*", "Variable Star of irregular type")
T(14, 9, 1, 1, "Or*",       "Orion_V*", "Variable Star of Orion Type")
T(14, 9, 1, 2, "RI*",       "Rapid_Irreg_V*", "Variable Star with rapid variations")
T(14, 9, 3, 0, "Er*",     "Eruptive*", "Eruptive variable Star")
T(14, 9, 3, 1, "Fl*",       "Flare*", "Flare Star")
T(14, 9, 3, 2, "FU*",       "FUOr", "Variable Star of FU Ori type")
T(14, 9, 3, 4, "RC*",       "Erupt*RCrB", "Variable Star of R CrB type")
T(14, 9, 3, 5, "RC?",       "RCrB_Candidate", "Variable Star of R CrB type candiate")
T(14, 9, 4, 0, "Ro*",     "RotV*", "Rotationally variable Star")
T(14, 9, 4, 1, "a2*",       "RotV*alf2CVn", "Variable Star of alpha2 CVn type")
T(14, 9, 4, 3, "Psr",       "Pulsar", "Pulsar")
T(14, 9, 4, 4, "BY*",       "BYDra", "Variable of BY Dra type")
T(14, 9, 4, 5, "RS*",       "RSCVn", "Variable of RS CVn type")
T(14, 9, 5, 0, "Pu*",     "PulsV*", "Pulsating variable Star")
T(14, 9, 5, 2, "RR*",       "RRLyr", "Variable Star of RR Lyr type")
T(14, 9, 5, 3, "Ce*",       "Cepheid", "Cepheid variable Star")
T(14, 9, 5, 5, "dS*",       "PulsV*delSct", "Variable Star of delta Sct type")
T(14, 9, 5, 6, "RV*",       "PulsV*RVTau", "Variable Star of RV Tau type")
T(14, 9, 5, 7, "WV*",       "PulsV*WVir", "Variable Star of W Vir type")
T(14, 9, 5, 8, "bC*",       "PulsV*bCep", "Variable Star of beta Cep type")
T(14, 9, 5, 9, "cC*",       "deltaCep", "Classical Cepheid (delta Cep type)")
T(14, 9, 5,10, "gD*",       "gammaDor", "Variable Star of gamma Dor type")
T(14, 9, 5,11, "SX*",       "pulsV*SX", "Variable Star of SX Phe type (subdwarf)")
T(14, 9, 6, 0, "LP*",     "LPV*", "Long-period variable star")
T(14, 9, 6, 1, "Mi*",       "Mira", "Variable Star of Mira Cet type")
T(14, 9, 6, 4, "sr*",       "semi-regV*", "Semi-regular pulsating Star")
T(14, 9, 8, 0, "SN*",     "SN", "SuperNova")
T(14,14, 0, 0, "su*",   "Sub-stellar", "Sub-stellar object")
T(14,14, 2, 0, "Pl?",     "Planet?", "Extra-solar Planet Candidate")
T(14,14,10, 0, "Pl" ,     "Planet", "Extra-solar Confirmed Planet")
T(15, 0, 0, 0, "G"  , "Galaxy", "Galaxy")
T(15, 1, 0, 0, "PoG",   "PartofG", "Part of a Galaxy")
T(15, 2, 0, 0, "GiC",   "GinCl", "Galaxy in Cluster of Galaxies")
T(15, 2, 2, 0, "BiC",     "BClG", "Brightest galaxy in a Cluster (BCG)")
T(15, 3, 0, 0, "GiG",   "GinGroup", "Galaxy in Group of Galaxies")
T(15, 4, 0, 0, "GiP",   "GinPair", "Galaxy in Pair of Galaxies")
T(15, 5, 0, 0, "HzG",   "High_z_G", "Galaxy with high redshift")
T(15, 6, 0, 0, "ALS",   "AbsLineSystem", "Absorption Line system")
T(15, 6, 1, 0, "LyA",     "Ly-alpha_ALS", "Ly alpha Absorption Line system")
T(15, 6, 2, 0, "DLA",     "DLy-alpha_ALS", "Damped Ly-alpha Absorption Line system")
T(15, 6, 3, 0, "mAL",     "metal_ALS", "metallic Absorption Line system")
T(15, 6, 5, 0, "LLS",     "Ly-limit_ALS", "Lyman limit system")
T(15, 6, 8, 0, "BAL",     "Broad_ALS", "Broad Absorption Line system")
T(15, 7, 0, 0, "rG" ,   "RadioG", "Radio Galaxy")
T(15, 8, 0, 0, "H2G",   "HII_G", "HII Galaxy")
T(15, 9, 0, 0, "LSB",   "LSB_G", "Low Surface Brightness Galaxy")
T(15,10, 0, 0, "AG?",   "AGN_Candidate", "Possible Active Galaxy Nucleus")
T(15,10, 7, 0, "Q?" ,     "QSO_Candidate", "Possible Quasar")
T(15,10,11, 0, "Bz?",     "Blazar_Candidate", "Possible Blazar")
T(15,10,17, 0, "BL?",     "BLLac_Candidate", "Possible BL Lac")
T(15,11, 0, 0, "EmG",   "EmG", "Emission-line galaxy")
T(15,12, 0, 0, "SBG",   "StarburstG", "Starburst Galaxy")
T(15,13, 0, 0, "bCG",   "BlueCompG", "Blue compact Galaxy")
T(15,14, 0, 0, "LeI",   "LensedImage", "Gravitationally Lensed Image")
T(15,14, 1, 0, "LeG",     "LensedG", "Gravitationally Lensed Image of a Galaxy")
T(15,14, 7, 0, "LeQ",     "LensedQ", "Gravitationally Lensed Image of a Quasar")
T(15,15, 0, 0, "AGN",   "AGN", "Active Galaxy Nucleus")
T(15,15, 1, 0, "LIN",     "LINER", "LINER-type Active Galaxy Nucleus")
T(15,15, 2, 0, "SyG",     "Seyfert", "Seyfert Galaxy")
T(15,15, 2, 1, "Sy1",       "Seyfert_1", "Seyfert 1 Galaxy")
T(15,15, 2, 2, "Sy2",       "Seyfert_2", "Seyfert 2 Galaxy")
T(15,15, 3, 0, "Bla",     "Blazar", "Blazar")
T(15,15, 3, 1, "BLL",       "BLLac", "BL Lac - type object")
T(15,15, 3, 2, "OVV",       "OVV", "Optically Violently Variable object")
T(15,15, 4, 0, "QSO",     "QSO", "Quasar")

// Extra fields for Solar system objects.
T(16, 0, 0, 0, "SSO", "SolarSystemObj", "Solar System Object")
T(16, 1, 0, 0, "Sun",   "Sun", "Sun")
T(16, 2, 0, 0, "Pla",   "Planet", "Planet")
T(16, 3, 0, 0, "Moo",   "Moon", "Moon")
T(16, 4, 0, 0, "Asa",   "ArtificialSatellite", "Artifical Satellite")
T(16, 5, 0, 0, "MPl",   "MinorPlanet", "Minor Planet")
T(16, 5, 1, 0, "DPl",   "DwarfPlanet", "Dwarf Planet")
T(16, 5, 2, 0, "Com",   "Comet", "Comet")
T(16, 5, 2, 1, "PCo",     "PerComet", "Periodic Comet")
T(16, 5, 2, 2, "CCo",     "NonPerComet", "Non Periodic Comet")
T(16, 5, 2, 3, "XCo",     "XComet", "Unreliable (Historical) Comet")
T(16, 5, 2, 4, "DCo",     "DisaComet", "Disappeared Comet")
T(16, 5, 2, 5, "ACo",     "AComet", "Minor  Planet")
T(16, 5, 2, 6, "ISt",     "InterstelObj", "Interstellar Object")
T(16, 5, 3, 0, "NEO",   "NEO", "Near Earth Object")
T(16, 5, 3, 1, "Ati",     "Atira", "Atira Asteroid")
T(16, 5, 3, 2, "Ate",     "Aten", "Aten Asteroid")
T(16, 5, 3, 3, "Apo",     "Apollo", "Apollo Asteroid")
T(16, 5, 3, 4, "Amo",     "Amor", "Amor Asteroid")
T(16, 5, 4, 0, "Hun",   "Hungaria", "Hungaria Asteroid")
T(16, 5, 5, 0, "Pho",   "Phocaea", "Phocaea Asteroid")
T(16, 5, 6, 0, "Hil",   "Hilda", "Hilda Asteroid")
T(16, 5, 7, 0, "JTA",   "JupiterTrojan", "Jupiter Trojan Asteroid")
T(16, 5, 8, 0, "DOA",   "DistantObject", "Distant Object Asteroid")
T(16, 5, 9, 0, "MBA",   "MainBeltAst", "Main Belt Asteroid")

// Extra fields for Cultural Sky Representation
T(17, 0, 0, 0, "Cul", "Cultural", "Cultural Sky Representation")
T(17, 1, 0, 0, "Con",   "Constellation", "Constellation")
T(17, 2, 0, 0, "Ast",   "Asterism", "Asterism")

{}
};
