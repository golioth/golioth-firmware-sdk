#!/bin/bash

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$THIS_DIR/../.."

find $REPO_ROOT/src -type f -name "*.c" -o -name "*.h" \
    | xargs clang-format -i -style=file --verbose

find $REPO_ROOT/port -type f -name "*.c" -o -name "*.h" \
    | xargs clang-format -i -style=file --verbose

find $REPO_ROOT/examples/common -type f -name "*.c" -o -name "*.h" \
    | grep -v -E ".*build.*" \
    | xargs clang-format -i -style=file --verbose

find $REPO_ROOT/examples/esp_idf/common -type f -name "*.c" -o -name "*.h" \
    | grep -v -E ".*build.*" \
    | xargs clang-format -i -style=file --verbose

find $REPO_ROOT/examples/esp_idf/*/main -type f -name "*.c" -o -name "*.h" \
    | grep -v -E ".*build.*" \
    | xargs clang-format -i -style=file --verbose

find $REPO_ROOT/examples/modus_toolbox/golioth_basics/golioth_app/source -type f -name "*.c" -o -name "*.h" \
    | grep -v -E ".*build.*" \
    | xargs clang-format -i -style=file --verbose
