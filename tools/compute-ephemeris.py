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

import ephem
import json
import skyfield.api as sf

from skyfield.data import hipparcos
from math import *

# Load all the needed kernels.
loader = sf.Loader('./tmp')
de421 = loader('de421.bsp')
jup365 = loader('jup365.bsp')
mar097 = loader('https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/'
                'satellites/mar097.bsp')

def JD_to_besselian_epoch(jd):
  return 2000.0 + (jd - 2451545.0 ) / 365.25

# Compute target using skyfield.
def compute(target, kernel=de421, name=None, topo=None, t=None, planet=None,
            precision_radec=3, precision_azalt=5, klass=None, json=None):
    ts = sf.load.timescale()
    if isinstance(target, str):
        name = target
        target = kernel[target]

    if topo is None:
        topo = ['33.7490 N', '84.3880 W'] # Atlanta.
    topo = sf.Topos(*topo)

    if t is None:
        t = [2019, 9, 6, 17, 0, 0]
    t = ts.utc(*t)
    if name is None:
        name = target.target_name

    if planet is None and isinstance(target.target, int):
        planet = target.target

    earth = de421['earth']
    obs = (earth + topo).at(t)
    pos = obs.observe(target)  # Astrometric position
    apparent = obs.observe(target).apparent()  # Apparent position
    geo = earth.at(t).observe(target)
    radec = apparent.radec(epoch='date')
    altaz = pos.apparent().altaz()
    # skyfield use JD, ephemeride uses Modified JD.
    ut1 = t.ut1 - 2400000.5
    utc = ut1 - t.dut1 / (60 * 60 * 24)
    ret = dict(
        name = name,
        planet = planet,
        ut1 = ut1,
        utc = utc,
        longitude = topo.longitude.degrees,
        latitude = topo.latitude.degrees,
        pos = list(apparent.position.au),
        ra = radec[0]._degrees,
        dec = radec[1].degrees,
        alt = altaz[0].degrees,
        az = altaz[1].degrees,
        geo = list(geo.position.au),
        precision_radec = precision_radec,
        precision_azalt = precision_azalt,
    )
    if json is not None:
        ret['klass'] = klass
        ret['json'] = json
    return ret


def compute_asteroid(name, data, t, precision_radec=15, precision_azalt=120):
    R2D = 180. / pi;
    o = ephem.city('Atlanta')
    o.date = t
    target = ephem.EllipticalBody()
    target._inc = data['i']
    target._Om = data['Node']
    target._om = data['Peri']
    target._a = data['a']
    target._M = data['M']
    target._epoch_M = data['Epoch'] - 2415020.0
    target._e = data['e']
    target._epoch = '2000'
    target.compute(o)
    # Pyephem use Dublin JD, ephemeride uses Modified JD!
    mjd = o.date + 15020 - 0.5
    return dict(
        name = name,
        planet = 0,
        utc = mjd,
        longitude = o.lon * R2D,
        latitude = o.lat * R2D,
        ra = target.ra * R2D,
        dec = target.dec * R2D,
        alt = target.alt * R2D,
        az = target.az * R2D,
        precision_radec = precision_radec,
        precision_azalt = precision_azalt,
        klass = 'mpc_asteroid',
        json = dict(model_data=data),
    )

def compute_star(star, name, precision_radec = 0.1):
    data = dict(
        ra = star.ra._degrees,
        de = star.dec._degrees,
        plx = star.parallax_mas,
        pm_ra = star.ra_mas_per_year,
        pm_de = star.dec_mas_per_year,
        epoch = JD_to_besselian_epoch(star.epoch),
        rv = star.radial_km_per_s
    )
    return compute(star, name=name, klass='star', json=dict(model_data=data),
                   planet=0, precision_radec=precision_radec,
                   precision_azalt = 5 )



def compute_all():
    yield compute('Sun', precision_radec=1)
    yield compute('Moon')
    yield compute('Jupiter barycenter', planet=599)
    yield compute('Io', kernel=jup365)

    # XXX: Those tests might fail when we update the planet.ini data since
    #      the orbits are not stable.
    t = [2021, 3, 22, 15, 0, 0]
    yield compute('Metis', kernel=jup365, t=t,
                  precision_radec=15, precision_azalt=20)
    yield compute('Thebe', kernel=jup365, t=t,
                  precision_radec=15, precision_azalt=20)
    yield compute('Phobos', kernel=mar097, t=t,
                  precision_radec=5, precision_azalt=10)
    yield compute('Deimos', kernel=mar097, precision_radec=5, t=t,
                  precision_azalt=10)
    yield compute('Pluto barycenter', planet=999, t=t,
                  precision_radec=10, precision_azalt=15)
    yield compute('Pluto barycenter', planet=999, t=t,
                  precision_radec=10, precision_azalt=15)

    # ISS, using TLE as of 2019-08-04.
    tle = [
        '1 25544U 98067A   19216.19673594 -.00000629  00000-0 -27822-5 0  9998',
        '2 25544  51.6446 123.0769 0006303 213.9941 302.5470 15.51020378182708',
    ]
    iss = sf.EarthSatellite(*tle, 'ISS (ZARYA)')
    iss = de421['earth'] + iss
    json = {
        'model_data': {
            'norad_number': 25544,
            'tle': tle,
        }
    }

    yield compute(iss, name='ISS', t=[2019, 8, 4, 17, 0], json=json,
                  planet=0, klass='tle_satellite',
                  precision_radec=15, precision_azalt=15)

    yield compute(iss, name='ISS', t=[2019, 8, 3, 20, 51, 46], json=json,
                  topo=['43.4822 N', '1.432 E'], # Goyrans
                  planet=0, klass='tle_satellite',
                  precision_radec=100, precision_azalt=100)

    # Pallas, using MPC data as of 2019-08-06.
    data = {"Epoch": 2458600.5, "M": 59.69912, "Peri": 310.04884,
            "Node": 173.08006, "i": 34.83623, "e": 0.2303369, "n": 0.21350337,
            "a": 2.772466}
    yield compute_asteroid('Pallas', data=data, t='2019/08/10 17:00',
                           precision_radec=200, precision_azalt=400)

    with sf.load.open(hipparcos.URL) as f:
        df = hipparcos.load_dataframe(f)
    polaris = sf.Star.from_dataframe(df.loc[11767])
    yield compute_star(polaris, name='Polaris')

    proxima = sf.Star.from_dataframe(df.loc[70890])
    yield compute_star(proxima, name='Proxima Centauri')

    barnard = sf.Star.from_dataframe(df.loc[87937])
    yield compute_star(barnard, name='Barnards Star')



def c_format(v):
    if isinstance(v, dict): v = json.dumps(v)
    if isinstance(v, str): return '"{}"'.format(v.replace('"', '\\"'))
    if isinstance(v, list): return '{%s}' % ', '.join(c_format(x) for x in v)
    return repr(v)

if __name__ == '__main__':
    out = open('./src/ephemeris_tests.inl', 'w')
    print('// Generated from tools/compute-ephemeris.py\n', file=out)
    for d in compute_all():
        fields = ['    .{} = {}'.format(k, c_format(v)) for k, v in d.items()]
        print('{\n%s\n},' % ',\n'.join(fields), file=out)
