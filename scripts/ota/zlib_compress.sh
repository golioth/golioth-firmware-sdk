#!/bin/bash

set -eu

if [[ $# -ne 2 ]]; then
    echo "$0: usage: infile outfile" >&2
    exit 2
fi

INFILE=$1
OUTFILE=$2

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Use python zlib to compress
python -c "import zlib; import sys; open(sys.argv[2], 'wb').write(zlib.compress(open(sys.argv[1], 'rb').read(), 9))" \
  $INFILE $OUTFILE

echo "Generated $OUTFILE"
