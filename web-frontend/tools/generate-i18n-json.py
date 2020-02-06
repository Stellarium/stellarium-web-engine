#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Update the translations JSON files for otypes and sky objects
# in public/i18n/otypes/ and public/i18n/skycultures/

import os
import json
import sys
import polib

if os.path.dirname(__file__) != "./tools":
    print("Should be run from the web-frontend directory")
    sys.exit(-1)

for fname in [d for d in os.listdir('../po/otypes/')]:
    print(fname)
    path = os.path.join('../po/otypes/', fname)
    lang = fname.split('.')[0]
    po_data = polib.pofile(path)
    json_data = {}
    for entry in po_data:
        if entry.msgstr:
            json_data[entry.msgid] = entry.msgstr
    with open('public/i18n/otypes/' + lang + '.json', 'w') as tr_file:
        json.dump(json_data, tr_file, ensure_ascii=False, indent=2)

for fname in [d for d in os.listdir('../po/skycultures/')]:
    print(fname)
    path = os.path.join('../po/skycultures/', fname)
    lang = fname.split('.')[0]
    po_data = polib.pofile(path)
    json_data = {}
    for entry in po_data:
        if entry.msgstr:
            json_data[entry.msgid] = entry.msgstr
    with open('public/i18n/skycultures/' + lang + '.json', 'w') as tr_file:
        json.dump(json_data, tr_file, ensure_ascii=False, indent=2)
