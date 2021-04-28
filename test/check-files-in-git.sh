#!/usr/bin/env bash
#
# Usage: check-files-in-git.sh /path/to/libwacom/

if [ -z "$top_srcdir" ]; then
    top_srcdir="$1"
fi
if [ -z "$top_srcdir" ]; then
    echo "Usage: `basename $0` /path/to/libwacom"
    exit 1
fi

export GIT_DIR="$top_srcdir/.git";
if ! git ls-files >& /dev/null; then
    echo "Not a git tree. Skipping"
    exit 77
fi

pushd "$top_srcdir" > /dev/null
for file in data/*.tablet data/*.stylus data/layouts/*.svg; do
    git ls-files --error-unmatch "$file" &> /dev/null || (
        echo "ERROR: File $file is not in git" && test);
    rc="$(($rc + $?))";
done
popd > /dev/null
exit $rc
