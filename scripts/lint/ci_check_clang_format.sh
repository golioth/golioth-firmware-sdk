#!/bin/bash
## check changed files for clang-format violations
## exit with error if changed files (against main) are ill-formatted

## This script is used in CI. For running locally, you want check_clang_format.sh.

set -euxo pipefail

cd "$(dirname "$0")"

MERGE_BASE=origin/main

# https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    --merge-base)
    MERGE_BASE="$2"
    shift # past argument
    ;;
    *)
    # unknown option
    ;;
esac
shift # past argument or value
done

set +e
./check_clang_format.sh --against "$MERGE_BASE" --verbose --show-files
status=$?

if [[ $status -ne 0 ]]
then
    echo "!! clang-format check failed"
    echo "!! Please run clang-format to fix the formatting of your changed files"
    exit 1
else
    echo -e "clang-format check succeeded."
fi
