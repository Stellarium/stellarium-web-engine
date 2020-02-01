#!/usr/bin/python3
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

import gzip

from utils import download

URL = 'https://minorplanetcenter.net/Extended_Files/mpcorb_extended.dat.gz'

def run():
    path = download(URL)
    lines = gzip.open(path, 'rt').readlines()
    lines = [x for x in lines if len(x) > 162]
    # Remove Pluto (we have it as a planet)
    lines = [x for x in lines if x[175:180] != 'Pluto']
    # Sort by magnitude.
    lines = sorted(lines, key=lambda x: float(x[8:14].strip() or 'inf'))
    # Keep only 500 first.
    lines = lines[:500]
    out = open("data/skydata/mpcorb.dat", "w")
    for line in lines:
        print(line, file=out)
    out.close()


if __name__ == '__main__':
    run()
