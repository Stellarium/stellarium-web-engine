# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Eph file creation functions.

from math import *

import os
import struct
import zlib

# Unit constants: must be exactly the same as in src/eph-file.h!
UNIT_RAD             = 1 << 16
UNIT_VMAG            = 3 << 16
UNIT_ARCSEC          = 5 << 16 | 1 | 2 | 4
UNIT_RAD_PER_YEAR    = 7 << 16


def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)


def numpy_to_type(s):
    s = s.char
    if s == 'I': return 'i'
    if s == 'L': return 'Q'
    if s == 'f': return 'f'
    if s == 'S': return 's'
    raise ValueError('cannot convert numpy type %s' % s)


def shuffle_bytes(data, size):
    assert len(data) % size == 0
    ret = ''
    for i in range(size):
        for j in range(len(data) / size):
            ret += data[j * size + i]
    return ret


def float_trunc(v, zerobits):
    """Truncate a float value so that it can be better compressed"""
    # A float is represented in binary like this:
    # seeeeeeeefffffffffffffffffffffff
    mask = -(1L << zerobits)
    v = struct.unpack('I', struct.pack('f', v))[0]
    v &= mask
    return struct.unpack('f', struct.pack('I', v))[0]


def preprocess(data, infos):
    for name, inf in infos.items():
        zerobits = inf.get('zerobits', 0)
        if zerobits:
            for i in range(len(data)):
                data[i][name] = float_trunc(data[i][name], zerobits)


def create_tile(data, chunk_type, nuniq, path, infos={}):
    data = data[:]
    preprocess(data, infos)
    order = int(log(nuniq / 4, 2) / 2);
    pix = nuniq - 4 * (1 << (2 * order));
    path = '%s/Norder%d/Dir%d/Npix%d.eph' % (
            path, order, (pix / 10000) * 10000, pix)
    ensure_dir(path)
    row_size = data.nbytes / data.size
    # Header:
    header = ''
    header += struct.pack('iiii', 1, row_size, len(data.dtype), len(data))
    for d in data.dtype.fields.items():
        t = numpy_to_type(d[1][0])
        s = d[1][0].itemsize
        unit = infos.get(d[0], {}).get('unit', 0)
        header += struct.pack('4s4siii', d[0], t, unit, d[1][1], s)

    data = shuffle_bytes(data.data, row_size)
    comp_data = zlib.compress(data)

    ret = 'EPHE'
    ret += struct.pack('I', 2) # File version

    chunk = ''
    chunk += struct.pack('I', 3) # Tile Version
    chunk += struct.pack('Q', nuniq)
    chunk += header

    chunk += struct.pack('I', len(data))
    chunk += struct.pack('I', len(comp_data))
    chunk += comp_data

    ret += chunk_type
    ret += struct.pack('I', len(chunk))
    ret += chunk
    ret += struct.pack('I', 0) # CRC TODO

    with open(path, 'wb') as out:
        out.write(ret)
