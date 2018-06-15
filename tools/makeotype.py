#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


# Quick & dirty script.
# Download the Simbad object types list
# and generate the C code that can be put in src/otypes.c

import lxml.html
import re
import requests

# Get the data
URL = ("http://simbad.u-strasbg.fr/simbad/sim-display?"
       "data=otypes&option=display+numeric+codes")
html = requests.get(URL).content
html = lxml.html.fromstring(html)

def quote(x): return u'"{}"'.format(x)

lines = html.xpath('//tr')[1:]
for line in lines:
    cols = line.xpath(u'td//text()')
    cols = [x.strip() for x in cols if x.strip()]
    if not cols: continue
    n = [int(x) for x in cols[0].split('.')]
    name = cols[1]
    cond = cols[2]
    desc = cols[3] if len(cols) > 3 else ''
    ident = 0
    while name.startswith(u'\xb7'):
        ident += 1
        name = name[1:].strip()
    print u'T({:>2},{:>2},{:>2},{:>2}, {:<5}, {}{}, {})'.format(
            n[0], n[1], n[2], n[3], quote(cond), " " * ident * 2,
            quote(name), quote(desc)).encode('utf-8')
