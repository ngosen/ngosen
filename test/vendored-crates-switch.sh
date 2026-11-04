#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A distribution that builds against its own crates has no lock file to match, so cargo must not get
# --locked there; every other build must keep it so the vendored crates stay the ones reviewed.
set -u
cmake=${1:?usage: vendored-crates-switch.sh <cmake> <source dir> <build dir>}
source_dir=${2:?}
work=$(mktemp -d "${3:?}/vendored-crates-switch.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT
fail=0

# Prints the generated build rules of the Rust library.
rules() {
    dir=$work/$1
    shift
    "$cmake" -S "$source_dir" -B "$dir" -DNGOSEN_CORE_ONLY=ON "$@" >"$dir.log" 2>&1 || {
        cat "$dir.log" >&2
        return 1
    }
    cat "$dir/bamboo-rs/CMakeFiles/ngosen-bamboo.dir/build.make" 2>/dev/null || cat "$dir/build.ninja"
}

on=$(rules on) || exit 1
case "$on" in
*"--locked"*) ;;
*)
    echo "the default build no longer passes --locked" >&2
    fail=1
    ;;
esac

off=$(rules off -DNGOSEN_VENDORED_CRATES=OFF) || exit 1
case "$off" in
*"--offline --lib"*) ;;
*)
    echo "NGOSEN_VENDORED_CRATES=OFF does not run cargo without --locked" >&2
    fail=1
    ;;
esac
case "$off" in
*Cargo.lock*)
    echo "NGOSEN_VENDORED_CRATES=OFF still needs Cargo.lock, which distributions delete" >&2
    fail=1
    ;;
esac

exit $fail
