#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


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

# DEPRECATED: I try to move the data generation into separate scripts since
#             usually we only want to update one type of data.  You should
#             not have to generate the data since they are already bundled
#             in the src/assets/<something>.c files.

# Generate some of the data to be bundled in the sources.
# I guess we should split this in separate scripts for each type of data.
# For the moment this generates:
#
# - The cities list.
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


def make_symbols():
    files = [
        'pointer.svg',
        'artificial-satellite.svg',
    ]
    dst = 'data/symbols.png'
    if check_uptodate(['symbols/{}'.format(x) for x in files], dst):
        return
    ret_img = PIL.Image.new('L', (128, 128))
    for i, src in enumerate(files):
        path = 'symbols/{}'.format(src)
        subprocess.check_output([
            'inkscape', path, '--export-area-page',
            '--export-width=32', '--export-height=32',
            '--export-png=/tmp/symbols.png'])
        img = PIL.Image.open('/tmp/symbols.png')
        img = img.split()[3]
        ret_img.paste(img, (32 * (i % 4), 32 * (i / 4)))
    ret_img.save(dst)


make_cities()
make_symbols()
