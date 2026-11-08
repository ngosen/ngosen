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
case "$on" in
*Cargo.lock*config.toml*) ;;
*)
    echo "the default build no longer rebuilds when the lock file or vendor config changes" >&2
    fail=1
    ;;
esac

off=$(rules off -DNGOSEN_VENDORED_CRATES=OFF) || exit 1
case "$off" in
*--locked*)
    echo "NGOSEN_VENDORED_CRATES=OFF still passes --locked" >&2
    fail=1
    ;;
esac
case "$off" in
*Cargo.lock* | *config.toml*)
    echo "NGOSEN_VENDORED_CRATES=OFF still needs files distributions delete" >&2
    fail=1
    ;;
esac
# Cargo keeps reading bamboo-rs/.cargo/config.toml, so a packager who leaves vendor/ in place would
# ship the bundled crates without noticing.
# A tarball repacked without vendor/ has nothing to warn about.
if [ -d "$source_dir/bamboo-rs/vendor" ]; then
    grep -q "bamboo-rs/vendor" "$work/off.log" || {
        echo "NGOSEN_VENDORED_CRATES=OFF does not warn about bamboo-rs/vendor" >&2
        fail=1
    }
fi

exit $fail
