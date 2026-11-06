#!/bin/bash
# Checks which Fedora packages packaging/collect-release-files.sh publishes, on empty stand-ins for
# the CI artifacts.
#
#   test/collect-release-files.sh [path-to-collect-script]
set -euo pipefail

collect=${1:-$(dirname "$0")/../packaging/collect-release-files.sh}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

artifact() {
    mkdir -p "$work/artifacts/$1"
    : >"$work/artifacts/$1/$2"
}

for codename in bookworm trixie forky noble resolute stonking; do
    artifact "deb-$codename" fcitx5-ngosen_1-1_amd64.deb
    artifact "deb-$codename-arm64" fcitx5-ngosen_1-1_arm64.deb
done
artifact rpm-opensuse-tumbleweed fcitx5-ngosen-1-1.x86_64.rpm
artifact arch-pkg fcitx5-ngosen-1-1-x86_64.pkg.tar.zst

# 90 has both architectures, 91 only x86_64, and rawhide is never published.
artifact rpm-fedora-90 fcitx5-ngosen-1-1.fc90.x86_64.rpm
artifact rpm-fedora-90 fcitx5-ngosen-debuginfo-1-1.fc90.x86_64.rpm
artifact rpm-fedora-90-arm64 fcitx5-ngosen-1-1.fc90.aarch64.rpm
artifact rpm-fedora-91 fcitx5-ngosen-1-1.fc91.x86_64.rpm
artifact rpm-fedora-rawhide fcitx5-ngosen-1-1.fc92.x86_64.rpm

bash "$collect" "$work/artifacts" "$work/out"

expected='fcitx5-ngosen-1-1.fc90.aarch64.rpm
fcitx5-ngosen-1-1.fc90.x86_64.rpm
fcitx5-ngosen-1-1.fc91.x86_64.rpm'
actual=$(find "$work/out" -name '*.fc*' -printf '%f\n' | LC_ALL=C sort)
if [ "$actual" != "$expected" ]; then
    printf 'FAIL: published Fedora packages:\n%s\nexpected:\n%s\n' "$actual" "$expected" >&2
    exit 1
fi
echo "PASS: published Fedora packages"
