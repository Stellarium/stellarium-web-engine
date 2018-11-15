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


def col_get_size(col):
    if 'size' in col: return col['size']
    t = col['type']
    if t in ['i', 'f']: return 4
    if t in ['Q']: return 8
    assert False


def create_tile(data, chunk_type, nuniq, path, columns):
    order = int(log(nuniq / 4, 2) / 2);
    pix = nuniq - 4 * (1 << (2 * order));
    path = '%s/Norder%d/Dir%d/Npix%d.eph' % (
            path, order, (pix / 10000) * 10000, pix)
    ensure_dir(path)
    row_size = sum(col_get_size(x) for x in columns)
    # Header:
    header = ''
    header += struct.pack('iiii', 1, row_size, len(columns), len(data))
    ofs = 0
    for col in columns:
        t = col['type']
        s = col_get_size(col)
        unit = col.get('unit', 0)
        header += struct.pack('4s4siii', col['id'], t, unit, ofs, s)
        ofs += s

    # Create packed binary data table.
    buf = ''
    for d in data:
        if hasattr(d, '_asdict'): d = d._asdict() # For named tuple.
        for col in columns:
            v = d[col['id']]
            t = col['type']
            if t == 's': t = '%ds' % col['size']
            zerobits = col.get('zerobits', 0)
            if zerobits: v = float_trunc(v, zerobits)
            buf += struct.pack(t, v)
    data = shuffle_bytes(buf, row_size)

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
