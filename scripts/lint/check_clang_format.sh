#!/bin/bash

set -e

verbose=0
show_files=1
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
    | grep -v -E ".*cy_flash_map.h") || true

cd "$(git rev-parse --show-toplevel)"

# list of files which contain a diff
diff_files=()

have_diff=0
for file in $changed_c_h_files; do
    # turn off fail on error because diff exits with non-zero when there is a diff
    # we want to identify that happening, not fail the script
    set +e
    if ((verbose)); then
        diff -u "$file" <(clang-format "$file")
    else
        diff -u "$file" <(clang-format "$file") > /dev/null
    fi

    if [[ $? -ne 0 ]]
    then
        diff_files+=($file)
        have_diff=1
    fi
    set -e
done

RED='\033[0;31m'
NO_COLOR='\033[0m'

if [[ $have_diff -eq 1 ]]
then
    if ((show_files)); then
        echo
        echo -e "${RED}"
        echo "Files with clang-format violations:"
        for file in "${diff_files[@]}"; do
            echo "* $file"
        done
        echo ""
        echo "To fix the offending files (i.e. format them in-place), try:"
        echo "cd $REPO_ROOT &&"
        for file in $diff_files; do
            echo "  clang-format -i $file &&"
        done
        echo "  cd -"
        echo -e "${NO_COLOR}"
        echo ""
    fi

    exit 1
fi

exit 0
