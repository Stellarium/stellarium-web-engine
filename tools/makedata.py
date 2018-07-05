#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


import fontforge
import codecs
import csv
import gzip
import hashlib
import json
from math import *
import os
import os.path
import re
import requests
import requests_cache
import struct
import subprocess
import sys
import unicodedata
import zipfile
import zlib
import PIL.Image

# Generate some of the data to be bundled in the sources.
# I guess we should split this in separate scripts for each type of data.
# For the moment this generates:
#
# - The cities list.
# - The font.
# - The symbols.
# - Some minor planet sources.


if os.path.dirname(__file__) != "./tools":
    print "Should be run from root directory"
    sys.exit(-1)

def create_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)

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

def check_uptodate(src, dst):
    """Check if any file in dst is older than any file file in src"""
    if isinstance(src, str): src = [src]
    if isinstance(dst, str): dst = [dst]
    for d in dst:
        if not os.path.exists(d): return False
        for s in src:
            if os.path.getmtime(d) < os.path.getmtime(s):
                return False
    return True

create_dir('data-src')
requests_cache.install_cache('data-src/cache')

def make_cities():
    out = codecs.open('data/cities.txt', "w", "utf-8")
    path = download('http://download.geonames.org/export/dump/cities15000.zip')
    zp = zipfile.ZipFile(path, 'r')
    f = zp.open('cities15000.txt')
    for line in f:
        line = line.decode('utf-8')
        (geonameid, name, asciiname, alternatenames, latitude, longitude,
         feature_class, feature_code, country_code, cc2, admin1_code,
         admin2_code, admin3_code, admin4_code, population, elevation,
         dem, timezone, modification_date) = line.split('\t')
        if int(population) < 500000: continue
        line = (u"{name}\t{asciiname}\t{latitude}\t{longitude}\t{elevation}\t"
                u"{country_code}\t{timezone}").format(**locals())
        print >>out, line
    out.close()

def make_font():
    create_dir('data/font')
    path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    dst = "data/font/DejaVuSans-small.ttf"
    chars = (u"abcdefghijklmnopqrstuvwxyz"
             u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             u"0123456789"
             u" ?!\"#$%&'()*+,-./°¯[]:<>{}"
             u"☉☿♀♁♂♃♄⛢♆⚳⚴⚵⚶🍷⚘⚕♇"
             u"αβγδεζηθικλμνξοπρςστυφχψω")
    if check_uptodate(path, dst): return

    font = fontforge.open(path)
    for g in font:
        u = font[g].unicode
        if u == -1: continue
        u = unichr(u)
        if u not in chars: continue
        font.selection[ord(u)] = True

    font.selection.invert()
    for i in font.selection.byGlyphs:
        font.removeGlyph(i)

    font.generate(dst)

def make_symbols():
    files = [
        'pointer.svg',
        'pointer2.svg',

        'neb-open-star-cluster.svg',
        'neb-globular-star-cluster.svg',
        'neb-galaxy.svg',
        'neb-planetary-nebula.svg',
        'neb-default.svg',
        'neb-bright-emission-nebula.svg',
        'neb-cluster-nebula.svg',

        'btn-atmosphere.svg',
        'btn-cst-lines.svg',
        'btn-landscape.svg',
        'btn-equatorial-grid.svg',
        'btn-azimuthal-grid.svg',
        'btn-nebulae.svg',
    ]
    dst = 'data/symbols.png'
    if check_uptodate(['symbols/{}'.format(x) for x in files], dst):
        return
    ret_img = PIL.Image.new('L', (1024, 1024))
    for i, src in enumerate(files):
        path = 'symbols/{}'.format(src)
        subprocess.check_output([
            'inkscape', path, '--export-area-page',
            '--export-width=128', '--export-height=128',
            '--export-png=/tmp/symbols.png'])
        img = PIL.Image.open('/tmp/symbols.png')
        img = img.split()[3]
        ret_img.paste(img, (128 * (i % 8), 128 * (i / 8)))
    ret_img.save(dst)

def make_mpc():
    path = download('http://www.minorplanetcenter.net/iau/MPCORB/MPCORB.DAT.gz')
    f = gzip.GzipFile(path)
    it = iter(f)
    # Skip header
    while not it.next().startswith('--------------'): pass
    lines = list(it)
    # Sort by magnitude.
    lines = sorted(lines, key=lambda x: float(x[8:14].strip() or 'inf'))
    # Keep only 500 first.
    lines = lines[:500]

    out = open("data/mpcorb.dat", "w")
    for line in lines:
        print >>out, line.rstrip()
    out.close()

make_cities()
make_font()
make_symbols()
make_mpc()
