#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Some utils functions that can be used by the other scripts.

import functools
import gzip
import hashlib
import math
import os
import requests
import struct
import sys

# Degrees to radians
DD2R = 1.745329251994329576923691e-2

# Radians to degrees
DR2D = 57.29577951308232087679815


# Make sure that all the scripts are run from the root dir.
if not os.path.relpath(__file__).startswith('tools/'):
    print('Should be run from root directory')
    sys.exit(-1)


def ensure_dir(file_path):
    '''Create a directory for a path if it doesn't exists yet'''
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def download(url, dest=None, sha256=None, md5=None, unpacked_md5=False, headers=None):
    '''download a file into data-src and return a path to it'''
    dest = dest or 'build/data-src/'
    if dest.endswith('/'):
        dest = os.path.join(dest, os.path.basename(url))
    if not os.path.exists(dest):
        ensure_dir(dest)
        print("Download '{}'".format(url))
        r = requests.get(url, headers=headers)
        with open(dest, 'wb') as out:
            out.write(r.content)
    if md5:
        assert hashlib.md5(open(dest, 'rb').read()).hexdigest() == md5
    if sha256:
        assert hashlib.sha256(open(dest, 'rb').read()).hexdigest() == sha256
    if unpacked_md5:
        data_md5 = hashlib.md5(gzip.open(dest).read()).hexdigest()
        assert data_md5 == unpacked_md5
    return dest


def parse(line, start, end, type=float, default=None, required=False,
          conv=None):
    '''parse a part of a line'''
    line = line[start - 1:end].strip()
    if not line:
        assert not required
        return default
    ret = type(line)
    if conv is not None: ret *= conv
    return ret


def shuffle_bytes(data, size):
    assert len(data) % size == 0
    ret = ''
    for i in range(size):
        for j in range(len(data) / size):
            ret += data[j * size + i]
    return ret

def compute_dir_md5(path):
    """Compute the md5 of all the files and dir of a directory
       The computation takes into account both the files content and the
       directories and file names.
    """
    m = hashlib.md5()
    for dirpath, dirnames, filenames in os.walk(path):
        dirnames[:] = sorted(dirnames)
        filenames[:] = sorted(filenames)
        for d in dirnames:
            m.update(d.encode('utf8'))
        for f in filenames:
            m.update(open(os.path.join(dirpath, f), 'rb').read())
    return m.hexdigest()


def compute_file_md5(path):
    """Compute the md5 of a file or a directory"""
    if not path.endswith('/'):
        return hashlib.md5(open(path, 'rb').read()).hexdigest()
    else:
        return compute_dir_md5(path)


def generator(target, md5):
    """Decorator that checks if a generated file is already up to date
       This can be used to cache data that is slow to generate.
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(abort_on_wrong_md5=True):
            path = target
            if not path.startswith('./'):
                path = os.path.join('build/data-src', path)
            if not os.path.exists(path):
                print('Generating %s' % path)
                ensure_dir(path)
                func(path)
            current_md5 = compute_file_md5(path)
            if current_md5 != md5:
                print('Md5 for file %s changed!' % target)
                print('Current md5: %s' % current_md5)
                print('Expected   : %s' % md5)
                if abort_on_wrong_md5:
                    raise ValueError
            return path
        return wrapper
    return decorator


def s2c(theta, phi):
   cp = math.cos(phi)
   return (math.cos(theta) * cp, math.sin(theta) * cp, math.sin(phi))


def sepp(a, b):
   axb = (a[1] * b[2] - a[2] * b[1],
          a[2] * b[0] - b[0] * b[2],
          a[0] * b[1] - a[1] * b[0])
   ss = math.sqrt(axb[0]**2 + axb[1]**2 + axb[2]**2)
   cs = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
   s = math.atan2(ss, cs) if ss != 0.0 or cs != 0.0 else 0.0
   return s


def seps(al, ap, bl, bp):
    '''Angular separation between two sets of spherical coordinates'''
    a = s2c(al, ap)
    b = s2c(bl, bp)
    return sepp(a, b)
