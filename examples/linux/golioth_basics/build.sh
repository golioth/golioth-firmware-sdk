#!/usr/bin/env bash

set -Eeuo pipefail
mkdir -p build
cd build
# cmake ..
cmake -D CMAKE_FIND_DEBUG_MODE=ON ..
make -j8
