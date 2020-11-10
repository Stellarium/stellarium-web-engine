#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# This script queries NASA JPL HORIZONS service using the batch interface to
# update the solar system planet info in data/planets.ini.

import configparser
import math
import os
import requests
import re
import sys
import time


if os.path.dirname(__file__) != "./tools":
    print("Should be run from root directory")
    sys.exit(-1)


def get_all():
    """Get the list of major objects."""
    ret = []
    res = requests.get('https://ssd.jpl.nasa.gov/horizons_batch.cgi',
                       params={'batch':1, 'command':"'*'"})
    for line in res.text.splitlines():
        m = re.match(r'^ +(\d+)  (.+?)  +(.+?)  +(.+?) *$', line)
        if not m: continue
        id = int(m.group(1))
        name = m.group(2).strip()
        if not name or id < 10 or id > 999: continue
        ret.append((id, name))
    return ret


def parse_geophysical_data(txt):
    """Parse the physical info from the text

    Return a dic of key values.

    Unfortunately HORIZONS doesn't allow to get those in an computer friendly
    format, so we have to use some heuristics to get the values.
    """
    ret = {}
    flags = re.IGNORECASE | re.MULTILINE
    for v in re.findall(r'radius.*?=\s*([\d.]+)', txt, flags):
        ret['radius'] = '%g km' % float(v)
    for v in re.findall(r'albedo\s*?=\s*([\d.]+)', txt, flags):
        ret['albedo'] = '%g' % float(v)
    for v in re.findall(r'Mass \(10\^(\d+) kg\s*\)\s*?=\s*([\d.]+)',
                        txt, flags):
        ret['mass'] = '%g kg' % (float(v[1]) * 10**int(v[0]))
    return ret


def parse_orbital_data(txt):
    """Parse the orbital data line

    Return a dict of values to be added to the ini file.
    """

    # Here are the values we get from HORIZONS.
    # For simplicity for the moment we just add them all as a single value
    # in the ini file.

    # JDTDB  Julian Day Number, Barycentric Dynamical Time
    # EC     Eccentricity, e
    # QR     Periapsis distance, q (km)
    # IN     Inclination w.r.t XY-plane, i (degrees)
    # OM     Longitude of Ascending Node, OMEGA, (degrees)
    # W      Argument of Perifocus, w (degrees)
    # Tp     Time of periapsis (Julian Day Number)
    # N      Mean motion, n (degrees/sec)
    # MA     Mean anomaly, M (degrees)
    # TA     True anomaly, nu (degrees)
    # A      Semi-major axis, a (km)
    # AD     Apoapsis distance (km)
    # PR     Sidereal orbit period (sec)

    ret = {}
    line = re.search(r'^\$\$SOE\n(.*)\n\$\$EOE', txt, re.MULTILINE).group(1)
    return dict(orbit='horizons:%s' % line)


config = configparser.ConfigParser()
config.read('./data/planets.ini')


all_bodies = get_all()

now = time.time() / 86400.0 + 2440587.5 - 2400000.5
# Truncate to the day, so that we can run the script several times
# and the values won't change.
now = math.floor(now)
print('now', now)

for id, name in all_bodies:
    if id == 10: continue # skip sun.
    if id // 100 == 3: continue  # skip Moon, L1, L2, L4, L4, L5, Earth
    if id % 100 == 99: parent = 10 # Planet
    if id % 100 != 99: parent = id // 100 * 100 + 99 # Moon

    section = name.lower()
    print('process %s (%d)' % (name, id))

    try:
        center = '500@%d' % parent # Center of parent body.

        res = requests.get('https://ssd.jpl.nasa.gov/horizons_batch.cgi',
                params=dict(
                    batch=1, command=id, csv_format='yes',
                    table_type='elements', center=center, ref_plane='frame',
                    tlist=now,
                ))

        data = dict(horizons_id=str(id))
        data.update(parse_geophysical_data(res.text))

        # Only update horizons orbits.
        if      not config.has_section(section) or \
                not config.has_option(section, 'orbit') or \
                config.get(section, 'orbit').startswith('horizons:'):
            data.update(parse_orbital_data(res.text))
        data['parent'] = [x for x in all_bodies if x[0] == parent][0][1].lower()
        data['type'] = 'Pla' if parent == 10 else 'Moo'

        print(', '.join('%s = %s' % (k, v[:16] + bool(v[16:]) * '...')
                        for k, v in data.items()))
        # if not 'albedo' in data or not 'radius' in data: continue
        if not config.has_section(section): config.add_section(section)

        for key, value in data.items():
            config.set(section, key, value)
    except Exception as ex:
        print('Failed:', ex)

print('Update planets.ini')
config.write(open('./data/planets.ini', 'w'))
