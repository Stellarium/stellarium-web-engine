/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * C bindings to the SGP4 functions for artifitial satellites position
 * computation so that we can use them from C.
 */

#include <stdbool.h>

typedef struct sgp4_elsetrec sgp4_elsetrec_t;

sgp4_elsetrec_t *sgp4_twoline2rv(
        const char str1[130], const char str2[130],
        char typerun, char typeinput, char opsmode,
        double *startmfe, double *stopmfe, double *deltamin);

bool sgp4(sgp4_elsetrec_t *satrec, double utc_mjd, double r[3], double v[3]);

/*
 * Function: sgp4_get_satepoch
 * Return the reference epoch of a sat (UTC MJD)
 */
double sgp4_get_satepoch(const sgp4_elsetrec_t *satrec);
