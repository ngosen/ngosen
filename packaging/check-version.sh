#!/bin/bash
# Fails unless every packaging file carries the given version, e.g. 3.5.10-4.
# A release tag that disagrees with them would publish packages under the wrong version.
set -euo pipefail

want=${1:?usage: check-version.sh <version>-<release>}
cd "$(dirname "$0")"
fail=0

check() {
    if [ "$2" = "$3" ]; then
        echo "ok        $1: $2"
    else
        echo "MISMATCH  $1: $2 (want $3)"
        fail=1
    fi
}

spec_version() {
    echo "$(sed -n 's/^Version: *//p' "$1")-$(sed -n 's/^Release: *\([0-9]*\).*/\1/p' "$1")"
}

check debian/changelog "$(sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' debian/changelog)" "$want"
check rpm/fedora "$(spec_version rpm/fedora/fcitx5-lotus.spec)" "$want"
check rpm/opensuse "$(spec_version rpm/opensuse/fcitx5-lotus.spec)" "$want"
check arch/PKGBUILD "$(sed -n 's/^pkgver=//p' arch/PKGBUILD)-$(sed -n 's/^pkgrel=//p' arch/PKGBUILD)" "$want"
check CMakeLists.txt "$(sed -n 's/^project(fcitx5-lotus VERSION \([0-9.]*\)).*/\1/p' ../CMakeLists.txt)" "${want%-*}"

exit "$fail"
