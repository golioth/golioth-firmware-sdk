#!/bin/bash

set -eu

if [[ $# -ne 2 ]]; then
    echo "$0: usage: infile outfile" >&2
    exit 2
fi

INFILE=$1
OUTFILE=$2

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$SCRIPT_DIR/../..
HEATSHRINK_PATH=$REPO_ROOT/external/heatshrink
START_DIR=$PWD

# Build heatshrink executable
cd $HEATSHRINK_PATH
make
cd $START_DIR

# Use heatshrink to compress
$HEATSHRINK_PATH/build/heatshrink -w 8 -l 4 $INFILE $OUTFILE

echo "Generated $OUTFILE"
