#!/usr/bin/python

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generate a list of ephemerides using Skyfield.
# The output of this script is used in the test in src/frame.c

from skyfield.api import load
from skyfield.api import Topos

planets = load('de421.bsp')
ts = load.timescale()
t = ts.utc(2018, 11, 28, 0, 0, 0)
earth = planets['earth'].at(t)
atlanta_p = (planets['earth'] + Topos('33.7490 N', '84.3880 W'))
atlanta = atlanta_p.at(t)

print('TAI = %r' % (t.tai - 2400000.5))
print('TT  = %r' % (t.tt  - 2400000.5))
print('UT1 = %r' % (t.ut1 - 2400000.5))

def dump_pv(p):
    print(' {{%.12f, %.12f, %.12f},\n'
          '  {%.12f, %.12f, %.12f}},' %
          (p.position.au[0], p.position.au[1], p.position.au[2],
          p.velocity.au_per_d[0], p.velocity.au_per_d[1], p.velocity.au_per_d[2]))

def dump_altazd(p):
    print(' {%.12f, %.12f, %.12f},' %
          (p[0].degrees, p[1].degrees, p[2].au))

def dumppos(planet_name):
    # Barycentric, Geocentric and Observercentric (Topocentric) positions
    planet = planets[planet_name]
    p_bary = planet.at(t)
    p_geo  = earth.observe(planet)
    p_obs  = atlanta.observe(planet)
    p_obsazalt = p_obs.apparent().altaz(temperature_C=15, pressure_mbar=1013.25)
    print('{\n "%s",' % (planet_name))
    dump_pv(p_bary)
    dump_pv(p_geo)
    dump_pv(p_obs)
    dump_altazd(p_obsazalt)
    print('},')

print('// Barycentric/geocentric position of atlanta')
print('{\n "atlanta",')
dump_pv(atlanta)
dump_pv(earth.observe(atlanta_p))
print(' {{0.0, 0.0, 0.0},\n  {0.0, 0.0, 0.0}}')
print('};')

dumppos('sun')
dumppos('venus')
dumppos('earth')
dumppos('moon')
dumppos('pluto barycenter')

