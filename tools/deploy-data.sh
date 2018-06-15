#!/bin/sh

# Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.


# Upload the data into noctua server.

TMPDIR=/tmp/dataout
DATE=`date --utc --rfc-3339=date`
RSYNC_FLAGS="-Cavz --no-p --no-g --chmod=ugo=rwX --delete"
DEST=noctua-software.com:/srv/stelladata/surveys

rm -rf $TMPDIR
mkdir $TMPDIR

echo Send stars data
mkdir $TMPDIR/stars
cp -rf data/stars/ $TMPDIR/stars/$DATE
cat << EOF > $TMPDIR/stars/info.json
{
    "type": "survey",
    "survey": {
        "type": "stars",
        "path": "$DATE/{l}/{y}/{y}_{x}.eph"
    }
}
EOF
rsync $RSYNC_FLAGS $TMPDIR/stars/ $DEST/stars
