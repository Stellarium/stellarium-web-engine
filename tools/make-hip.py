#!/usr/bin/python3
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generate the file hip.inl, that contains a mapping of HIP -> healpix for
# fast HIP stars lookup.

from math import *

import csv
import healpy
import sys

sys.path.append('./tools')
from utils import generator

def hms_to_deg(hms):
    if hms.startswith("b'"): hms = hms[2:-2] # For gaia python3 bug.
    hms = [float(x) for x in hms.split()]
    return hms[0] * 15 + hms[1] * 15 / 60 + hms[2] * 15 / 60 / 60

def dms_to_deg(dms):
    if dms.startswith("b'"): dms = dms[2:-2] # For gaia python3 bug.
    dms = [float(x) for x in dms.split()]
    return dms[0] + dms[1] / 60 + dms[2] / 60 / 60

@generator('hip.csv', md5='f2f11a8f3f8ccae13f6eabf331b4e4db')
def get_hip_data_file(path):
    '''Get HIP data from gaia TAP server'''
    # We use the hipparcos new reduction table, as it is supposed to have
    # better values.
    from astroquery.gaia import Gaia
    QUERY = '''
        SELECT hip, ra, de, vmag, rahms, dedms
        FROM public.hipparcos
        ORDER BY hip
    '''
    job = Gaia.launch_job_async(QUERY)
    r = job.get_results()
    r.write(path, overwrite=True, format='ascii.csv')
    if not hasattr(job, 'get_jobid'):
        print('Warning: cannot delete Gaia job')
        return
    Gaia.remove_jobs([job.get_jobid()])

@generator('hip_newreduction.csv', md5='b0b86439386bbcbae15efc402ed06da7')
def get_hip_newreduction_data_file(path):
    '''Get HIP data from gaia TAP server'''
    # We use the hipparcos new reduction table, as it is supposed to have
    # better values.
    from astroquery.gaia import Gaia
    QUERY = '''
        SELECT hip, ra, dec, hp_mag AS vmag
        FROM public.hipparcos_newreduction
        ORDER BY hip
    '''
    job = Gaia.launch_job_async(QUERY)
    r = job.get_results()
    r.write(path, overwrite=True, format='ascii.csv')
    if not hasattr(job, 'get_jobid'):
        print('Warning: cannot delete Gaia job')
        return
    Gaia.remove_jobs([job.get_jobid()])

def run():
    htable = {}

    data = list(csv.DictReader(open(get_hip_newreduction_data_file())))
    for d in data:
        hip = int(d['hip'])
        ra = float(d['ra'])
        de = float(d['dec'])
        pix = healpy.ang2pix(4, ra, de, nest=True, lonlat=True)
        htable[hip] = pix

    # Add the stars missing from Hipparcos New Reduction!
    # XXX: would be nice to be able to load them all in a single SQL query.
    data = list(csv.DictReader(open(get_hip_data_file())))
    for d in data:
        hip = int(d['hip'])
        if hip in htable: continue
        ra = hms_to_deg(d['rahms'])
        de = dms_to_deg(d['dedms'])
        pix = healpy.ang2pix(4, ra, de, nest=True, lonlat=True)
        htable[hip] = pix

    data = [htable.get(x, 255) for x in range(max(htable.keys()))]

    out = open('src/hip.inl', 'w')
    print('// HIP -> pix pos at order 2.', file=out)
    print('// Generated by tools/make-hip.py', file=out)

    for i, v in enumerate(data):
        if (i % 15): print(' ', file=out, end='')
        print('%3d,' % v, file=out, end='')
        if (i + 1) % 15 == 0: print(file=out)

if __name__ == '__main__':
    run()
