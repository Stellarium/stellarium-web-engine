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
atlanta = (planets['earth'] + Topos('33.7490 N', '84.3880 W')).at(t)

def dump_pv(p):
    print(' {{%.12f, %.12f, %.12f},\n'
          '  {%.12f, %.12f, %.12f}},' %
          (p.position.au[0], p.position.au[1], p.position.au[2],
          p.velocity.au_per_d[0], p.velocity.au_per_d[1], p.velocity.au_per_d[2]))

def dumppos(planet_name):
    # Barycentric, Geocentric and Observercentric (Topocentric) positions
    planet = planets[planet_name]
    p_bary = planet.at(t)
    p_geo  = earth.observe(planet)
    p_obs  = atlanta.observe(planet)
    print('{\n "%s",' % (planet_name))
    dump_pv(p_bary)
    dump_pv(p_geo)
    dump_pv(p_obs)
    print('},')

dumppos('sun')
dumppos('venus')
dumppos('earth')
dumppos('moon')
dumppos('pluto barycenter')

