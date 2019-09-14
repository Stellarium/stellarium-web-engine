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

// N_ macro for gettext parsing.
#ifndef N_
#define N_(s) s
#endif

typedef struct entry
{
    uint8_t n[4];   // 4 digits number.
    char    id[5];  // 4 bytes id (use 5 to ensure null termination).
    char    *str_en;   // Long name in English.
    char    *str_pl;   // Long name in Polish.
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
const char *otype_get_str(const char *id, const char *lang)
{
    const entry_t *e;
    e = otype_get(id);
    
    if (e) {
        if (strcmp(lang, "en") == 0) {
            return e->str_en;
        } else if (strcmp(lang, "pl") == 0) {
            return e->str_pl;
        } else {
            return NULL; // temp
        }
    } else {
        return NULL;
    }
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

void otype_get_digits(const char *id, uint8_t out[4])
{
    const entry_t *e;
    e = otype_get(id);
    assert(e);
    memcpy(out, e->n, 4);
}

// STYLE-CHECK OFF

// The actual database.  See 'tools/makeotype.py'.
static const entry_t ENTRIES[] = {
#define T(n0, n1, n2, n3, id, str_en, str_pl) {{n0, n1, n2, n3}, id, str_en, str_pl},
T( 0, 0, 0, 0, "?"  , N_("Object of unknown nature"), N_("Obiekt nieznanej natury"))
T( 0, 2, 0, 0, "ev" ,   N_("transient event"), N_("Chwilowe wydarzenie"))
T( 1, 0, 0, 0, "Rad", N_("Radio-source"), N_("Źródło radiowe"))
T( 1, 2, 0, 0, "mR" ,   N_("metric Radio-source"), N_("metrowe źródło radiowe"))
T( 1, 4, 0, 0, "cm" ,   N_("centimetric Radio-source"), N_("centymetrowe źródło radiowe"))
T( 1, 6, 0, 0, "mm" ,   N_("millimetric Radio-source"), N_("milimetrowe źródło radiowe"))
T( 1, 8, 0, 0, "smm",   N_("sub-millimetric source"), N_("submilimetrowe źródło radiowe"))
T( 1,11, 0, 0, "HI" ,   N_("HI source"), N_("Źródło wodoru HI"))
T( 1,12, 0, 0, "rB" ,   N_("radio Burst"), N_("Szybki błysk radiowy"))
T( 1,14, 0, 0, "Mas",   N_("Maser"), N_("Maser"))
T( 2, 0, 0, 0, "IR" , N_("Infra-Red source"), N_("Źródło podczerwieni"))
T( 2, 2, 0, 0, "FIR",   N_("Far-IR source"), N_("Żródło dalekiej podczerwieni"))
T( 2, 4, 0, 0, "NIR",   N_("Near-IR source"), N_("Źródło bliskiej podczerwieni"))
T( 3, 0, 0, 0, "red", N_("Very red source"), N_("Bardzo czerwone źródło"))
T( 3, 3, 0, 0, "ERO",   N_("Extremely Red Object"), N_("Ekstremalnie czerwony obiekt"))
T( 4, 0, 0, 0, "blu", N_("Blue object"), N_("Niebieski obiekt"))
T( 5, 0, 0, 0, "UV" , N_("UV-emission source"), N_("Źródło promieniowania UV"))
T( 6, 0, 0, 0, "X"  , N_("X-ray source"), N_("Żródło promieniowania rentgenowskiego"))
T( 6, 2, 0, 0, "UX?",   N_("Possible Ultra-luminous X-ray"), N_("Możliwe ultraintensywne źródło rentgenowskie"))
T( 6,10, 0, 0, "ULX",   N_("Ultra-luminous X-ray source"), N_("Ultraintensywne źródło rentgenowskie"))
T( 7, 0, 0, 0, "gam", N_("gamma-ray source"), N_("Źródło promieniowania gamma"))
T( 7, 3, 0, 0, "gB" ,   N_("gamma-ray Burst"), N_("Rozbłysk gamma"))
T( 8, 0, 0, 0, "err", N_("Not an object"), N_("To nie jest obiekt (błąd, artefakt)"))
T( 9, 0, 0, 0, "grv", N_("Gravitational Source"), N_("Żródło gravitacyjne"))
T( 9, 3, 0, 0, "Lev",   N_("(Micro)Lensing Event"), N_("(Mikro)soczewkowanie grawitacyjne"))
T( 9, 6, 0, 0, "LS?",   N_("Possible gravitational lens System"), N_("Możliwy system soczewkowany grawitacyjnie"))
T( 9, 7, 0, 0, "Le?",   N_("Possible gravitational lens"), N_("Możliwa soczewka grawitacyjna"))
T( 9, 8, 0, 0, "LI?",   N_("Possible gravitationally lensed image"), N_("Możliwy obraz soczewkowany grawitacyjnie"))
T( 9, 9, 0, 0, "gLe",   N_("Gravitational Lens"), N_("Soczewka grawitacyjna"))
T( 9,11, 0, 0, "gLS",   N_("Gravitational Lens System"), N_("System soczewkowany grawitacyjnie"))
T( 9,12, 0, 0, "GWE",   N_("Gravitational Wave Event"), N_("Źródło fal grawitacyjnych"))
T(10, 0, 0, 0, "..?", N_("Candidate objects"), N_("Możliwe obiekty"))
T(10, 1, 0, 0, "G?" ,   N_("Possible Galaxy"), N_("Możliwa galaktyka"))
T(10, 2, 0, 0, "SC?",   N_("Possible Supercluster of Galaxies"), N_("Możliwa supergromada galaktyk"))
T(10, 3, 0, 0, "C?G",   N_("Possible Cluster of Galaxies"), N_("Możliwa gromada galaktyk"))
T(10, 4, 0, 0, "Gr?",   N_("Possible Group of Galaxies"), N_("Możliwa grupa galaktyk"))
T(10, 6, 0, 0, "As?",   N_("Possible Association of Stars"), N_("Możliwa asocjacja gwiazdowa"))
T(10,11, 0, 0, "**?",   N_("Possible Physical Binary"), N_("Możliwa fizyczna gwiazda podwójna"))
T(10,11, 1, 0, "EB?",     N_("Possible Eclipsing Binary"), N_("Możliwa gwiazda podwójna zaćmieniowa"))
T(10,11,10, 0, "Sy?",     N_("Possible Symbiotic Star"), N_("Możliwa gwiazda w układzie symbiotycznym"))
T(10,11,11, 0, "CV?",     N_("Possible Cataclysmic Binary"), N_("Możliwa gwiazda w układzie kataklizmicznym"))
T(10,11,11, 6, "No?",       N_("Possible Nova"), N_("Możliwa nowa"))
T(10,11,12, 0, "XB?",     N_("Possible X-ray binary"), N_("Możliwy rentgenowski układ podwójny"))
T(10,11,12, 2, "LX?",       N_("Possible Low-Mass X-ray binary"), N_("Możliwy rentgenowski układ podwójny o małej masie"))
T(10,11,12, 3, "HX?",       N_("Possible High-Mass X-ray binary"), N_("Możliwy rentgenowski układ podwójny o wysokiej masie"))
T(10,12, 0, 0, "Pec?",   N_("Possible Peculiar Star"), N_("Możliwa gwiazda typu CP"))
T(10,12, 1, 0, "Y*?",     N_("Possible Young Stellar Object"), N_("Możliwy młody obiekt gwiezdny"))
T(10,12, 2, 0, "pr?",     N_("Possible Pre-main sequence Star"), N_("Gwiazda przed ciągiem głównym"))
T(10,12, 2, 3, "TT?",       N_("Possible T Tau star"), N_("Możliwa gwiazda typu T Tau"))
T(10,12, 3, 0, "C*?",     N_("Possible Carbon Star"), N_("Możliwa gwiazda węglowa"))
T(10,12, 4, 0, "S*?",     N_("Possible S Star"), N_("Możliwa gwiazda typu widmowego S"))
T(10,12, 5, 0, "OH?",     N_("Possible Star with envelope of OH/IR type"), N_("Możliwa gwiazda z otoczką typu OH/IR"))
T(10,12, 6, 0, "CH?",     N_("Possible Star with envelope of CH type"), N_("Możliwa gwiazda z otoczką typu CH"))
T(10,12, 7, 0, "WR?",     N_("Possible Wolf-Rayet Star"), N_("Możliwa gwiazda typu Wolfa-Rayeta"))
T(10,12, 8, 0, "Be?",     N_("Possible Be Star"), N_("Możliwa gwiazda Be"))
T(10,12, 9, 0, "Ae?",     N_("Possible Herbig Ae/Be Star"), N_("Możliwa gwiazda typu Herbiga Ae/Be"))
T(10,12,11, 0, "HB?",     N_("Possible Horizontal Branch Star"), N_("Możliwa gwiazda typu HB"))
T(10,12,11, 2, "RR?",       N_("Possible Star of RR Lyr type"), N_("Możliwa gwiazda typu RR Lyr"))
T(10,12,11, 3, "Ce?",       N_("Possible Cepheid"), N_("Możliwa Cefeida"))
T(10,12,12, 0, "RB?",     N_("Possible Red Giant Branch star"), N_("Możliwy czerwony olbrzym"))
T(10,12,13, 0, "sg?",     N_("Possible Supergiant star"), N_("Możliwy supergigant"))
T(10,12,13, 3, "s?r",       N_("Possible Red supergiant star"), N_("Możliwy czerwony supergigant"))
T(10,12,13, 4, "s?y",       N_("Possible Yellow supergiant star"), N_("Możliwy żółty supergigant"))
T(10,12,13, 5, "s?b",       N_("Possible Blue supergiant star"), N_("Możliwy niebieski supergigant"))
T(10,12,14, 0, "AB?",     N_("Possible Asymptotic Giant Branch Star"), N_("Możliwa gwiazda typu AGB"))
T(10,12,14, 1, "LP?",       N_("Possible Long Period Variable"), N_("Możliwa długookresowa gwiazda zmienna"))
T(10,12,14, 2, "Mi?",       N_("Possible Mira"), N_("Możliwa gwiazda zmienna typu Mira Cet"))
T(10,12,14, 3, "sv?",       N_("Possible Semi-regular variable"), N_("Możliwa pół-regularna gwiazda zmienna"))
T(10,12,15, 0, "pA?",     N_("Possible Post-AGB Star"), N_("Możliwa gwiazda po stadium AGB"))
T(10,12,16, 0, "BS?",     N_("Possible Blue Straggler Star"), N_("Możliwy błękitny maruder"))
T(10,12,17, 0, "HS?",     N_("Possible Hot subdwarf"), N_("Możliwy gorący podkarzeł"))
T(10,12,18, 0, "WD?",     N_("Possible White Dwarf"), N_("Możliwy biały karzeł"))
T(10,12,20, 0, "N*?",     N_("Possible Neutron Star"), N_("Możliwa gwiazda neutronowa"))
T(10,12,22, 0, "BH?",     N_("Possible Black Hole"), N_("Możliwa czarna dziura"))
T(10,12,23, 0, "SN?",     N_("Possible SuperNova"), N_("Możliwa supernowa"))
T(10,12,24, 0, "LM?",     N_("Possible Low-mass star"), N_("Możliwa gwiazda o małej masie"))
T(10,12,26, 0, "BD?",     N_("Possible Brown Dwarf"), N_("Możliwy brązowy karzeł"))
T(12, 0, 0, 0, "mul", N_("Composite object"), N_("Obiekt kompozytowy"))
T(12, 1, 0, 0, "reg",   N_("Region defined in the sky"), N_("Obszar zdefiniowany na niebie"))
T(12, 1, 5, 0, "vid",     N_("Underdense region of the Universe"), N_("Mało gęsty obszar wszechświata"))
T(12, 2, 0, 0, "SCG",   N_("Supercluster of Galaxies"), N_("Supergromada galaktyk"))
T(12, 3, 0, 0, "ClG",   N_("Cluster of Galaxies"), N_("Gromada galaktyk"))
T(12, 4, 0, 0, "GrG",   N_("Group of Galaxies"), N_("Grupa galaktyk"))
T(12, 4, 5, 0, "CGG",     N_("Compact Group of Galaxies"), N_("Kompaktowa grupa galaktyk"))
T(12, 5, 0, 0, "PaG",   N_("Pair of Galaxies"), N_("Para galaktyk"))
T(12, 5, 5, 0, "IG" ,     N_("Interacting Galaxies"), N_("Zderzenie galaktyk"))
T(12, 9, 0, 0, "C?*",   N_("Possible Star Cluster"), N_("Możliwa gromada otwarta"))
T(12,10, 0, 0, "Gl?",   N_("Possible Globular Cluster"), N_("Możliwa gromada kulista"))
T(12,11, 0, 0, "Cl*",   N_("Cluster of Stars"), N_("Gromada gwiazd"))
T(12,11, 1, 0, "GlC",     N_("Globular Cluster"), N_("Gromada kulista"))
T(12,11, 2, 0, "OpC",     N_("Open Cluster"), N_("Gromada otwarta"))
T(12,12, 0, 0, "As*",   N_("Association of Stars"), N_("Asocjacja gwiazdowa"))
T(12,12, 1, 0, "St*",     N_("Stellar Stream"), N_("Strumień gwiazd"))
T(12,12, 2, 0, "MGr",     N_("Moving Group"), N_("Poruszająca się grupa"))
T(12,13, 0, 0, "**" ,   N_("Double or multiple star"), N_("Gwiazda podwójna lub wielokrotna"))
T(12,13, 1, 0, "EB*",     N_("Eclipsing binary"), N_("Gwiazda podwójna zaćmieniowa"))
T(12,13, 1, 1, "Al*",       N_("Eclipsing binary of Algol type"), N_("Gwiazda podwójna zaćmieniowa typu Algola"))
T(12,13, 1, 2, "bL*",       N_("Eclipsing binary of beta Lyr type"), N_("Gwiazda podwójna zaćmieniowa typu beta Lyr"))
T(12,13, 1, 3, "WU*",       N_("Eclipsing binary of W UMa type"), N_("Gwiazda podwójna zaćmieniowa typu W UMa"))
T(12,13, 1, 8, "EP*",       N_("Star showing eclipses by its planet"), N_("Gwiazda zaćmiewana przez jej planety"))
T(12,13, 2, 0, "SB*",     N_("Spectroscopic binary"), N_("Układ spektroskopowo podówjny"))
T(12,13, 5, 0, "El*",     N_("Ellipsoidal variable Star"), N_("Gwiazda zmienna w kształcie elipsoidy"))
T(12,13,10, 0, "Sy*",     N_("Symbiotic Star"), N_("Gwiazda w układzie symbiotycznym"))
T(12,13,11, 0, "CV*",     N_("Cataclysmic Variable Star"), N_("Gwiazda w układzie kataklizmicznym"))
T(12,13,11, 2, "DQ*",       N_("CV DQ Her type"), N_("Układ kataklizmiczny typu DQ Her"))
T(12,13,11, 3, "AM*",       N_("CV of AM Her type"), N_("Układ kataklizmiczny typu AM Her"))
T(12,13,11, 5, "NL*",       N_("Nova-like Star"), N_("Gwiazda przypominająca nową"))
T(12,13,11, 6, "No*",       N_("Nova"), N_("Nowa"))
T(12,13,11, 7, "DN*",       N_("Dwarf Nova"), N_("Nowa karłowata"))
T(12,13,12, 0, "XB*",     N_("X-ray Binary"), N_("Rentgenowski układ podwójny"))
T(12,13,12, 2, "LXB",       N_("Low Mass X-ray Binary"), N_("Rentgenowski układ podwójny o niskiej masie"))
T(12,13,12, 3, "HXB",       N_("High Mass X-ray Binary"), N_("Rentgenowski układ podwójny o wysokiej masie"))
T(13, 0, 0, 0, "ISM", N_("Interstellar matter"), N_("Materia międzygwiezdna"))
T(13, 1, 0, 0, "PoC",   N_("Part of Cloud"), N_("Część obłoku"))
T(13, 2, 0, 0, "PN?",   N_("Possible Planetary Nebula"), N_("Możliwa mgławica planetarna"))
T(13, 3, 0, 0, "CGb",   N_("Cometary Globule"), N_("Globula kometarna"))
T(13, 4, 0, 0, "bub",   N_("Bubble"), N_("Bąbel"))
T(13, 6, 0, 0, "EmO",   N_("Emission Object"), N_("Obiekt emisyjny"))
T(13, 8, 0, 0, "Cld",   N_("Interstellar Cloud"), N_("Materia międzygwiezdna"))
T(13, 8, 3, 0, "GNe",     N_("Galactic Nebula"), N_("Mgławica w galaktyce"))
T(13, 8, 4, 0, "BNe",     N_("Bright Nebula"), N_("Jasna mgławica"))
T(13, 8, 6, 0, "DNe",     N_("Dark Cloud"), N_("Ciemna mgławica"))
T(13, 8, 7, 0, "RNe",     N_("Reflection Nebula"), N_("Mgławica refleksyjna"))
T(13, 8,12, 0, "MoC",     N_("Molecular Cloud"), N_("Obłok molekularny"))
T(13, 8,12, 3, "glb",       N_("Globule"), N_("Globula"))
T(13, 8,12, 6, "cor",       N_("Dense core"), N_("Gęsty rdzeń"))
T(13, 8,12, 8, "SFR",       N_("Star forming region"), N_("Obszar formowania się gwiazd"))
T(13, 8,13, 0, "HVC",     N_("High-velocity Cloud"), N_("Obłok o wysokiej prędkości"))
T(13, 9, 0, 0, "HII",   N_("HII region"), N_("Obszar HII"))
T(13,10, 0, 0, "PN" ,   N_("Planetary Nebula"), N_("Mgławica planetarna"))
T(13,11, 0, 0, "sh" ,   N_("HI shell"), N_("Powłoka wodoru HI"))
T(13,12, 0, 0, "SR?",   N_("Possible SuperNova Remnant"), N_("Możliwa pozostałość po supernowej"))
T(13,13, 0, 0, "SNR",   N_("SuperNova Remnant"), N_("Pozostałość po supernowej"))
T(13,14, 0, 0, "cir",   N_("CircumStellar matter"), N_("Materia wokółgwiezdna"))
T(13,14, 1, 0, "of?",     N_("Possible Outflow"), N_("Możliwy wypływ gazu"))
T(13,14,15, 0, "out",     N_("Outflow"), N_("Wypływ gazu"))
T(13,14,16, 0, "HH" ,     N_("Herbig-Haro Object"), N_("Obiekt Herbiga-Haro"))
T(14, 0, 0, 0, "*"  , N_("Star"), N_("Gwiazda"))
T(14, 1, 0, 0, "*iC",   N_("Star in Cluster"), N_("Gwiazda w gromadzie"))
T(14, 2, 0, 0, "*iN",   N_("Star in Nebula"), N_("Gwiazda w mgławicy"))
T(14, 3, 0, 0, "*iA",   N_("Star in Association"), N_("Gwiazda w asocjacji"))
T(14, 4, 0, 0, "*i*",   N_("Star in double system"), N_("Gwiazda w systemie podwójnym"))
T(14, 5, 0, 0, "V*?",   N_("Star suspected of Variability"), N_("Gwiazda podejrzana o zmienność"))
T(14, 6, 0, 0, "Pe*",   N_("Peculiar Star"), N_("Gwiazda typu CP"))
T(14, 6, 1, 0, "HB*",     N_("Horizontal Branch Star"), N_("Gwiazda HB"))
T(14, 6, 2, 0, "Y*O",     N_("Young Stellar Object"), N_("Gwiazda we wczesnym stadium ewolucji"))
T(14, 6, 2, 4, "Ae*",       N_("Herbig Ae/Be star"), N_("Gwiazda typu Herbiga Ae/Be"))
T(14, 6, 5, 0, "Em*",     N_("Emission-line Star"), N_("Gwiazda z liniami emisyjnymi"))
T(14, 6, 5, 3, "Be*",       N_("Be Star"), N_("Gwiazda Be"))
T(14, 6, 6, 0, "BS*",     N_("Blue Straggler Star"), N_("Błękitny maruder"))
T(14, 6,10, 0, "RG*",     N_("Red Giant Branch star"), N_("Czerwony olbrzym"))
T(14, 6,12, 0, "AB*",     N_("Asymptotic Giant Branch Star"), N_("Gwiazda AGB (spalająca hel)"))
T(14, 6,12, 3, "C*" ,       N_("Carbon Star"), N_("Gwiazda węglowa"))
T(14, 6,12, 6, "S*" ,       N_("S Star"), N_("Gwiazda typu widmowego S"))
T(14, 6,13, 0, "sg*",     N_("Evolved supergiant star"), N_("Wyewoluowany nadolbrzym"))
T(14, 6,13, 3, "s*r",       N_("Red supergiant star"), N_("Czerwony nadolbrzym"))
T(14, 6,13, 4, "s*y",       N_("Yellow supergiant star"), N_("Żółty nadolbrzym"))
T(14, 6,13, 5, "s*b",       N_("Blue supergiant star"), N_("Niebieski nadolbrzym"))
T(14, 6,14, 0, "HS*",     N_("Hot subdwarf"), N_("Gorący podkarzeł"))
T(14, 6,15, 0, "pA*",     N_("Post-AGB Star"), N_("Gwiazda po stadium AGB (mgławica protoplanetarna)"))
T(14, 6,16, 0, "WD*",     N_("White Dwarf"), N_("Biały karzeł"))
T(14, 6,16, 1, "ZZ*",       N_("Pulsating White Dwarf"), N_("Pulsujący biały karzeł"))
T(14, 6,17, 0, "LM*",     N_("Low-mass star"), N_("Gwiazda o małej masie"))
T(14, 6,18, 0, "BD*",     N_("Brown Dwarf"), N_("Brązowy karzeł"))
T(14, 6,19, 0, "N*" ,     N_("Confirmed Neutron Star"), N_("Gwiazda neutronowa"))
T(14, 6,23, 0, "OH*",     N_("OH/IR star"), N_("Gwiazda OH/IR"))
T(14, 6,24, 0, "CH*",     N_("Star with envelope of CH type"), N_("Gwiazda z otoczką typu CH"))
T(14, 6,25, 0, "pr*",     N_("Pre-main sequence Star"), N_("Gwiazda przed ciągiem głównym"))
T(14, 6,25, 3, "TT*",       N_("T Tau-type Star"), N_("Gwiazda typu T Tau"))
T(14, 6,30, 0, "WR*",     N_("Wolf-Rayet Star"), N_("Gwiazda Wolfa-Rayeta"))
T(14, 7, 0, 0, "PM*",   N_("High proper-motion Star"), N_("Gwiazda z dużym ruchem własnym"))
T(14, 8, 0, 0, "HV*",   N_("High-velocity Star"), N_("Gwiazda o wysokiej prędkości"))
T(14, 9, 0, 0, "V*" ,   N_("Variable Star"), N_("Gwiazda zmienna"))
T(14, 9, 1, 0, "Ir*",     N_("Variable Star of irregular type"), N_("Gwiazda zmienna nieregularnego typu"))
T(14, 9, 1, 1, "Or*",       N_("Variable Star of Orion Type"), N_("Gwiazda zmienna typu Oriona"))
T(14, 9, 1, 2, "RI*",       N_("Variable Star with rapid variations"), N_("Gwiazda zmienna z szybkimi zmianami"))
T(14, 9, 3, 0, "Er*",     N_("Eruptive variable Star"), N_("Erupcyjna gwiazda zmienna"))
T(14, 9, 3, 1, "Fl*",       N_("Flare Star"), N_("Gwiazda rozbłyskowa"))
T(14, 9, 3, 2, "FU*",       N_("Variable Star of FU Ori type"), N_("Gwiazda zmienna typu FU Ori"))
T(14, 9, 3, 4, "RC*",       N_("Variable Star of R CrB type"), N_("Gwiazda zmienna typu R CrB"))
T(14, 9, 3, 5, "RC?",       N_("Variable Star of R CrB type candiate"), N_("Możliwa gwiazda zmienna typu R CrB"))
T(14, 9, 4, 0, "Ro*",     N_("Rotationally variable Star"), N_("Gwiazda zmienna rotacyjna"))
T(14, 9, 4, 1, "a2*",       N_("Variable Star of alpha2 CVn type"), N_("Gwiazda zmienna typu alpha2 CVn"))
T(14, 9, 4, 3, "Psr",       N_("Pulsar"), N_("Pulsar"))
T(14, 9, 4, 4, "BY*",       N_("Variable of BY Dra type"), N_("Gwiazda zmienna typu BY Dra"))
T(14, 9, 4, 5, "RS*",       N_("Variable of RS CVn type"), N_("Gwiazda zmienna typu RS CVn"))
T(14, 9, 5, 0, "Pu*",     N_("Pulsating variable Star"), N_("Pulsująca gwiazda zmienna"))
T(14, 9, 5, 2, "RR*",       N_("Variable Star of RR Lyr type"), N_("Gwiazda zmienna typu RR Lyr"))
T(14, 9, 5, 3, "Ce*",       N_("Cepheid variable Star"), N_("Cefeida"))
T(14, 9, 5, 5, "dS*",       N_("Variable Star of delta Sct type"), N_("Gwiazda zmienna typu delta Sct"))
T(14, 9, 5, 6, "RV*",       N_("Variable Star of RV Tau type"), N_("Gwiazda zmienna typu RV Tau"))
T(14, 9, 5, 7, "WV*",       N_("Variable Star of W Vir type"), N_("Gwiazda zmienna typu W Vir"))
T(14, 9, 5, 8, "bC*",       N_("Variable Star of beta Cep type"), N_("Gwiazda zmienna typu beta Cep"))
T(14, 9, 5, 9, "cC*",       N_("Classical Cepheid"), N_("Klasyczna Cefeida"))
T(14, 9, 5,10, "gD*",       N_("Variable Star of gamma Dor type"), N_("Gwiazda zmienna typu gamma Dor"))
T(14, 9, 5,11, "SX*",       N_("Variable Star of SX Phe type"), N_("Gwiazda zmienna typu SX Phe"))
T(14, 9, 6, 0, "LP*",     N_("Long-period variable star"), N_("Długookresowa gwiazda zmienna"))
T(14, 9, 6, 1, "Mi*",       N_("Variable Star of Mira Cet type"), N_("Gwiazda zmienna typu Mira Cet"))
T(14, 9, 6, 4, "sr*",       N_("Semi-regular pulsating Star"), N_("Pół-regularna gwiazda pulsująca"))
T(14, 9, 8, 0, "SN*",     N_("SuperNova"), N_("Supernowa"))
T(14,14, 0, 0, "su*",   N_("Sub-stellar object"), N_("Obiekt sub-gwiezdny"))
T(14,14, 2, 0, "Pl?",     N_("Possible Extra-solar Planet"), N_("Możliwa egzoplaneta"))
T(14,14,10, 0, "Pl" ,     N_("Extra-solar Confirmed Planet"), N_("Egzoplaneta"))
T(15, 0, 0, 0, "G"  , N_("Galaxy"), N_("Galaktyka"))
T(15, 1, 0, 0, "PoG",   N_("Part of a Galaxy"), N_("Część galaktyki"))
T(15, 2, 0, 0, "GiC",   N_("Galaxy in Cluster of Galaxies"), N_("Galaktyka w gromadzie galaktyk"))
T(15, 2, 2, 0, "BiC",     N_("Brightest Galaxy in a Cluster"), N_("Najjaśniejsca galaktyka w gromadzie"))
T(15, 3, 0, 0, "GiG",   N_("Galaxy in Group of Galaxies"), N_("Galaktyka w grupie galaktyk"))
T(15, 4, 0, 0, "GiP",   N_("Galaxy in Pair of Galaxies"), N_("Galaktyka w parze galaktyk"))
T(15, 5, 0, 0, "HzG",   N_("Galaxy with high redshift"), N_("Galaktyka z wysokim przesunięciem ku czerwieni"))
T(15, 6, 0, 0, "ALS",   N_("Absorption Line system"), N_("Kwazar z systemem linii absorpcyjnych"))
T(15, 6, 1, 0, "LyA",     N_("Ly alpha Absorption Line system"), N_("System z liniami absorpcyjnymi Ly alpha"))
T(15, 6, 2, 0, "DLA",     N_("Damped Ly-alpha Absorption Line system"), N_("Kwazar z widocznymi liniami absorpcyjnymi neutralnego wodoru Ly-alpha"))
T(15, 6, 3, 0, "mAL",     N_("metallic Absorption Line system"), N_("Układ z metalicznymi liniami absorpcyjnymi"))
T(15, 6, 5, 0, "LLS",     N_("Lyman limit system"), N_("Kwazar z limitem Lymana"))
T(15, 6, 8, 0, "BAL",     N_("Broad Absorption Line system"), N_("Kwazar z systemem szerokich linii absorpcyjnych"))
T(15, 7, 0, 0, "rG" ,   N_("Radio Galaxy"), N_("Radiogalaktyka"))
T(15, 8, 0, 0, "H2G",   N_("HII Galaxy"), N_("Galaktyka z obszarami HII"))
T(15, 9, 0, 0, "LSB",   N_("Low Surface Brightness Galaxy"), N_("Galaktyka o niskiej jasności powierzchniowej"))
T(15,10, 0, 0, "AG?",   N_("Possible Active Galaxy Nucleus"), N_("Możliwe aktywne jądro galaktyki"))
T(15,10, 7, 0, "Q?" ,     N_("Possible Quasar"), N_("Możliwy kwazar"))
T(15,10,11, 0, "Bz?",     N_("Possible Blazar"), N_("Możliwy blazar"))
T(15,10,17, 0, "BL?",     N_("Possible BL Lac"), N_("Możliwa Lacertyda"))
T(15,11, 0, 0, "EmG",   N_("Emission-line Galaxy"), N_("Galaktyka z liniami emisyjnymi"))
T(15,12, 0, 0, "SBG",   N_("Starburst Galaxy"), N_("Galaktyka gwiazdotwórcza"))
T(15,13, 0, 0, "bCG",   N_("Blue compact Galaxy"), N_("Kompaktowa niebieska galaktyka karłowata"))
T(15,14, 0, 0, "LeI",   N_("Gravitationally Lensed Image"), N_("Obraz powstały w wyniku soczewkowania grawitacyjnego"))
T(15,14, 1, 0, "LeG",     N_("Gravitationally Lensed Image of a Galaxy"), N_("Soczewkowany grawitacyjnie obraz galaktyki"))
T(15,14, 7, 0, "LeQ",     N_("Gravitationally Lensed Image of a Quasar"), N_("Soczewkowany grawitacyjnie obraz kwazara"))
T(15,15, 0, 0, "AGN",   N_("Active Galaxy Nucleus"), N_("Aktywne jądro galaktyki"))
T(15,15, 1, 0, "LIN",     N_("LINER-type Active Galaxy Nucleus"), N_("Aktywne jądro galaktyki typu LINER"))
T(15,15, 2, 0, "SyG",     N_("Seyfert Galaxy"), N_("Galaktyka Seyferta"))
T(15,15, 2, 1, "Sy1",       N_("Seyfert 1 Galaxy"), N_("Galaktyka typu Seyfert 1"))
T(15,15, 2, 2, "Sy2",       N_("Seyfert 2 Galaxy"), N_("Galaktyka typu Seyfert 2"))
T(15,15, 3, 0, "Bla",     N_("Blazar"), N_("Blazar"))
T(15,15, 3, 1, "BLL",       N_("BL Lac - type object"), N_("Lacertyda"))
T(15,15, 3, 2, "OVV",       N_("Optically Violently Variable object"), N_("Kwazar szybko zmienny optycznie"))
T(15,15, 4, 0, "QSO",     N_("Quasar"), N_("Kwazar"))

// Extra fields for Solar system objects.
T(16, 0, 0, 0, "SSO", N_("Solar System Object"), N_("Obiekt Układu Słonecznego"))
T(16, 1, 0, 0, "Sun",   N_("Sun"), N_("Słońce"))
T(16, 2, 0, 0, "Pla",   N_("Planet"), N_("Planeta"))
T(16, 3, 0, 0, "Moo",   N_("Moon"), N_("Księżyc"))
T(16, 4, 0, 0, "Asa",   N_("Artificial Satellite"), N_("Sztuczny satelita"))
T(16, 5, 0, 0, "MPl",   N_("Minor Planet"), N_("Planetoida"))
T(16, 5, 1, 0, "DPl",   N_("Dwarf Planet"), N_("Planeta karłowata"))
T(16, 5, 2, 0, "Com",   N_("Comet"), N_("Kometa"))
T(16, 5, 2, 1, "PCo",     N_("Periodic Comet"), N_("Kometa okresowa"))
T(16, 5, 2, 2, "CCo",     N_("Non Periodic Comet"), N_("Kometa nieokresowa"))
T(16, 5, 2, 3, "XCo",     N_("Unreliable (Historical) Comet"), N_("Niewiarygodna (historyczna) kometa"))
T(16, 5, 2, 4, "DCo",     N_("Disappeared Comet"), N_("Rozpadnięta kometa"))
T(16, 5, 2, 5, "ACo",     N_("Minor Planet"), N_("Planetoida"))
T(16, 5, 2, 6, "ISt",     N_("Interstellar Object"), N_("Obiekt międzygwiezdny"))
T(16, 5, 3, 0, "NEO",   N_("Near Earth Object"), N_("Obiekt bliski Ziemi (NEO)"))
T(16, 5, 3, 1, "Ati",     N_("Atira Asteroid"), N_("Planetoida z grupy Atiry"))
T(16, 5, 3, 2, "Ate",     N_("Aten Asteroid"), N_("Planetoida z grupy Atena"))
T(16, 5, 3, 3, "Apo",     N_("Apollo Asteroid"), N_("Planetoida z grupy Apolla"))
T(16, 5, 3, 4, "Amo",     N_("Amor Asteroid"), N_("Planetoida z grupy Amora"))
T(16, 5, 4, 0, "Hun",   N_("Hungaria Asteroid"), N_("Planetoida z grupy Hungaria"))
T(16, 5, 5, 0, "Pho",   N_("Phocaea Asteroid"), N_("Planetoida z grupy Phocaea"))
T(16, 5, 6, 0, "Hil",   N_("Hilda Asteroid"), N_("Planetoida z grupy Hilda"))
T(16, 5, 7, 0, "JTA",   N_("Jupiter Trojan Asteroid"), N_("Planetoida trojański"))
T(16, 5, 8, 0, "DOA",   N_("Distant Object Asteroid"), N_("Odległa asteroida"))
T(16, 5, 9, 0, "MBA",   N_("Main Belt Asteroid"), N_("Planetoida z głównego pasa planetoid"))
T(16, 6, 0, 0, "IPS",   N_("Interplanetary Spacecraft"), N_("Międzyplanetarny statek kosmiczny"))

// Extra fields for Cultural Sky Representation
T(17, 0, 0, 0, "Cul", N_("Cultural Sky Representation"), N_("Kulturowa reprezentacja nieba"))
T(17, 1, 0, 0, "Con",   N_("Constellation"), N_("Gwiazdozbiór"))
T(17, 2, 0, 0, "Ast",   N_("Asterism"), N_("Asteryzm"))

{}
};
