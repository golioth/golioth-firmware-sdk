#!/bin/bash

set -e

verbose=0
show_files=0
AGAINST="HEAD"

# https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    --against)
    AGAINST="$2"
    shift # past argument
    ;;
    --verbose)
    verbose=1
    ;;
    --show-files)
    show_files=1
    ;;
    *)
    # unknown option
    ;;
esac
shift # past argument or value
done

# get commit
base_commit=$(git log -n 1 --pretty=format:"%H" "$AGAINST")

# all changed files
changed_files=$(git diff --diff-filter=d --name-only "$AGAINST")

# all changed .h and .c files
# Exclude: anything with "external" in the name
# || true at the end because grep exits with 1 when it doesn't find anything
changed_c_h_files=$(echo "$changed_files" \
    | grep -E "\.(c|h)$" \
    | grep -v -E ".*external.*" \
    | grep -v -E ".*unity.*" \
    | grep -v -E ".*fff.*" \
    | grep -v -E ".*cy_flash_map.h" \
    | tr '\n' ' ') || true

cd "$(git rev-parse --show-toplevel)"

if ((verbose)); then
    set +e
    git-clang-format --diff $base_commit -- $changed_c_h_files
    set -e
fi

if ((show_files)); then
    git-clang-format --diffstat $base_commit -- $changed_c_h_files
else
    git-clang-format --diffstat $base_commit -- $changed_c_h_files > /dev/null
fi
