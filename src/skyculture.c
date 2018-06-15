/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static const char *NAMES;
static const char *CONSTELLATIONS;

typedef struct star_name star_name_t;
struct star_name
{
    UT_hash_handle  hh;
    int             hd;
    char            *name;
};

typedef struct constellation
{
    char id[8];
    char name[128];
    int  lines[64][2];
    int  nb_lines;
} constellation_t;

struct skyculture
{
    star_name_t     *star_names;
    int             nb_constellations;
    constellation_t *constellations;
};

static void parse_names(skyculture_t *cult, const char *names)
{
    char *data, *line, *tmp = NULL;
    star_name_t *star_name;

    data = strdup(names);
    for (line = strtok_r(data, "\n", &tmp); line;
          line = strtok_r(NULL, "\n", &tmp)) {
        star_name = calloc(1, sizeof(*star_name));
        sscanf(strtok(line, " "), "%d", &star_name->hd);
        star_name->name = strdup(strtok(NULL, "\n"));
        HASH_ADD_INT(cult->star_names, hd, star_name);
    }
    free(data);
}

static void trim_right_spaces(char *s)
{
    int l = strlen(s);
    while (s[l - 1] == ' ') {
        s[l - 1] = '\0';
        l--;
    }
}

static int parse_star(const char *cst, const char *tok)
{
    int hd;
    char bayer_id[32];
    if (tok[0] >= '0' && tok[1] <= '9') { // A HD number
        sscanf(tok, "%d", &hd);
        return hd;
    }
    // Otherwise a bayer id.
    sprintf(bayer_id, "%s %s", tok, cst);
    tok = (char*)identifiers_search(bayer_id);
    assert(tok && str_startswith(tok, "HD "));
    tok += 3;
    sscanf(tok, "%d", &hd);
    return hd;
}

static void parse_constellations(skyculture_t *cult, const char *consts)
{
    char *data, *line, *tmp = NULL, *tok;
    bool linked;
    int nb = 0, i, star, last_star = 0;
    constellation_t *cons;

    data = strdup(consts);

    // Count the number of lines in the file.
    for (line = data; *line; line = strchr(line, '\n') + 1) nb++;
    assert(nb);

    cult->nb_constellations = nb;
    cult->constellations = calloc(nb, sizeof(constellation_t));

    for (i = 0, line = strtok_r(data, "\n", &tmp); line;
          line = strtok_r(NULL, "\n", &tmp), i++) {
        cons = &cult->constellations[i];
        strcpy(cons->id, strtok(line, "|"));
        strcpy(cons->name, strtok(NULL, "|"));
        trim_right_spaces(cons->name);
        nb = 0;
        while ((tok = strtok(NULL, " -"))) {
            // Check if the last separator was a '-'.
            linked = consts[tok - 1 - data] == '-';
            if (linked) assert(last_star);
            star = parse_star(cons->id, tok);
            if (linked) {
                cons->lines[nb][0] = last_star;
                cons->lines[nb][1] = star;
                nb++;
            }
            last_star = star;
        }
        cons->nb_lines = nb;
    }
    free(data);
}

static void print_constellation(int index, const char *id, const char *name,
                                int nb_lines, int (*lines)[2], void *user)
{
    int i;
    char cst[4];
    int bayer, n, hd;
    int last_hd = 0;
    const char *g;
    static const char *GREEK =
        "alf bet gam del eps zet eta tet iot kap lam mu  nu  xi  omi "
        "pi  rho sig tau ups phi chi psi ome";
    printf("%s|%-22s|", id, name);
    for (i = 0; i < nb_lines * 2; i++) {
        hd = lines[i / 2][i % 2];
        if (i) {
            if (last_hd == hd) continue;
            printf("%c", i % 2 == 0 ? ' ' : '-');
        }
        bayer_get(hd, cst, &bayer, &n);
        g = &GREEK[(bayer - 1) * 4];
        if (bayer && strcmp(cst, id) == 0) {
            printf("%.*s%.*d", g[2] == ' ' ? 2 : 3, g, n ? 1 : 0, n);
        } else {
            printf("%d", hd);
        }
        last_hd = hd;
    }
    printf("\n");
}

skyculture_t *skyculture_create(void)
{
    char id[64];
    star_name_t *star_name, *tmp;
    skyculture_t *cult = calloc(1, sizeof(*cult));
    parse_names(cult, NAMES);
    parse_constellations(cult, CONSTELLATIONS);

    // Register all the names.
    HASH_ITER(hh, cult->star_names, star_name, tmp) {
        sprintf(id, "HD %d", star_name->hd);
        identifiers_add(id, "NAME", star_name->name, NULL, NULL);
    }

    // XXX: I keep this around for debuging.
    if (0)
        skyculture_iter_constellations(cult, print_constellation, NULL);

    return cult;
}

