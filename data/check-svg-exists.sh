#!/usr/bin/env bash
#
# Usage: check-svg-exists.sh /path/to/libwacom/

if [ -z "$top_srcdir" ]; then
    top_srcdir="$1"
fi
if [ -z "$top_srcdir" ]; then
    echo "Usage: `basename $0` /path/to/libwacom"
    exit 1
fi

pushd "$top_srcdir" > /dev/null
for file in data/*.tablet; do
        svg=`grep 'Layout=' "$file" | sed -e "s/Layout=//"`;
        test -z "$svg" ||
        test -e "data/layouts/$svg" ||
            (echo "ERROR: File '$file' references nonexistent '$svg'" && test);
        rc="$(($rc + $?))";
done

for file in data/layouts/*.svg; do
    grep -q "`basename $file`" $top_srcdir/data/*.tablet ||
        (echo "ERROR: Layout $file is not referenced" && test);
    rc="$(($rc + $?))";
done

exit $rc
