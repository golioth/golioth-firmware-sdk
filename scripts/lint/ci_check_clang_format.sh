#!/bin/bash
## check changed files for clang-format violations
## exit with error if changed files (against main) are ill-formatted

## This script is used in CI. For running locally, you want check_clang_format.sh.

set -euxo pipefail

cd "$(dirname "$0")"

merge_base=origin/main

set +e
./check_clang_format.sh --against "$merge_base" --verbose --show-files
status=$?

if [[ $status -ne 0 ]]
then
    echo "!! clang-format check failed"
    echo "!! Please run clang-format to fix the formatting of your changed files"
    exit 1
else
    echo -e "clang-format check succeeded."
fi
