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

import json
import skyfield.api as sf

# Load all the needed kernels.
loader = sf.Loader('./tmp')
de421 = loader('de421.bsp')
jup310 = loader('jup310.bsp')
mar097 = loader('https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/'
                'satellites/mar097.bsp')

# Compute target using skyfield.
def compute(target, kernel=de421, name=None, topo=None, t=None, planet=0,
            precision_radec=15, precision_azalt=120, klass=None, json=None):
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
    if not planet and isinstance(target.target, int):
        planet = target.target

    obs = (de421['earth'] + topo).at(t)
    pos = obs.observe(target)
    radec = pos.radec(t)
    altaz = pos.apparent().altaz()
    # skyfield use JD, ephemeride uses Modified JD.
    mjd = t.ut1 - 2400000.5
    ret = dict(
        name = name,
        planet = planet,
        utc = mjd,
        longitude = topo.longitude.degrees,
        latitude = topo.latitude.degrees,
        ra = radec[0]._degrees,
        dec = radec[1].degrees,
        alt = altaz[0].degrees,
        az = altaz[1].degrees,
        precision_radec = precision_radec,
        precision_azalt = precision_azalt,
    )
    if json is not None:
        ret['klass'] = klass
        ret['json'] = json
    return ret
    '''
    print('        {"%s", %.8f, %.8f, %.8f,\n'
          '          %.8f, %.8f, %.8f, %.8f},' % (
          name, mjd, topo.longitude.degrees, topo.latitude.degrees,
          radec[0]._degrees, radec[1].degrees,
          altaz[0].degrees, altaz[1].degrees))
    '''

def compute_all():
    yield compute('Sun', precision_radec=30)
    yield compute('Moon')
    yield compute('Jupiter barycenter', planet=599)
    yield compute('Io', kernel=jup310)
    yield compute('Phobos', kernel=mar097, precision_radec=30)
    yield compute('Deimos', kernel=mar097, precision_radec=30)

    # TODO.
    # compute('Polaris')

    # ISS, using TLE as of 2019-08-04.
    iss_tle = [
        '1 25544U 98067A   19216.19673594 -.00000629  00000-0 -27822-5 0  9998',
        '2 25544  51.6446 123.0769 0006303 213.9941 302.5470 15.51020378182708',
    ]
    iss = sf.EarthSatellite(*iss_tle, 'ISS (ZARYA)')
    iss = de421['earth'] + iss
    iss_json = {
        'model_data': {
            'norad_num': 25544,
            'tle': iss_tle,
        }
    }
    yield compute(iss, name='ISS', t=[2019, 8, 4, 17, 0], json=iss_json,
                  klass='tle_satellite',
                  precision_radec=400, precision_azalt=400)


def c_format(v):
    if isinstance(v, dict): v = json.dumps(v)
    if isinstance(v, str): return '"{}"'.format(v.replace('"', '\\"'))
    return repr(v)

if __name__ == '__main__':
    out = open('./src/ephemeris_tests.inl', 'w')
    print('// Generated from tools/compute-ephemeris.py\n', file=out)
    for d in compute_all():
        fields = ['    .{} = {}'.format(k, c_format(v)) for k, v in d.items()]
        print('{\n%s\n},' % ',\n'.join(fields), file=out)
