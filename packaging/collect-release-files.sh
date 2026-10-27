#!/bin/bash
# Picks the packages a release publishes out of the CI artifacts and names them so the installer
# can tell which one belongs to which distribution. Development targets (Debian unstable, Fedora
# Rawhide) and debug packages are built by CI but not published. Debian testing is published
# because Kali Rolling follows it.
set -euo pipefail

src=${1:?usage: collect-release-files.sh <artifacts-dir> <output-dir>}
out=${2:?usage: collect-release-files.sh <artifacts-dir> <output-dir>}
mkdir -p "$out"

# Prints the single file under $1 matching $2; anything else means the build changed shape.
only() {
    local found
    found=$(find "$1" -type f -name "$2" | sort)
    if [ "$(printf '%s\n' "$found" | grep -c .)" != 1 ]; then
        echo "expected exactly one $2 under $1, found: ${found:-none}" >&2
        exit 1
    fi
    printf '%s\n' "$found"
}

for codename in bookworm trixie forky noble resolute stonking; do
    for arch in amd64 arm64; do
        dir=$src/deb-$codename
        [ "$arch" = amd64 ] || dir=$dir-$arch
        deb=$(only "$dir" "fcitx5-ngosen_*_$arch.deb")
        cp "$deb" "$out/$(basename "$deb" "_$arch.deb")_${codename}_$arch.deb"
    done
done

for release in 43 44; do
    cp "$(only "$src/rpm-fedora-$release" 'fcitx5-ngosen-[0-9]*.x86_64.rpm')" "$out/"
    cp "$(only "$src/rpm-fedora-$release-arm64" 'fcitx5-ngosen-[0-9]*.aarch64.rpm')" "$out/"
done

rpm=$(only "$src/rpm-opensuse-tumbleweed" 'fcitx5-ngosen-[0-9]*.x86_64.rpm')
cp "$rpm" "$out/$(basename "$rpm" .x86_64.rpm).opensuse-tumbleweed.x86_64.rpm"

cp "$(only "$src/arch-pkg" 'fcitx5-ngosen-[0-9]*-x86_64.pkg.tar.zst')" "$out/"

# Written outside the directory first, so a rerun never lists the checksum file in itself.
sums=$(mktemp)
(cd "$out" && rm -f SHA256SUMS && sha256sum -- * > "$sums")
mv "$sums" "$out/SHA256SUMS"
chmod 644 "$out/SHA256SUMS"
