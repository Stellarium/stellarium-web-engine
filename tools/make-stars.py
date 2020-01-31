#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# This script generates a small stars survey from Hipparcos catalog.  This
# survey is directly bundled in the code so we can always use it by default.

from math import *
import collections
import gzip
import hashlib
import healpy
import os
import requests
import struct
import zlib

import eph
from utils import ensure_dir, download, parse, DD2R

MAX_SOURCES_PER_TILE = 1024

Star = collections.namedtuple('Star',
        ['hip', 'hd', 'vmag', 'ra', 'de', 'plx', 'pra', 'pde', 'bv', 'ids'])

print('Parse bayer cat')
bayers = {}
bayer_file = download(
        'http://cdsarc.u-strasbg.fr/vizier/ftp/cats/IV/27A/catalog.dat',
        dest='data-src/IV_27A.dat',
        md5='7b51c0aa8255c6aaf261a1083e0f0bd8')
for line in open(bayer_file):
    hd = int(line[0:6].strip())
    flamsteed = line[64:67].strip()
    bayer = line[68:73].strip()
    cst = line[74:77].strip()
    ids = []
    if bayer:
        ids.append('* %s %s' % (bayer, cst))
    if flamsteed:
        ids.append('* %s %s' % (flamsteed, cst))
    bayers[hd] = ids

print('Parse stars')
stars = {}
hip_file = download(
        'http://cdsarc.u-strasbg.fr/ftp/pub/cats/I/239/hip_main.dat.gz',
        md5='7b50b051364b3f846ba8d6cdf81a4fcb')

for line in gzip.open(hip_file):
    hd = parse(line, 391, 396, type=int)
    if hd is None: continue
    hip = parse(line, 9, 14, type=int, required=True)
    vmag = parse(line, 42, 46, required=True)
    # Ignore stars with vmag > 7.
    if vmag > 7: continue
    ra = parse(line, 52, 63, conv=DD2R)
    de = parse(line, 65, 76, conv=DD2R)
    if ra is None or de is None: continue
    plx = parse(line, 80, 86, default=0.0, conv=1/1000.)
    bv = parse(line, 246, 251, default=0.0)
    ids = '|'.join(bayers.get(hd, [])).encode()
    star = Star(hip=hip, hd=hd, vmag=vmag,
                ra=ra, de=de, plx=plx, pra=0, pde=0, bv=bv, ids=ids)
    stars[hd] = star

stars = sorted(stars.values(), key=lambda x: (x.vmag, x.hd))

# Create the tiles.
# For each star, try to add to the lowest tile that is not already full.
print('create tiles')

max_vmag_order0 = 0

tiles = {}
for s in stars:
    for order in range(8):
        pix = healpy.ang2pix(1 << order, pi / 2.0 - s.de, s.ra, nest=True)
        nuniq = pix + 4 * (1 << (2 * order))
        assert int(log(nuniq // 4, 2) / 2) == order
        tile = tiles.setdefault(nuniq, [])
        if len(tile) >= MAX_SOURCES_PER_TILE: continue # Try higher order
        tile.append(s)
        if order == 0: max_vmag_order0 = max(max_vmag_order0, s.vmag)
        break

print('save tiles')
out_dir = './data/skydata/stars/'
COLUMNS = [
    {'id': 'hip',  'type': 'i'},
    {'id': 'hd',   'type': 'i'},
    {'id': 'vmag', 'type': 'f', 'unit': eph.UNIT_VMAG, 'zerobits': 16},
    {'id': 'ra',   'type': 'f', 'unit': eph.UNIT_RAD, 'zerobits': 8},
    {'id': 'de',   'type': 'f', 'unit': eph.UNIT_RAD, 'zerobits': 8},
    {'id': 'plx',  'type': 'f', 'unit': eph.UNIT_ARCSEC, 'zerobits': 16},
    {'id': 'pra',  'type': 'f', 'unit': eph.UNIT_RAD_PER_YEAR, 'zerobits': 16},
    {'id': 'pde',  'type': 'f', 'unit': eph.UNIT_RAD_PER_YEAR, 'zerobits': 16},
    {'id': 'bv',   'type': 'f'},
    {'id': 'ids',  'type': 's', 'size': 256},
]

for nuniq, stars in tiles.items():
    stars = sorted(stars, key=lambda x: x.hip * 1000000 + x.hd)
    eph.create_tile(stars, chunk_type='STAR', nuniq=nuniq, path=out_dir,
                    columns=COLUMNS)

# Also generate the properties file.
with open(os.path.join(out_dir, 'properties'), 'w') as out:
    props = dict(
        hips_order_min = 0,
        max_vmag=max_vmag_order0,
        type = 'stars',
        hips_tile_format = 'eph',
    )
    for key, v in props.items():
        print('{:<24} = {}'.format(key, v), file=out)
