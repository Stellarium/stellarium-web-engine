#!/usr/bin/python3

# Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

# Update the english translation file in web-engine/src/locale/en.json
# This file can be used as a base for translation in other laguages

import os
import json
import sys

if os.path.dirname(__file__) != "./tools":
    print("Should be run from the web-frontend directory")
    sys.exit(-1)


def update_tr(src_path, locales_path, exclude_path=None):
    os.system('docker run -it -v "$PWD:/app" -v "$PWD/../data:/data" stellarium-web-dev yarn run i18n --src "' +
              src_path + '/*.?(js|vue)" --locales "' +
              locales_path + '/*.json" --output "i18n-report.json"')
    report = {}
    with open('i18n-report.json') as report_file:
        report = json.load(report_file)

    trs = {}
    # Loads existing english translations
    with open(locales_path + '/en.json') as tr_file:
        trs = json.load(tr_file)

    for tr in [x for x in report['missingKeys'] if x['language'] == 'en']:
        if exclude_path and tr['file'].startswith(exclude_path):
            continue
        key = tr['path']
        # Add missing translations
        print('Add missing: ' + key)
        trs[key] = key

    # Saves new version
    with open(locales_path + '/en.json', 'w') as tr_file:
        json.dump(trs, tr_file, ensure_ascii=False, indent=2)
    os.remove("i18n-report.json")


update_tr('./src/**', './src/locales', '/src/plugins/')

for plugin in [d for d in os.listdir('./src/plugins') if
               os.path.isdir(os.path.join('./src/plugins', d))]:
    update_tr('./src/plugins/' + plugin + '/**', './src/plugins/' + plugin +
              '/locales')
