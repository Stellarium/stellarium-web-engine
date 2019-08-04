#!/usr/bin/python

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generate a list of ephemerides using pyephem.
# The output of this script is used in the test in src/swe.c

import ephem
from math import *

R2D = 180. / pi;

def compute(target, o=None):
    if o is None:
        o = ephem.city('Atlanta')
        o.date = '2009/09/06 17:00'
    if isinstance(target, str):
        target = ephem.star(target)
    target.compute(o)
    # Pyephem use Dublin JD, ephemeride uses Modified JD!
    mjd = o.date + 15020 - 0.5
    print ('        {"%s", %.8f, %.8f, %.8f,\n'
           '         %.8f, %.8f, %.8f, %.8f, %.8f, %.8f},') % (
            target.name, mjd, o.lon * R2D, o.lat * R2D,
            target.a_ra * R2D, target.a_dec * R2D,
            target.ra * R2D, target.dec * R2D,
            target.alt * R2D, target.az * R2D)

compute(ephem.Sun())
compute(ephem.Moon())
compute('Polaris')
compute(ephem.Jupiter())
compute(ephem.Io())
compute(ephem.Phobos())
compute(ephem.Deimos())

# ISS TLE as of 2019-08-04.
iss = ephem.readtle(
    'ISS (ZARYA)',
    '1 25544U 98067A   19216.19673594 -.00000629  00000-0 -27822-5 0  9998',
    '2 25544  51.6446 123.0769 0006303 213.9941 302.5470 15.51020378182708')
o = ephem.city('Atlanta')
o.date = '2019/08/04 17:00'
compute(iss, o)
