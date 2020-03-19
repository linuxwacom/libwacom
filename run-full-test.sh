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

echo "####################################### running make distcheck"

# we can't configure with a custom sourcetree if we have run configure
# previously. So let's only do this where we have a clean tree that we can
# clone.
if git diff --exit-code -s; then
    unset GIT_WORK_TREE
    mkdir -p "$builddir/autotools/build"
    mkdir -p "$builddir/autotools/inst"
    pushd "$builddir/autotools" > /dev/null
    git clone ../.. "src"
    pushd src > /dev/null
    autoreconf -ivf
    popd
    pushd build > /dev/null
    ../src/configure --prefix=$PWD/../inst/
    make && make check
    make install
    make distcheck
    echo "############### running meson off tarball"
    mkdir -p _tarball_dir
    tar xf libwacom-*.tar.bz2 -C _tarball_dir
    pushd _tarball_dir/libwacom-*/ > /dev/null
    meson $builddir && ninja -C $builddir test
    popd > /dev/null
    popd > /dev/null
    popd > /dev/null
else
    echo "local changes present, skipping make distcheck"
fi

echo "######## Success. Removing builddir #########"
rm -rf "$buildir"
