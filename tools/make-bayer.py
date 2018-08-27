#!/usr/bin/python

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


# Simple script that takes as input the files from 
#   http://heasarc.gsfc.nasa.gov/W3Browse/star-catalog/bsc5p.html
# and generate a data file in binary format as a list of 8 bytes entries:
#   hip (4 bytes) n (4 bytes)

from math import *
import collections
import hashlib
import gzip
import os
import requests
import requests_cache
import struct
import sys
import zlib

Source = collections.namedtuple('Source',
        ['cst', 'bayer', 'bayer_n', 'vmag', 'hd'])

LETTERS = ("Alp,Bet,Gam,Del,Eps,Zet,Eta,The,Iot,Kap,Lam,Mu ,Nu ,Xi ,Omi,"
           "Pi ,Rho,Sig,Tau,Ups,Phi,Chi,Psi,Ome").split(',')

CSTS = ("Aql And Scl Ara Lib Cet Ari Sct Pyx Boo Cae Cha Cnc Cap Car Cas "
        "Cen Cep Com CVn Aur Col Cir Crt CrA CrB Crv Cru Cyg Del Dor Dra "
        "Nor Eri Sge For Gem Cam CMa UMa Gru Her Hor Hya Hyi Ind Lac Mon "
        "Lep Leo Lup Lyn Lyr Ant Mic Mus Oct Aps Oph Ori Pav Peg Pic Per "
        "Equ CMi LMi Vul UMi Phe Psc PsA Vol Pup Ret Sgr Sco Ser Sex Men "
        "Tau Tel Tuc Tri TrA Aqr Vir Vel").split()

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
    r = requests.get(url)
    if md5 and hashlib.md5(r.content).hexdigest() != md5:
        print "Wrong md5 for %s" % url
        sys.exit(-1)
    with open(outpath, 'wb') as out:
        out.write(r.content)
    return outpath

def encode_bin(data):
    ret = ""
    line = ""
    for i, c in enumerate(data):
        line += "{},".format(ord(c))
        if len(line) >= 70 or i == len(data) - 1:
            ret += "    " + line + "\n"
            line = ""
    return ret;

src = download('http://cdsarc.u-strasbg.fr/vizier/ftp/cats/V/50/catalog.gz',
               md5='2f0662e53aa4e563acb8b2705f45c1a3')

# Parse all entries.
sources = []
for line in gzip.open(src):
    hd = line[25:31].strip()
    name = line[5:14]
    vmag = float(line[102:107].strip() or 'nan')
    if not hd or name[2] == ' ': continue
    hd = int(hd)
    assert name[2:5] in LETTERS
    assert name[6:9] in CSTS
    assert not isnan(vmag)
    bayer = LETTERS.index(name[2:5]) + 1
    bayer_n = int(name[5]) if name[5] != ' ' else 0
    cst = CSTS.index(name[6:9]) + 1
    sources.append(Source(cst, bayer, bayer_n, vmag, hd))
sources = sorted(sources)

data = ''
for s in sources:
    data += struct.pack("IBBBB", s.hd, s.cst, s.bayer, s.bayer_n, 0)

data = struct.pack('I', len(data)) + zlib.compress(data, 9)

print 'static const unsigned char DATA[%d]' % len(data),
print '__attribute__((aligned(4))) = {'
print '    %s, // uncompressed size' % ','.join(str(ord(x)) for x in data[:4])
print encode_bin(data[4:]),
print '};'
