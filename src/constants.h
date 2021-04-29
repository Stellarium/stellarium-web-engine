/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: constants.h
 * Some macro of useful constants.
 *
 * Some are taken from erfa, so that if the values are improved in erfa we
 * automatically get the proper value.
 */

#include "erfa.h"

#define PARSEC_IN_METER 3.08567758e16
#define LIGHT_YEAR_IN_METER 9.4605284e15

/* Julian Date of Modified Julian Date zero */
#define DJM0    ERFA_DJM0

/* Degrees to radians */
#define DD2R    ERFA_DD2R

/* Radians to degrees */
#define DR2D    ERFA_DR2D

/* Meter to Astronomical unit */
#define DM2AU  (1. / ERFA_DAU)
/* Astronomical unit to Meter */
#define DAU2M  ERFA_DAU

/* Seconds of time to radians */
#define DS2R    ERFA_DS2R

/* Light time for 1 au (s) */
#define AULT    ERFA_AULT

/* Seconds per day. */
#define DAYSEC  ERFA_DAYSEC

/* Days per Julian year */
#define DJY     ERFA_DJY

/* Reference epoch (J2000.0), Modified Julian Date */
#define DJM00 (51544.5)

/* Milliarcseconds to radians */
#define DMAS2R ERFA_DMAS2R

/* Radians to mas */
#define DR2MAS (1. / ERFA_DMAS2R)
