#!/bin/sh
set -e
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cp -R "$root/src" "$work/src"
cp "$root"/compat/http-at/* "$work/src/"
cp "$root/Makefile" "$work/Makefile"
ln -s "$root/third_party" "$work/third_party"
make -C "$work" arm "$@"
mkdir -p "$root/build"
cp "$work/build/zxct-ims.arm" "$root/build/zxct-ims-http.arm"
