#!/bin/bash -e

pushd "$1" > /dev/null
diff -u1 <(grep -o 'data/.*\.tablet' meson.build) <(ls -v data/*.tablet)
diff -u1 <(grep -o 'data/.*\.stylus' meson.build) <(ls -v data/*.stylus)
diff -u1 <(grep -o 'data/layouts/.*\.svg' meson.build) <(ls -v data/layouts/*.svg)
popd > /dev/null
