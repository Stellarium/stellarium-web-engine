/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static const catalog_t HIP_CAT[] = {
    {  9,  14, "I6",    "HIP"},
    { 42,  46, "F5.2",  "vmag", '?'},
    { 52,  63, "F12.8", "RAdeg", '?'},
    { 65,  76, "F12.8", "DEdeg", '?'},
    { 80,  86, "F7.2",  "Plx",   '?'},
    { 88,  95, "F8.2",  "pmRA",  '?'},
    { 97, 104, "F8.2",  "pmDE",  '?'},
    {246, 251, "F6.3",  "B-V",   '?'},
    {391, 396, "I6",    "HD",    '?', .default_i = -1},
    {436, 447, "A12",   "SpType"},
    {0}
};

// http://cdsarc.u-strasbg.fr/ftp/cats/v/50/ReadMe
static const catalog_t BSC_CAT[] = {
    { 26,  31, "I6",     "hd", '?', .default_i = -1},
    { 76,  77, "I2",     "ra_hour", '?', .default_i = -1},
    { 78,  79, "I2",     "ra_min", '?'},
    { 80,  83, "F4.1",   "ra_sec", '?'},
    { 84,  84, "A1",     "de_s", '?'},
    { 85,  86, "I2",     "de_deg", '?', .default_i = -1},
    { 87,  88, "I2",     "de_min", '?'},
    { 89,  90, "I2",     "de_sec", '?'},
    {103, 107, "F5.2",   "vmag", '?', .default_f = NAN},
    {110, 114, "F5.2",   "bv", '?'},
    {162, 166, "F5.3",   "plx", '?'},
    {130, 130, "A1",     "sp"},
    {0}
};

// ftp://cdsarc.u-strasbg.fr/cats/I/259/ReadMe
static const catalog_t TYCHO2_CAT[] = {
    {1,     4, "I4"       "TYC1"},
    {6,    10, "I5"       "TYC2"},
    {12,   12, "I1  "     "TYC3"},
    {16,   27, "F12.8",   "RAmdeg", '?'},
    {29,   40, "F12.8",   "DEmdeg", '?'},
    {42,   48, "F7.1",    "pmRA",   '?'},
    {50,   56, "F7.1",    "pmDE", '?'},
    {111, 116, "F6.3",    "BTmag", '?'},
    {124, 129, "F6.3",    "VTmag", '?'},
    {141, 141, "A1",      "TYC"},
    {143, 148, "I6",      "HIP", '?', .default_i = -1},
    {0}
};

// http://cdsarc.u-strasbg.fr/viz-bin/Cat?I/297
static const catalog_t NOMAD_CAT[] = {
    {1,    12, "A12",   "NOMAD1"},
    {36,   47, "A12",   "Tycho-2", '?'}, // Or HIP
    {52,   62, "F11.7", "RAdeg"}, // ICRS, Ep=J2000
    {63,   73, "F11.7", "DEdeg"}, // ICRS, Ep=J2000
    {87,   94, "F8.1",  "pmRA"},
    {96,  103, "F8.1",  "pmDE"},
    {125, 130, "F6.3",  "Vmag", '?'},
    {0}
};

static void dump_entry(int i, ...)
{
    va_list ap;
    const char *name, *fmt;
    char buf[64];
    int j;

    if (i) printf(",\n");
    printf("{ ");
    va_start(ap, i);
    for (j = 0; true; j++) {
        name = va_arg(ap, char*);
        if (!name) break;
        fmt = va_arg(ap, char*);
        vsprintf(buf, fmt, ap);
        if (!name[0]) continue;
        if (j) printf(", ");
        printf("\"%s\": %s", name, buf);
    }
    printf("}");
    va_end(ap);
}

static int dump_hip(const char *data)
{
    int i, hip, hd;
    double vmag, ra, de, bv, plx;
    printf("[");
    CATALOG_ITER(HIP_CAT, data, i, &hip, &vmag, &ra, &de, &plx, NULL, NULL,
                 &bv, &hd, NULL) {
        dump_entry(i,
                   "type", "%s", "star",
                   "catalog_source", "%s", "Hipparcos",
                   "hip", "%d",   hip,
                   (hd != -1) ? "hd" : "",  "%d",   hd,
                   "vmag","%.2f", vmag,
                   "ra",  "%.8f", ra,
                   "de",  "%.8f", de,
                   "plx", "%.2f", plx,
                   "bv",  "%.3f", bv,
                   NULL);
    }
    printf("]\n");
    return 0;
}

