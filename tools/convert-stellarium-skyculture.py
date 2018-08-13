#!/usr/bin/python

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Script that converts stellarium skyculture file into SWE format.
# Optimally SWE should support stellarium skycultures directly, but we
# don't bundle as many stars by default, and so at least we need to change
# the constellation line and art star to use the closest bright star.
# EXPERIMENTAL.

# Usage:
#   convert-stellarium-skyculture <input_dir> <output_dir>

import collections
import gzip
import hashlib
from math import *
import os
import re
import requests
import requests_cache
import sys

assert len(sys.argv) == 3
input_dir = sys.argv[1]
output_dir = sys.argv[2]


Star = collections.namedtuple('Star', ['hip', 'hd', 'vmag', 'pos'])
Cst = collections.namedtuple('Cst', ['abb', 'name', 'lines'])

DD2R = pi / 180.0

# Map of HIP -> HD for some special cases.
specials = {
    55203: 98230
}

if os.path.dirname(__file__) != "./tools":
    print "Should be run from root directory"
    sys.exit(-1)

if not os.path.exists('data-src'): os.makedirs('data-src')
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

def s2c(theta, phi):
    """Spherical to cartesian"""
    cp = cos(phi)
    return (cos(theta) * cp, sin(theta) * cp, sin(phi))

def sep(s1, s2):
    """Angular separation between two stars"""
    p1 = s1.pos
    p2 = s2.pos
    axb = (p1[1] * p2[2] - p1[2] * p2[1],
           p1[2] * p2[0] - p1[0] * p2[2],
           p1[0] * p2[1] - p1[1] * p2[0])
    ss = sqrt(axb[0]**2 + axb[1]**2 + axb[2]**2)
    cs = p1[0] * p2[0] + p1[1] * p2[1] + p1[2] * p2[2]
    s = atan2(ss, cs) if (ss != 0.0 or cs != 0.0) else 0.0
    return s


# Download and parse the list of HIP stars.

print 'parse HIP stars'
all_stars = {}
hip_file = download(
        'http://cdsarc.u-strasbg.fr/ftp/pub/cats/I/239/hip_main.dat.gz',
        md5='7b50b051364b3f846ba8d6cdf81a4fcb')
for line in gzip.open(hip_file):
    hip = int(line[8:14])
    hd = int(line[390:396].strip() or 0)
    vmag = float(line[41:46].strip() or 'nan')
    ra = float(line[51:63].strip() or 'nan')
    de = float(line[64:76].strip() or 'nan')
    all_stars[hip] = Star(hip, hd, vmag, s2c(ra * DD2R, de * DD2R))

# Compute list of bright stars we can use in the data files.
bright_stars = [x for x in all_stars.values() if x.hd]
bright_stars = sorted(bright_stars, key=lambda x: x.vmag)[:10000]
bright_stars = {x.hip: x for x in bright_stars}


def find_closest_brigh_star(hip):
    """Return the closest bright star hd index around a given hip star"""
    if hip in bright_stars: return bright_stars[hip].hd
    if hip in specials: return specials[hip]
    s = all_stars[hip]
    assert s
    assert not isnan(s.pos[0])
    return sorted(bright_stars.values(), key=lambda x: sep(x, s))[0].hd

def make_lines(stars):
    """Turn list of [a1, b1, a2, b2, ....] into lines"""
    assert len(stars) % 2 == 0
    segs = [stars[i:i+2] for i in range(0, len(stars), 2)]

    while True:
        for i in range(len(segs) - 1):
            if segs[i][-1] == segs[i + 1][0]:
                segs[i] = segs[i][:-1] + segs[i + 1]
                del segs[i + 1]
                break
        else:
            break

    return segs

path = os.path.join(input_dir, 'star_names.fab')
out = open(os.path.join(output_dir, 'names.txt'), 'w')
print 'parse %s' % path
for line in open(path):
    line = line.strip()
    if not line: continue
    if line.startswith('#'):
        print >>out, line
        continue
    hip, name = re.match(r' *(\d+)\|_\("(.+)"\)', line).group(1, 2)
    hd = find_closest_brigh_star(int(hip))
    print >>out, 'HD %s | %s' % (hd, name)


csts = {}
comments = []
path = os.path.join(input_dir, 'constellationship.fab')
print 'parse %s' % path
for line in open(path):
    line = line.strip()
    if not line: continue
    if line.startswith('#'):
        comments.append(line)
        continue
    token = re.split(r' +', line.strip())
    abb = token[0]
    nb = int(token[1])
    stars = [int(x) for x in token[2:]]
    stars = stars[:nb * 2]
    # assert len(stars) == int(nb) * 2
    stars = [find_closest_brigh_star(x) for x in stars]
    lines = make_lines(stars)
    csts[abb.lower()] = Cst(abb, None, lines)

print 'read cst names'
path = os.path.join(input_dir, 'constellation_names.eng.fab')
for line in open(path):
    line = line.strip()
    if not line: continue
    if line.startswith('#'):
        comments.append(line)
        continue
    abb, native_name, english_name = \
            re.match(r'(.+?)[ \t]+"(.*)"[ \t]+_\("(.+)"\)', line) \
            .group(1, 2, 3)
    abb = abb.strip().lower()
    if abb in csts:
        csts[abb] = csts[abb]._replace(name=english_name)

out = open(os.path.join(output_dir, 'constellations.txt'), 'w')
print >>out, '\n'.join(comments)
for cst in sorted(csts.values(), key=lambda x: x.abb):
    lines = ' '.join('-'.join(str(x) for x in seg) for seg in cst.lines)
    print >>out, '%s|%s|%s' % (cst.abb, cst.name, lines)
