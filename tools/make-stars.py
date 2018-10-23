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

import collections
import gzip
import hashlib
import healpy
from math import *
import os
import requests
import requests_cache
import struct
import zlib

MAX_SOURCES_PER_TILE = 1024

# Seconds of time to radians
DS2R = 7.272205216643039903848712e-5
# Arcseconds to radians
DAS2R = 4.848136811095359935899141e-6
# Degrees to radians
DD2R = 1.745329251994329576923691e-2
# Radians to degrees
DR2D = 57.29577951308232087679815

# Unit constants: must be exactly the same as in src/eph-file.h!
EPH_RAD             = 1 << 16
EPH_VMAG            = 3 << 16
EPH_ARCSEC          = 5 << 16 | 1 | 2 | 4
EPH_RAD_PER_YEAR    = 7 << 16


Star = collections.namedtuple('Star',
        ['hd', 'hip', 'vmag', 'ra', 'de', 'plx', 'bv', 'sp'])

if os.path.dirname(__file__) != "./tools":
    print "Should be run from root directory"
    sys.exit(-1)

def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

ensure_dir('data-src/')
requests_cache.install_cache('data-src/cache')

def download(url, md5=None):
    filename = os.path.basename(url)
    outpath = 'data-src/{}'.format(filename)
    print("Download '{}'".format(url))
    r = requests.get(url)
    if md5 and hashlib.md5(r.content).hexdigest() != md5:
        print "Wrong md5 for %s" % url
        sys.exit(-1)
    with open(outpath, 'wb') as out:
        out.write(r.content)
    return outpath


def shuffle_bytes(data, size):
    assert len(data) % size == 0
    ret = ''
    for i in range(size):
        for j in range(len(data) / size):
            ret += data[j * size + i]
    return ret


print 'parse stars'
hip_file = download(
        'http://cdsarc.u-strasbg.fr/ftp/pub/cats/I/239/hip_main.dat.gz',
        md5='7b50b051364b3f846ba8d6cdf81a4fcb')
bsc_file = download(
        'http://cdsarc.u-strasbg.fr/ftp/pub/cats/V/50/catalog.gz',
        md5='2f0662e53aa4e563acb8b2705f45c1a3')

def float_trunc(v, zerobits):
    """Truncate a float value so that it can be better compressed"""
    # A float is represented in binary like this:
    # seeeeeeeefffffffffffffffffffffff
    mask = -(1L << zerobits)
    v = struct.unpack('I', struct.pack('f', v))[0]
    v &= mask
    return struct.unpack('f', struct.pack('I', v))[0]


def parse(line, start, end, type=float, default=None, required=False,
          zerobits=0):
    line = line[start - 1:end].strip()
    if not line and not required: return default
    ret = type(line)
    if zerobits: ret = float_trunc(ret, zerobits)
    return ret

stars = {}

for line in gzip.open(bsc_file):
    hd = parse(line, 26, 31, type=int)
    if hd is None: continue
    ra_hour = parse(line, 76, 77, required=True)
    ra_min = parse(line, 78, 79, required=True)
    ra_sec = parse(line, 80, 83, required=True)
    ra = (60 * ((60 * ra_hour) + ra_min) + ra_sec) * DS2R
    ra = float_trunc(ra, 8)
    de_s = parse(line, 84, 84, type=str, required=True)
    de_deg = parse(line, 85, 86, type=int, required=True)
    de_min = parse(line, 87, 88, type=int, required=True)
    de_sec = parse(line, 89, 90, type=int, required=True)
    de = (60 * ((60 * de_deg) + de_min) + de_sec) * DAS2R
    if de_s == '-': de = -de
    de = float_trunc(de, 8)
    vmag = parse(line, 103, 107, zerobits=16, required=True)
    bv = parse(line, 110, 114, default=0.0, zerobits=16)
    plx = parse(line, 162, 166, default=0.0, zerobits=16)
    sp = parse(line, 130, 130, type=str)
    stars[hd] = Star(hd=hd, hip=0, vmag=vmag,
                     ra=ra, de=de, plx=plx, bv=bv, sp=ord(sp))


for line in gzip.open(hip_file):
    hd = parse(line, 391, 396, type=int)
    if hd is None: continue
    hip = parse(line, 9, 14, type=int, required=True)
    vmag = parse(line, 42, 46, zerobits=16, required=True)
    ra = parse(line, 52, 63, zerobits=8)
    de = parse(line, 65, 76, zerobits=8)
    if ra is None or de is None: continue
    plx = parse(line, 80, 86, default=0.0, zerobits=16)
    plx /= 1000. # MAS to AS.
    bv = parse(line, 246, 251, default=0.0, zerobits=16)
    sp = 0
    # If the stars was in the HD catalog, we use the vmag and sp from there.
    if hd in stars:
        vmag = stars[hd].vmag
        sp = stars[hd].sp
    star = Star(hd=hd, hip=hip, vmag=vmag,
                ra=ra * DD2R, de=de * DD2R, plx=plx, bv=bv, sp=sp)
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
ensure_dir(out_dir)

for nuniq, stars in tiles.items():
    stars = sorted(stars, key=lambda x: x.hip * 1000000 + x.hd)
    order = int(log(nuniq / 4, 2) / 2);
    pix = nuniq - 4 * (1 << (2 * order));
    path = '%s/Norder%d/Dir%d/Npix%d.eph' % (
            out_dir, order, (pix / 10000) * 10000, pix)
    ensure_dir(path)

    # Header:
    header = ''
    # shuffle, 40 bytes, 10 columns, <nb> rows
    header += struct.pack('iiii', 1, 40, 10, len(stars))
    header += struct.pack('4s4siii', 'hip',  'i', 0, 0,  4)
    header += struct.pack('4s4siii', 'hd',   'i', 0, 4,  4)
    header += struct.pack('4s4siii', 'sp',   'i', 0, 8,  4)
    header += struct.pack('4s4siii', 'vmag', 'f', EPH_VMAG, 12, 4)
    header += struct.pack('4s4siii', 'ra',   'f', EPH_RAD, 16, 4)
    header += struct.pack('4s4siii', 'de',   'f', EPH_RAD, 20, 4)
    header += struct.pack('4s4siii', 'plx',  'f', EPH_ARCSEC, 24, 4)
    header += struct.pack('4s4siii', 'pra',  'f', EPH_RAD_PER_YEAR, 28, 4)
    header += struct.pack('4s4siii', 'pde',  'f', EPH_RAD_PER_YEAR, 32, 4)
    header += struct.pack('4s4siii', 'bv',   'f', 0, 36, 4)

    data = ''
    for s in stars:
        line = struct.pack('iiifffffff',
                s.hip, s.hd, s.sp, s.vmag, s.ra, s.de, s.plx, 0.0, 0.0, s.bv)
        data += line
    data = shuffle_bytes(data, 10 * 4)
    comp_data = zlib.compress(data)

    ret = 'EPHE'
    ret += struct.pack('I', 2) # File version

    chunk = ''
    chunk += struct.pack('I', 3) # Star tile Version
    chunk += struct.pack('Q', nuniq)
    chunk += header

    chunk += struct.pack('I', len(data))
    chunk += struct.pack('I', len(comp_data))
    chunk += comp_data

    ret += 'STAR'
    ret += struct.pack('I', len(chunk))
    ret += chunk
    ret += struct.pack('I', 0) # CRC TODO

    with open(path, 'wb') as out:
        out.write(ret)

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