const char *skyculture_get_star_name(const skyculture_t *cult, int hd)
{
    star_name_t *star_name;
    HASH_FIND_INT(cult->star_names, &hd, star_name);
    return star_name ? star_name->name : NULL;
}

int skyculture_search_star_name(const skyculture_t *cult, const char *name)
{
    star_name_t *star_name, *tmp;
    HASH_ITER(hh, cult->star_names, star_name, tmp) {
        if (strcasecmp(star_name->name, name) == 0) {
            return star_name->hd;
        }
    }
    return 0;
}

void skyculture_iter_constellations(const skyculture_t *cult,
                    void (*f)(int index, const char *id, const char *name,
                              int nb_lines, int (*lines)[2], void *user),
                    void *user)
{
    int i;
    constellation_t *cons;
    for (i = 0; i < cult->nb_constellations; i++) {
        cons = &cult->constellations[i];
        f(i, cons->id, cons->name, cons->nb_lines, cons->lines, user);
    }
}

static const char *NAMES =
    "358 Alpheratz\n"
    "432 Caph\n"
    "886 Algenib\n"
    "2261 Ankaa\n"
    "3712 Shedir\n"
    "4128 Diphda\n"
    // "3829 Van Maanen 2\n" // No HD!
    "6860 Mirach\n"
    "10144 Achernar\n"
    "12533 Almaak\n"
    "12929 Hamal\n"
    "14386 Mira\n"
    "8890 Polaris\n"
    "18622 Acamar\n"
    "18884 Menkar\n"
    "19356 Algol\n"
    "20902 Mirphak\n"
    "23630 Alcyone\n"
    "23862 Pleione\n"
    "25025 Zaurak\n"
    "29139 Aldebaran\n"
    "33793 Kapteyn's star\n"
    "34085 Rigel\n"
    "34029 Capella\n"
    "35468 Bellatrix\n"
    "35497 Alnath\n"
    "36079 Nihal\n"
    "36486 Mintaka\n"
    "36673 Arneb\n"
    "37128 Alnilam\n"
    "37742 Alnitak\n"
    "38771 Saiph\n"
    "39801 Betelgeuse\n"
    "44179 Red Rectangle\n"
    "45348 Canopus\n"
    "47105 Alhena\n"
    "48915 Sirius\n"
    "52089 Adhara\n"
    // "36208 Luyten's star\n" No HD!
    "60179 Castor\n"
    "61421 Procyon\n"
    "62509 Pollux\n"
    "81797 Alphard\n"
    "87901 Regulus\n"
    "89484 Algieba\n"
    "95418 Merak\n"
    "95689 Dubhe\n"
    "102647 Denebola\n"
    "103095 Groombridge 1830\n"
    "103287 Phad\n"
    "106591 Megrez\n"
    "108248 Acrux\n"
    // "60936 3C 273\n" No HD !
    "112185 Alioth\n"
    "112413 Cor Caroli\n"
    "113226 Vindemiatrix\n"
    "116656 Mizar\n"
    "116658 Spica\n"
    "116842 Alcor\n"
    "120315 Alkaid\n"
    "122451 Agena\n"
    "122451 Hadar\n"
    "123299 Thuban\n"
    "124897 Arcturus\n"
    // "70890 Proxima\n" No HD !
    "128620 Rigil Kent\n"
    "129988 Izar\n"
    "131873 Kocab\n"
    "139006 Alphekka\n"
    "140573 Unukalhai\n"
    "148478 Antares\n"
    "156014 Rasalgethi\n"
    "158926 Shaula\n"
    "159561 Rasalhague\n"
    "164058 Etamin\n"
    // "87937 Barnard's star\n" No HD !
    "169022 Kaus Australis\n"
    "172167 Vega\n"
    "174638 Sheliak\n"
    "175191 Nunki\n"
    "183912 Albireo\n"
    "184738 Campbell's star\n"
    "186791 Tarazed\n"
    "187642 Altair\n"
    "188512 Alshain\n"
    "226868 Cyg X-1\n"
    "197345 Deneb\n"
    "203280 Alderamin\n"
    "206778 Enif\n"
    "209750 Sadalmelik\n"
    "209952 Alnair\n"
    "239960 Kruger 60\n"
    "215441 Babcock's star\n"
    "216956 Fomalhaut\n"
    "217906 Scheat\n"
    "218045 Markab\n"
;

// STYLE-CHECK OFF

