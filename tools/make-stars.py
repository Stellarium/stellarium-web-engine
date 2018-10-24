#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# This script generates the eph format stars survey from HIP and BSC catalog.

from math import *
import collections
import gzip
import hashlib
import healpy
import numpy as np
import os
import requests
import struct
import zlib

import eph
from utils import ensure_dir, download, parse, DD2R

MAX_SOURCES_PER_TILE = 1024

Star = collections.namedtuple('Star',
        ['hip', 'hd', 'vmag', 'ra', 'de', 'plx', 'pra', 'pde', 'bv'])

print 'parse stars'
stars = {}
hip_file = download(
        'http://cdsarc.u-strasbg.fr/ftp/pub/cats/I/239/hip_main.dat.gz',
        md5='7b50b051364b3f846ba8d6cdf81a4fcb')

for line in gzip.open(hip_file):
    hd = parse(line, 391, 396, type=int)
    if hd is None: continue
    hip = parse(line, 9, 14, type=int, required=True)
    vmag = parse(line, 42, 46, required=True)
    ra = parse(line, 52, 63, conv=DD2R)
    de = parse(line, 65, 76, conv=DD2R)
    if ra is None or de is None: continue
    plx = parse(line, 80, 86, default=0.0, conv=1/1000.)
    bv = parse(line, 246, 251, default=0.0)
    star = Star(hip=hip, hd=hd, vmag=vmag,
                ra=ra, de=de, plx=plx, pra=0, pde=0, bv=bv)
    stars[hd] = star

stars = sorted(stars.values(), key=lambda x: (x.vmag, x.hd))

# Create the tiles.
# For each star, try to add to the lowest tile that is not already full.
print 'create tiles'

max_vmag_order0 = 0

tiles = {}
for s in stars:
    for order in range(8):
        pix = healpy.ang2pix(1 << order, pi / 2.0 - s.de, s.ra, nest=True)
        nuniq = pix + 4 * (1L << (2 * order))
        assert int(log(nuniq / 4, 2) / 2) == order
        tile = tiles.setdefault(nuniq, [])
        if len(tile) >= MAX_SOURCES_PER_TILE: continue # Try higher order
        tile.append(s)
        if order == 0: max_vmag_order0 = max(max_vmag_order0, s.vmag)
        break

print 'save tiles'
out_dir = './data/stars/'
dtype=[('hip', 'uint32'), ('hd', 'uint32'), ('vmag', 'float32'),
       ('ra', 'float32'), ('de', 'float32'), ('plx', 'float32'),
       ('pra', 'float32'), ('pde', 'float32'), ('bv', 'float32')]
infos = {
    'vmag': {'unit': eph.UNIT_VMAG, 'zerobits': 16},
    'ra':   {'unit': eph.UNIT_RAD, 'zerobits': 8},
    'de':   {'unit': eph.UNIT_RAD, 'zerobits': 8},
    'plx':  {'unit': eph.UNIT_ARCSEC, 'zerobits': 16},
    'pra':  {'unit': eph.UNIT_RAD_PER_YEAR, 'zerobits': 16},
    'pde':  {'unit': eph.UNIT_RAD_PER_YEAR, 'zerobits': 16},
}

for nuniq, stars in tiles.items():
    stars = sorted(stars, key=lambda x: x.hip * 1000000 + x.hd)
    stars = np.array(stars, dtype)
    eph.create_tile(stars, chunk_type='STAR', nuniq=nuniq, path=out_dir,
                    infos=infos)

# Also generate the properties file.
with open(os.path.join(out_dir, 'properties'), 'w') as out:
    props = dict(
        hips_order_min = 0,
        max_vmag=max_vmag_order0,
        type = 'stars',
        hips_tile_format = 'eph',
    )
    for key, v in props.items():
        print >>out, '{:<24} = {}'.format(key, v)
