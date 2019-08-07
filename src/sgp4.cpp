/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/* Include the original SGP4.cpp file, with a lot of pragmas to prevent
 * warnings!  */

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wformat"

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#endif

#include "../ext_src/sgp4/SGP4.cpp"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

extern "C" {
    #include "sgp4.h"
}

#include <stdlib.h>

sgp4_elsetrec_t *sgp4_twoline2rv(
        const char str1_[130], const char str2_[130],
        char typerun, char typeinput, char opsmode,
        double *startmfe, double *stopmfe, double *deltamin)
{
    char str1[131] = {}, str2[131] = {};
    elsetrec *ret = (elsetrec*)calloc(1, sizeof(*ret));
    strncpy(str1, str1_, 130);
    strncpy(str2, str2_, 130);

    SGP4Funcs::twoline2rv(str1, str2, typerun, typeinput, opsmode,
                          wgs72, *startmfe, *stopmfe, *deltamin, *ret);
    return (sgp4_elsetrec*)ret;
}

bool sgp4(sgp4_elsetrec_t *satrec, double utc_mjd, double r[3], double v[3])
{
    double tsince;
    elsetrec *elrec = (elsetrec*)satrec;
    tsince = utc_mjd - (elrec->jdsatepoch - 2400000.5 + elrec->jdsatepochF);
    tsince *= 24 * 60; // Put in min.
    return SGP4Funcs::sgp4(*((elsetrec*)satrec), tsince, r, v);
}

/*
 * Function: sgp4_get_satepoch
 * Return the reference epoch of a sat (UTC MJD)
 */
double sgp4_get_satepoch(const sgp4_elsetrec_t *satrec)
{
    elsetrec *elrec = (elsetrec*)satrec;
    return (elrec->jdsatepoch + elrec->jdsatepochF) - 2400000.5;
}
