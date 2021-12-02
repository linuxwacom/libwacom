#!/usr/bin/env bash

set -e

date=`date +"%Y-%m-%d-%H.%M.%S"`
builddir="build.$date"

echo "####################################### running test suite"
meson $builddir
ninja -C $builddir test

echo "####################################### running valgrind"
pushd $builddir > /dev/null
meson test --setup=valgrind --suite=valgrind
popd > /dev/null

echo "####################################### running ubsan"
meson configure $builddir  -Db_sanitize=undefined
ninja -C $builddir test

echo "####################################### running asan"
meson configure $builddir  -Db_sanitize=address
ninja -C $builddir test

echo "####################################### running scan-build"
meson configure $builddir  -Db_sanitize=none
ninja -C $builddir scan-build

echo "######## Success. Removing builddir #########"
rm -rf "$buildir"
