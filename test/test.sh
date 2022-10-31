#!/usr/bin/env bash

set -Eeuo pipefail

# Build
mkdir -p build
cd build
cmake ..
make -j8

# Code coverage
LCOV_ENABLED=${CODECOV:-0}

if [ "$LCOV_ENABLED" -eq 1 ]; then
    pushd ..

    # Cleanup lcov from prior run
    lcov \
        --directory . \
        --base-directory . \
        --zerocounters

    lcov \
        -c -i \
        --directory . \
        --base-directory . \
        -o build/base.info

    popd
fi

# Run tests
ctest \
    --output-on-failure \
    --timeout 2 \
    $@

if [ "$LCOV_ENABLED" -eq 1 ]; then
    pushd ..

    # Create test coverage data file
    lcov \
        --directory . \
        --base-directory . \
        -c \
        -o build/run.info

    # Combine baseline and test coverage data
    lcov -a build/base.info -a build/run.info -o build/total.info

    # Filter out unwanted stuff
    lcov \
        --remove build/total.info \
        '*test/*' \
        '*unity/*' \
        '*fff/*' \
        -o build/total_filtered.info

    # Generate HTML report
    genhtml -o build/lcov build/total_filtered.info
    echo "Coverage report generated: build/lcov/index.html."

    popd
fi


