#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


import os
import re
import struct
import sys
import zlib

SOURCE = "data"
DEST = "src/assets/"

if len(sys.argv) > 1:
    SOURCE, DEST = sys.argv[1:]

if os.path.dirname(__file__) != "./tools":
    print "Should be run from root directory"
    sys.exit(-1)

TYPES = {
    "png": {"text": False, "compress": False},
    "jpg": {"text": False, "compress": False},
    "txt": {"text": True,  "compress": False},
    "dat": {"text": False, "compress": True},
    "ttf": {"text": False, "compress": True},
    "eph": {"text": False, "compress": True},
    "gz":  {"text": False, "compress": False},
    "ini": {"text": True,  "compress": False},
    "vert": {"text": True,  "compress": False},
    "frag": {"text": True,  "compress": False},
}

def list_data_files():
    for root, dirs, files in os.walk(SOURCE):
        for f in sorted(files, key=lambda x: x.upper()):
            if any(f.endswith(x) for x in TYPES.keys()):
                p = os.path.join(root, f)
                # Limit hips surveys to level 1
                m = re.match(r'^.+/Norder(\d+)/.+\.eph', p)
                if m and int(m.group(1)) > 1: continue
                yield p

def encode_str(data):
    ret = '    "'
    for c in data:
        if c == '\n':
            ret += '\\n"\n    "'
            continue
        if c == '"': c = '\\"'
        if c == '\\': c = '\\\\'
        ret += c
    ret += '"'
    return ret

def encode_bin(data):
    ret = "{\n"
    line = ""
    for i, c in enumerate(data):
        line += "{},".format(ord(c))
        if len(line) >= 70 or i == len(data) - 1:
            ret += "    " + line + "\n"
            line = ""
    ret += "}"
    return ret;

# Get all the asset files sorted by group:
groups = {}
for f in list_data_files():
    group = os.path.relpath(f, SOURCE).split('/')[0]
    groups.setdefault(group, []).append(f)

for group in groups:
    out = open(os.path.join(DEST, "%s.inl" % group), "w")
    print >>out, "// Auto generated from tools/makeassets.py\n"
    for f in groups[group]:
        data = open(f).read()
        type = TYPES[f.split(".")[-1]]
        size = len(data)
        compressed = False
        if type["compress"]:
            data = zlib.compress(data, 9)
            data = struct.pack('I', size) + data
            compressed = True
        size = len(data)

        if type["text"]:
            size += 1 # NULL terminated string.
            data = encode_str(data)
        else:
            data = encode_bin(data)

        name = f.replace('.', '_').replace('-', '_').replace('/', '_')
        url = f[len('data/'):]

        print >>out, ("static const unsigned char DATA_{}[{}] "
                      "__attribute__((aligned(4))) =\n{};\n").format(
                              name, size, data)
        print >>out, 'ASSET_REGISTER({name}, "{url}", DATA_{name}, {comp})' \
                        .format(name=name, url=url,
                                comp='true' if compressed else 'false')
        print >>out
