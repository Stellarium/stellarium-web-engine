#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generates the symbols png image from the svg sources.

from math import *
import os
import os.path
import subprocess
import sys
import PIL.Image


if os.path.dirname(__file__) != "./tools":
    print("Should be run from root directory")
    sys.exit(-1)

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

def make_symbols():
    files = [
        'artificial-satellite.svg',
    ]
    dst = 'data/symbols.png'
    if check_uptodate(['data/symbols/{}'.format(x) for x in files], dst):
        return
    ret_img = PIL.Image.new('L', (128, 128))
    for i, src in enumerate(files):
        path = 'data/symbols/{}'.format(src)
        subprocess.check_output([
            'inkscape', path, '--export-area-page',
            '--export-width=32', '--export-height=32',
            '--export-png=/tmp/symbols.png'])
        img = PIL.Image.open('/tmp/symbols.png')
        img = img.split()[3]
        ret_img.paste(img, (32 * (i % 4), 32 * (i / 4)))
    ret_img.save(dst)

make_symbols()
