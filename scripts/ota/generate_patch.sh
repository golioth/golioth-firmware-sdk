#!/bin/bash

set -eu

if [[ $# -ne 3 ]]; then
    echo "$0: usage: oldfile newfile patchfile" >&2
    exit 2
fi

OLDFILE=$1
NEWFILE=$2
PATCHFILE=$3

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$SCRIPT_DIR/../..
BSDIFF_PATH=$REPO_ROOT/external/bsdiff
HEATSHRINK_PATH=$REPO_ROOT/external/heatshrink
START_DIR=$PWD

# Build bsdiff executable
cd $BSDIFF_PATH
gcc -O2 -DBSDIFF_EXECUTABLE -o bsdiff bsdiff.c
cd $START_DIR

# Build heatshrink executable
cd $HEATSHRINK_PATH
make
cd $START_DIR

# Generate the raw/uncompressed patch file
$BSDIFF_PATH/bsdiff $OLDFILE $NEWFILE $PATCHFILE.temp

# Use heatshrink to compress the patch
$HEATSHRINK_PATH/build/heatshrink -w 8 -l 4 $PATCHFILE.temp $PATCHFILE.hs
echo "Generated patch with heatshrink compression: $PWD/$PATCHFILE.hs"

# Use python zlib to compress
python -c "import zlib; import sys; open(sys.argv[2], 'wb').write(zlib.compress(open(sys.argv[1], 'rb').read(), 9))" \
  $PATCHFILE.temp $PATCHFILE.z
echo "Generated patch with zlib compression: $PWD/$PATCHFILE.z"

rm $PATCHFILE.temp
