#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


import os
import sys

def get_files():
    for dirname, dirnames, filenames in os.walk('./src'):
        for filename in filenames:
            if any(filename.endswith(x) for x in ['.h', '.c', '.cpp']):
                yield '%s/%s' % (dirname, filename)


def error(file, line, pos, msg):
    print('%s\n%s:%d\n%s' % (msg, file, pos + 1, line))
    sys.exit(1)


for file in get_files():
    check = True
    lines = open(file).readlines()
    for line_pos, line in enumerate(lines):
        if 'STYLE-CHECK OFF' in line: check = False
        if 'STYLE-CHECK ON' in line: check = True
        if not check: continue

        if line.endswith('\n'): line = line[:-1]

        if line.endswith(' '):
            error(file, line, line_pos, 'Trailing white space')
        if len(line) > 80:
            error(file, line, line_pos, 'Line too long')
        if '\t' in line:
            error(file, line, line_pos, 'Tab in code')
