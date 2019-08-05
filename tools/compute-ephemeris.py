#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generate a list of ephemerides using pyephem.
# The output of this script is used in the test in src/swe.c

import skyfield.api as sf

# Load all the needed kernels.
loader = sf.Loader('./tmp')
de421 = loader('de421.bsp')
jup310 = loader('jup310.bsp')
mar097 = loader('https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/'
                'satellites/mar097.bsp')

# Compute target using skyfield.
def compute(target, kernel=de421, name=None, topo=None, t=None):
    ts = sf.load.timescale()
    if isinstance(target, str):
        name = target
        target = kernel[target]
    if topo is None:
        topo = sf.Topos('33.7490 N', '84.3880 W') # Atlanta.
    if t is None:
        t = [2019, 9, 6, 17, 0, 0]
    t = ts.utc(*t)
    if name is None:
        name = target.target_name
    obs = (de421['earth'] + topo).at(t)
    pos = obs.observe(target)
    radec = pos.radec(t)
    altaz = pos.apparent().altaz()
    # skyfield use JD, ephemeride uses Modified JD.
    mjd = t.ut1 - 2400000.5
    print('        {"%s", %.8f, %.8f, %.8f,\n'
          '          %.8f, %.8f, %.8f, %.8f},' % (
          name, mjd, topo.longitude.degrees, topo.latitude.degrees,
          radec[0]._degrees, radec[1].degrees,
          altaz[0].degrees, altaz[1].degrees))


compute('Sun')
compute('Moon')
compute('Jupiter barycenter')
compute('Io', kernel=jup310)
compute('Phobos', kernel=mar097)
compute('Deimos', kernel=mar097)

# TODO.
# compute('Polaris')

# ISS, using TLE as of 2019-08-04.
iss = sf.EarthSatellite(
     '1 25544U 98067A   19216.19673594 -.00000629  00000-0 -27822-5 0  9998',
     '2 25544  51.6446 123.0769 0006303 213.9941 302.5470 15.51020378182708',
     'ISS (ZARYA)')
iss = de421['earth'] + iss
compute(iss, name='ISS', t=[2019, 8, 4, 17, 0])
