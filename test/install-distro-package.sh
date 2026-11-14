#!/bin/bash
# Checks that install.sh tells a distribution's fcitx5-ngosen from the packages Ngó Sen publishes.
#
#   test/install-distro-package.sh <source dir>
set -euo pipefail

top=${1:?usage: install-distro-package.sh <source dir>}
failures=0

# install.sh runs main on its last line, so only the function under test is loaded.
# shellcheck source=/dev/null
source <(sed -n '/^distro_package_version() {/,/^}/p' "$top/install.sh")

installed=1
fake_version=
is_package_installed() { [ "$installed" -eq 1 ]; }
dpkg-query() { printf '%s' "$fake_version"; }

check() {
    local want=$1 got
    got=$(distro_package_version || true)
    if [ "$got" != "$want" ]; then
        echo "FAIL: $manager $edition installed=$installed '$fake_version': expected '$want', got '$got'"
        failures=$((failures + 1))
    fi
}

manager=apt edition=fcitx5
for fake_version in 1.9.2+ds-1 1.11.0+ds-1ubuntu1; do
    check "$fake_version"
done
# Ngó Sen's own packages: the epoch since 0.5.0, none on the older 3.5.10 builds.
for fake_version in 1:1.10.1-1 1:1.11.0+ds-1 3.5.10-4; do
    check ""
done
fake_version=1.9.2+ds-1
installed=0 check ""
installed=1
edition=ibus check ""
manager=dnf check ""

[ "$failures" -eq 0 ] || exit 1
echo "all checks passed"
