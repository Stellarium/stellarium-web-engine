#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


import io
import os
import re
import struct
import sys
import zlib

ROOT = "data"
SOURCES = [
    "font/",
    "planets.ini",
    "shaders/",
    "symbols.png",
    "textures/"
]
DEST = "src/assets/"

if len(sys.argv) > 1:
    ROOT = sys.argv[1]
    DEST = sys.argv[2]

if os.path.dirname(__file__) != "./tools":
    print("Should be run from root directory")
    sys.exit(-1)

TYPES = {
    "png": {"text": False, "compress": False},
    "jpg": {"text": False, "compress": False},
    "webp": {"text": False, "compress": False},
    "txt": {"text": True,  "compress": False},
    "dat": {"text": False, "compress": True},
    "ttf": {"text": False, "compress": True},
    "eph": {"text": False, "compress": True},
    "gz":  {"text": False, "compress": False},
    "ini": {"text": True,  "compress": False},
    "vert": {"text": True,  "compress": False},
    "frag": {"text": True,  "compress": False},
    "glsl": {"text": True,  "compress": False},
    "html": {"text": True,  "compress": False},
    "utf8": {"text": True,  "compress": False},
    "fab": {"text": True,  "compress": False},
    "json": {"text": True,  "compress": False},
    "properties": {"text": True,  "compress": False},
}

def list_data_files():
    for src in SOURCES:
        if os.path.isfile(os.path.join(ROOT, src)):
            yield src
            continue
        for root, dirs, files in os.walk(os.path.join(ROOT, src)):
            for f in sorted(files, key=lambda x: x.upper()):
                if any(f.endswith(x) for x in TYPES.keys()):
                    p = os.path.join(root, f)
                    yield os.path.relpath(p, ROOT)
            dirs[:] = sorted(dirs)

def encode_str(data):
    assert isinstance(data, bytes)
    ret = '    "'
    for c in data.decode('utf8'):
        if c == '\n':
            ret += '\\n"\n    "'
            continue
        if c == '"': c = '\\"'
        if c == '\\': c = '\\\\'
        ret += c
    ret += '"'
    return ret

def encode_bin(data):
    ret = '{\n'
    line = ''
    for i, c in enumerate(data):
        line += '{},'.format(c)
        if len(line) >= 70 or i == len(data) - 1:
            ret += '    ' + line + '\n'
            line = ''
    ret += '}'
    return ret

# Get all the asset files sorted by group:
groups = {}
for f in list_data_files():
    group = f.split('/')[0]
    groups.setdefault(group, []).append(f)

for group in groups:
    out = io.StringIO()
    print("// Auto generated from tools/makeassets.py\n", file=out)
    for f in groups[group]:
        data = open(os.path.join(ROOT, f), 'rb').read()
        data_type = TYPES[os.path.basename(f).split(".")[-1]]
        size = len(data)
        compressed = False
        if data_type["compress"]:
            data = zlib.compress(data, 9)
            data = struct.pack('I', size) + data
            compressed = True
        size = len(data)

        if data_type["text"]:
            size += 1 # NULL terminated string.
            data = encode_str(data)
        else:
            data = encode_bin(data)

        name = f.replace('.', '_').replace('-', '_').replace('/', '_')

        print("static const unsigned char DATA_{}[{}] "
              "__attribute__((aligned(4))) =\n{};\n"
              .format(name, size, data), file=out)

        print('ASSET_REGISTER({name}, "{url}", DATA_{name}, {comp})'
              .format(name=name, url=f,
                      comp='true' if compressed else 'false'), file=out)
        print(file=out)

    # Only write the data if it has changed, so that we don't change the
    # timestamp of the files unnecessarily.
    path = os.path.join(DEST, "%s.inl" % group)
    if not os.path.exists(path) or open(path).read() != out.getvalue():
        open(path, 'w').write(out.getvalue())
