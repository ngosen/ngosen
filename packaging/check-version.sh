#!/bin/bash
# Fails unless every packaging file carries the given version, e.g. 0.5.0-1, and the same epoch.
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

deb=$(sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' debian/changelog)
check debian/changelog "${deb#*:}" "$want"
check rpm/fedora "$(spec_version rpm/fedora/fcitx5-ngosen.spec)" "$want"
check rpm/opensuse "$(spec_version rpm/opensuse/fcitx5-ngosen.spec)" "$want"
check arch/PKGBUILD "$(sed -n 's/^pkgver=//p' arch/PKGBUILD)-$(sed -n 's/^pkgrel=//p' arch/PKGBUILD)" "$want"
check CMakeLists.txt "$(sed -n 's/^project(fcitx5-ngosen VERSION \([0-9.]*\)).*/\1/p' ../CMakeLists.txt)" "${want%-*}"

# A package whose epoch differs would sort below or above the others for the package manager.
epoch=$(sed -n 's/^epoch=//p' arch/PKGBUILD)
check "debian/changelog epoch" "${deb%%:*}" "$epoch"
check "rpm/fedora epoch" "$(sed -n 's/^Epoch: *//p' rpm/fedora/fcitx5-ngosen.spec)" "$epoch"
check "rpm/opensuse epoch" "$(sed -n 's/^Epoch: *//p' rpm/opensuse/fcitx5-ngosen.spec)" "$epoch"

exit "$fail"