// Western skyculture constellations.
static const char *CONSTELLATIONS =
    "Aql|Aquila                |bet-alf-gam alf-del-eta tet-eta del-zet-eps del-lam\n"
    "And|Andromeda             |alf-del-bet gam1-bet-mu-nu\n"
    "Scl|Sculptor              |bet-alf-gam-bet\n"
    "Ara|Ara                   |tet-alf-zet-eta-del-gam-bet-tet\n"
    "Lib|Libra                 |tet-gam-bet-alf2-sig-gam\n"
    "Cet|Cetus                 |xi1-xi2 tau-bet-iot bet-eta-tet-zet-rho-eps-pi-sig-tau 14386-eps 14386-del-gam-alf-lam-mu-xi2-nu-gam\n"
    "Ari|Aries                 |17573-alf-bet-gam2\n"
    "Sct|Scutum                |bet-173819-175156-gam-alf-bet\n"
    "Pyx|Pyxis                 |bet-alf-gam\n"
    "Boo|Bootes                |zet-alf-eps-del-bet-gam-rho-alf-eta-ups\n"
    "Cae|Caelum                |del-alf-bet\n"
    "Cha|Chamaeleon            |alf-gam-bet\n"
    "Cnc|Cancer                |iot-gam-chi gam-del-bet del-alf\n"
    "Cap|Capricornus           |alf2-bet-tet-iot-gam-del iot-zet-tet bet-psi tet-ome\n"
    "Car|Carina                |bet-ome-tet-93070-96918-94510-90853-89388-iot 74375-eps-alf 79351-iot 79351-74375 alf-47670 eps-66811\n"
    "Cas|Cassiopeia            |eps-del-gam-alf-bet\n"
    "Cen|Centaurus             |alf1-bet-eps-zet-ups1-mu-nu-117440-iot nu-tet mu-eta-kap zet-gam-sig-del-100673-lam\n"
    "Cep|Cepheus               |zet-iot-bet-alf-zet iot-gam-bet\n"
    "Com|Coma Berenices        |alf-bet-gam\n"
    "CVn|Canes Venatici        |bet-alf2\n"
    "Aur|Auriga                |tet-bet-alf-zet-iot 35497-iot 35497-tet\n"
    "Col|Columba               |del-kap-gam-bet-eta bet-alf-eps\n"
    "Cir|Circinus              |alf-gam alf-bet\n"
    "Crt|Crater                |alf-bet-gam-del-alf del-eps-tet-eta-zet-gam\n"
    "CrA|Corona Australis      |lam-175362-eps-gam-alf-bet-del-zet-175219 lam-170642\n"
    "CrB|Corona Borealis       |tet-bet-alf-gam-del-eps-iot\n"
    "Crv|Corvus                |eta-del-gam-eps-alf eps-bet-del\n"
    "Cru|Crux                  |gam-alf1 bet-del\n"
    "Cyg|Cygnus                |kap-iot2-del-gam-alf gam-eps-zet-mu1 gam-eta-bet1\n"
    "Del|Delphinus             |eps-bet-alf-gam2-del-bet\n"
    "Dor|Dorado                |del-40409-bet-del bet-alf-gam\n"
    "Dra|Draco                 |xi-gam-bet-nu2-xi-del-eps-chi-zet-eta-tet-iot-alf-kap-lam\n"
    "Nor|Norma                 |kap-gam2-eps-eta-gam2 eta-kap\n"
    "Eri|Eridanus              |alf-chi-phi-kap-16754-iot-tet1-20794-24071-24160-ups4-28028-ups2-tau6-tau5-tau4-tau3-tau1-eta-zet-eps-del-nu-mu-ome-bet-lam-29503\n"
    "Sge|Sagitta               |bet-del-alf del-gam-eta\n"
    "For|Fornax                |bet-alf\n"
    "Gem|Gemini                |gam-zet-del-lam-xi del-ups-kap ups-bet ups-iot-tau-alf tau-tet tau-eps-nu eps-mu-eta-41116\n"
    "Cam|Camelopardalis        |21291-24479-alf 21291-gam-alf gam-33564\n"
    "CMa|Canis Major           |tet-gam-iot-alf-omi2-del-ome-eta eps-sig-del sig-50896-nu2-xi2 nu2-bet nu2-alf eps-kap zet-eps iot-tet\n"
    "UMa|Ursa Major            |eta-zet-eps-del-alf-bet-gam-del gam-chi-psi-lam psi-mu bet-phi-tet-kap tet-iot phi-ups-omi-81937-alf\n"
    "Gru|Grus                  |tet-del1-alf-bet-iot-tet bet-zet bet-eps alf-lam-gam\n"
    "Her|Hercules              |iot-tet-rho-156729-pi-eta-sig-tau-chi eta-zet-bet-gam zet-eps-lam-del mu-xi-omi xi-mu eps-pi\n"
    "Hor|Horologium            |alf-zet-mu\n"
    "Hya|Hydra                 |eta-sig-del-eps-rho-eta rho-zet-tet-tau2-tau1-alf-ups1-lam-mu-nu-xi-bet-psi-gam\n"
    "Hyi|Hydrus                |bet-gam-eps-del-alf\n"
    "Ind|Indus                 |tet-alf-bet-tet\n"
    "Lac|Lacerta               |211388-213420-213310-212593-bet-alf-213310\n"
    "Mon|Monoceros             |gam-bet-del-eps-42111 del-zet-alf\n"
    "Lep|Lepus                 |tet-eta-zet-alf-mu alf-del-gam-bet-eps alf-bet mu-lam mu-kap eps-mu kap-iot lam-nu\n"
    "Leo|Leo                   |bet-tet-alf-eta-gam1-del-bet gam1-zet-mu-eps del-tet\n"
    "Lup|Lupus                 |chi-144415-eta-chi eta-gam-del-phi1 del-bet gam-ome-zet-alf zet-rho alf-tau2 alf-bet\n"
    "Lyn|Lynx                  |alf-80081-77912-76943-70272-58142-50522-43378\n"
    "Lyr|Lyra                  |alf-zet1-bet-gam-del2-zet1\n"
    "Ant|Antlia                |alf-eta\n"
    "Mic|Microscopium          |eps-gam-alf\n"
    "Mus|Musca                 |bet-lam-gam-alf-bet\n"
    "Oct|Octans                |nu-bet-del-nu\n"
    "Aps|Apus                  |alf-gam-bet\n"
    "Oph|Ophiuchus             |alf-bet eta-bet alf-kap-eps-zet-eta-158643\n"
    "Ori|Orion                 |zet-eps-del 41040-xi-nu-chi1 xi-mu-alf-zet-kap-bet-del-gam-lam-alf gam-pi3-pi4-31139-pi6 pi3-pi2-pi1 nu-mu\n"
    "Pav|Pavo                  |alf-gam-bet-del-alf del-eps-zet-kap-del kap-lam-xi-pi-lam pi-eta\n"
    "Peg|Pegasus               |gam-alf bet-eta-pi1 bet-mu-lam-iot-kap alf-xi-zet-tet-eps 358-bet 358-gam bet-alf\n"
    "Pic|Pictor                |alf-gam-bet\n"
    "Per|Perseus               |omi-zet-xi-eps-del-alf-gam-eta alf-bet-rho-17584\n"
    "Equ|Equuleus              |gam-del-bet-alf-gam\n"
    "CMi|Canis Minor           |alf-bet\n"
    "LMi|Leo Minor             |94264-bet-87696-82635 87696-94264\n"
    "Vul|Vulpecula             |alf-189849\n"
    "UMi|Ursa Minor            |alf-del-eps-zet-eta-gam-bet-zet\n"
    "Phe|Phoenix               |zet-bet-kap-zet bet-del-psi-bet-gam-kap-alf-eps-kap\n"
    "Psc|Pisces                |sig-phi sig-ups-phi-eta-omi-alf-xi-nu-mu-eps-4627-1635-ome-iot-lam-kap-gam-tet-iot\n"
    "PsA|Piscis Austrinus      |alf-eps-eta-tet-tau-bet-del\n"
    "Vol|Volans                |zet-gam2-eps-zet eps-del eps-bet-alf-eps\n"
    "Pup|Puppis                |rho-63660-pi-nu-tau-sig-zet-rho\n"
    "Ret|Reticulum             |alf-eps-del-bet-alf\n"
    "Sgr|Sagittarius           |del-lam eta-eps-gam2-161592 gam2-del-eps-zet-phi-del phi-lam-mu zet-tau-sig-phi sig-xi2-omi-180540-rho1 tau-184552-189763-tet1-iot-alf iot-bet2\n"
    "Sco|Scorpius              |lam-kap-iot1-tet-eta-zet1-mu1-eps-tau-alf-del alf-pi alf-bet1\n"
    "Ser|Serpens               |mu-eps-alf-del-bet-gam-kap-bet tet1-eta-omi-xi-nu\n"
    "Sex|Sextans               |bet-alf\n"
    "Men|Mensa                 |gam-mu\n"
    "Tau|Taurus                |bet-tau-eps alf-zet gam-del1 gam-lam-omi alf-eps alf-tet2-gam eps-del3-del1-23850\n"
    "Tel|Telescopium           |zet-alf\n"
    "Tuc|Tucana                |alf-gam-zet gam-bet1\n"
    "Tri|Triangulum            |13869-bet-alf-13869\n"
    "TrA|Triangulum Australe   |alf-gam-bet-alf\n"
    "Aqr|Aquarius              |bet-alf-gam-zet1-eta-lam-psi1-220321 alf-tet-iot tet-sig-tau2-del-218594 eps-bet\n"
    "Vir|Virgo                 |nu-107070-gam-alf-kap-iot-mu alf-zet-tau-130109 zet-del-eps del-gam\n"
    "Vel|Vela                  |gam2-74195-del-kap-phi-mu-92139-88955-psi-lam-gam2\n"
;
