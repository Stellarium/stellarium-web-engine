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
import sys
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
            if t == 's':
                if len(v) > col['size']:
                    print >>sys.stderr, 'String too long (%s)' % col['id']
                    print >>sys.stderr, d
                    assert False
                t = '%ds' % col['size']
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


def read_tile(path):
    f = open(path, 'rb')
    assert f.read(4) == 'EPHE'
    version = struct.unpack('I', f.read(4))[0]
    assert version == 2
    chunk_type, chunk_len = struct.unpack('4sI', f.read(8))
    chunk_version = f.read(4)[0]
    nuniq = struct.unpack('Q', f.read(8))
    _, row_size, nb_col, nb_sources = struct.unpack('iiii', f.read(16))
    cols = []
    for i in range(nb_col):
        id, type, unit, ofs, size = struct.unpack('4s4siii', f.read(20))
        id = id.strip('\0')
        type = type.strip('\0')
        cols.append(dict(id=id, type=type, unit=unit, ofs=ofs, size=size))
    data_len, comp_data_len = struct.unpack('II', f.read(8))
    comp_data = f.read(comp_data_len)
    data = zlib.decompress(comp_data)
    data = shuffle_bytes(data, nb_sources)
    ret = []
    for i in range(nb_sources):
        source = {}
        for col in cols:
            id = col['id']
            t = col['type']
            if t == 's': t = '%ds' % col['size']
            pos = i * row_size + col['ofs']
            source[id] = struct.unpack(t, data[pos: pos + col['size']])[0]
        ret.append(source)
    return ret
