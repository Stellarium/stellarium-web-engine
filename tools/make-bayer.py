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

from utils import download

Source = collections.namedtuple('Source',
        ['cst', 'bayer', 'bayer_n', 'vmag', 'hd'])

LETTERS = ("alf,bet,gam,del,eps,zet,eta,the,iot,kap,lam,mu.,nu.,ksi,omi,"
           "pi.,rho,sig,tau,ups,phi,chi,psi,ome").split(',')

CSTS = ("Aql And Scl Ara Lib Cet Ari Sct Pyx Boo Cae Cha Cnc Cap Car Cas "
        "Cen Cep Com CVn Aur Col Cir Crt CrA CrB Crv Cru Cyg Del Dor Dra "
        "Nor Eri Sge For Gem Cam CMa UMa Gru Her Hor Hya Hyi Ind Lac Mon "
        "Lep Leo Lup Lyn Lyr Ant Mic Mus Oct Aps Oph Ori Pav Peg Pic Per "
        "Equ CMi LMi Vul UMi Phe Psc PsA Vol Pup Ret Sgr Sco Ser Sex Men "
        "Tau Tel Tuc Tri TrA Aqr Vir Vel").split()

if os.path.dirname(__file__) != "./tools":
    print "Should be run from root directory"
    sys.exit(-1)


def encode_bin(data):
    ret = ""
    line = ""
    for i, c in enumerate(data):
        line += "{},".format(ord(c))
        if len(line) >= 70 or i == len(data) - 1:
            ret += "    " + line + "\n"
            line = ""
    return ret;

src = download('http://cdsarc.u-strasbg.fr/vizier/ftp/cats/IV/27A/catalog.dat',
               dest='data-src/IV_27A.dat',
               md5='7b51c0aa8255c6aaf261a1083e0f0bd8')

# Parse all entries.
sources = []
for line in open(src):
    hd = line[0:6].strip()
    bayer = line[68:73]
    cst = line[74:77]
    vmag = float(line[58:63].strip() or 'nan')
    if not hd or bayer[:3] not in LETTERS: continue
    hd = int(hd)
    assert bayer[:3] in LETTERS
    assert cst in CSTS
    assert not isnan(vmag)
    bayer_l = LETTERS.index(bayer[:3]) + 1
    bayer_n = int(bayer[3:5]) if bayer[3] != ' ' else 0
    cst = CSTS.index(cst) + 1
    sources.append(Source(cst, bayer_l, bayer_n, vmag, hd))
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