static int dump_bsc(const char *data)
{
    char sp, de_s;
    int i, hd, ra_hour, ra_min, de_deg, de_min, de_sec, err;
    double vmag, plx, ra_sec, bv, ra, de;
    (void)err;
    printf("[");
    CATALOG_ITER(BSC_CAT, data, i, &hd, &ra_hour, &ra_min, &ra_sec,
                          &de_s, &de_deg, &de_min, &de_sec, &vmag, &bv,
                          &plx, &sp) {
        if (hd == -1) continue;
        err = eraAf2a(de_s, de_deg, de_min, de_sec, &de);
        assert(!err);
        err = eraTf2a('+', ra_hour, ra_min, ra_sec, &ra);
        assert(!err);
        dump_entry(i,
                   "type", "%s", "star",
                   "catalog_source", "%s", "Bright Star Catalogue",
                   "hd", "%d",   hd,
                   "vmag","%.2f", vmag,
                   "ra",  "%.8f", ra,
                   "de",  "%.8f", de,
                   "plx", "%.2f", plx,
                   "bv",  "%.3f", bv,
                   NULL);
    }
    printf("]\n");
    return 0;
}

static int dump_tycho2(const char *data)
{
    int i, tyc1, tyc2, tyc3, hip;
    double ra, de, pm_ra, pm_de, bt_mag, vt_mag, vmag, bv;
    char tyc, tyc2_s[32];

    printf("[");
    CATALOG_ITER(TYCHO2_CAT, data, i, &tyc1, &tyc2, &tyc3, &ra, &de,
                             &pm_ra, &pm_de, &bt_mag, &vt_mag,
                             &tyc, &hip) {
        sprintf(tyc2_s, "%d-%d-%d", tyc1, tyc2, tyc3);
        // Compute vmag and b-v according to the formula described in
        // tycho-2 ReadMe file.
        vmag = vt_mag - 0.090 * (bt_mag - vt_mag);
        bv = 0.850 * (bt_mag - vt_mag);
        dump_entry(i,
                   "type", "%s", "star",
                   "catalog_source", "%s", "Tycho-2",
                   "TYC", "\"%s\"",   tyc2_s,
                   (hip != -1) ? "HIP" : "", "%d", hip,
                   "vmag","%.2f", vmag,
                   "ra",  "%.8f", ra,
                   "de",  "%.8f", de,
                   "bv",  "%.3f", bv,
                   NULL);
    }
    printf("]\n");
    return 0;
}

static int dump_nomad(const char *data)
{
    int i;
    char nomad[16] = {}, tycho[16] = {};
    double ra, de, pm_ra, pm_de, vmag;
    int hip;
    printf("[");
    CATALOG_ITER(NOMAD_CAT, data, i, nomad, tycho,
                            &ra, &de, &pm_ra, &pm_de, &vmag) {
        if (!strchr(tycho, '-')) {
            hip = atoi(tycho);
            tycho[0] = '\0';
        } else {
            hip = 0;
        }
        dump_entry(i,
                   "type", "%s", "star",
                   "catalog_source", "%s", "NOMAD",
                   "NOMAD", "\"%s\"", nomad,
                   hip ? "HIP" : "", "%d", hip,
                   tycho[0] ? "TYC" : "", "\"%s\"", tycho,
                   "vmag","%.2f", vmag,
                   "ra",  "%.8f", ra,
                   "de",  "%.8f", de,
                   NULL);
    }
    printf("]\n");
    return 0;
}

int dump_catalog(const char *path)
{
    const char *data;
    data = asset_get_data(path, NULL, NULL);
    if (catalog_match(HIP_CAT, data)) {
        return dump_hip(data);
    } else if (catalog_match(BSC_CAT, data)) {
        return dump_bsc(data);
    } else if (catalog_match(TYCHO2_CAT, data)) {
        return dump_tycho2(data);
    } else if (catalog_match(NOMAD_CAT, data)) {
        return dump_nomad(data);
    } else {
        LOG_E("Cannot parse file %s", path);
        return -1;
    }
    return 0;
}
