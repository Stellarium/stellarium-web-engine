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

import struct
import sys
import re

LETTERS = ("Alp,Bet,Gam,Del,Eps,Zet,Eta,The,Iot,Kap,Lam,Mu ,Nu ,Xi ,Omi,"
           "Pi ,Rho,Sig,Tau,Ups,Phi,Chi,Psi,Ome").split(',')

CSTS = ("Aql And Scl Ara Lib Cet Ari Sct Pyx Boo Cae Cha Cnc Cap Car Cas "
        "Cen Cep Com CVn Aur Col Cir Crt CrA CrB Crv Cru Cyg Del Dor Dra "
        "Nor Eri Sge For Gem Cam CMa UMa Gru Her Hor Hya Hyi Ind Lac Mon "
        "Lep Leo Lup Lyn Lyr Ant Mic Mus Oct Aps Oph Ori Pav Peg Pic Per "
        "Equ CMi LMi Vul UMi Phe Psc PsA Vol Pup Ret Sgr Sco Ser Sex Men "
        "Tau Tel Tuc Tri TrA Aqr Vir Vel").split()

r = re.compile(r'^.+?\t(\d+?)\D*\t(.+)')
out = open("bayer.bin", "wb")

for line in open(sys.argv[1]):
    hd = line[25:31].strip()
    name = line[5:14]
    if not hd or name[2] == ' ': continue
    hd = int(hd)
    assert name[2:5] in LETTERS
    assert name[6:9] in CSTS
    bayer = LETTERS.index(name[2:5]) + 1
    bayer_n = int(name[5]) if name[5] != ' ' else 0
    cst = CSTS.index(name[6:9]) + 1
    out.write(struct.pack("IBBBB", hd, cst, bayer, bayer_n, 0))
