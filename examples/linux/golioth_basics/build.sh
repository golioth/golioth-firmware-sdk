#!/usr/bin/env bash

set -Eeuo pipefail
mkdir -p build
cd build
cmake ..
make -j8
