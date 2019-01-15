#!/usr/bin/python
# coding: utf-8

# Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Generate the default font used in the html version.

import fontforge

from utils import ensure_dir

def run():
    ensure_dir('data/font/')
    path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    dst = "data/font/DejaVuSans-small.ttf"
    chars = (u"abcdefghijklmnopqrstuvwxyz"
             u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             u"0123456789"
             u" ?!\"#$%&'()*+,-./°¯[]:<>{}"
             u"☉☿♀♁♂♃♄⛢♆⚳⚴⚵⚶🍷⚘⚕♇"
             u"αβγδεζηθικλμνξοπρςστυφχψω")

    font = fontforge.open(path)
    for g in font:
        u = font[g].unicode
        if u == -1: continue
        u = unichr(u)
        if u not in chars: continue
        font.selection[ord(u)] = True

    font.selection.invert()
    for i in font.selection.byGlyphs:
        font.removeGlyph(i)

    font.generate(dst)

if __name__ == '__main__':
    run()
