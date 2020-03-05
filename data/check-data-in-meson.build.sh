#!/usr/bin/env bash

set -e

pushd "$1" > /dev/null
diff -u1 <(grep -o 'data/.*\.tablet' meson.build) <(printf '%s\n' data/*.tablet | sort -V)
diff -u1 <(grep -o 'data/.*\.stylus' meson.build) <(printf '%s\n' data/*.stylus | sort -V)
diff -u1 <(grep -o 'data/layouts/.*\.svg' meson.build) <(printf '%s\n' data/layouts/*.svg | sort -V)
popd > /dev/null
