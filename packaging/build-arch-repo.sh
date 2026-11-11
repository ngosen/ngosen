#!/bin/bash
# Builds the signed pacman repository "ngosen" from the newest fcitx5-ngosen package and, when there are
# ones, the newest ibus-ngosen and wl-ngosen packages. pacman only offers the newest version of a package anyway.
# Signs with the default secret key in the caller's GnuPG home. Needs repo-add from pacman.
set -euo pipefail

src=${1:?usage: build-arch-repo.sh <packages-dir> <output-dir>}
out=${2:?usage: build-arch-repo.sh <packages-dir> <output-dir>}

# Prints the package files for $1 under $src; more than one means two releases got mixed.
find_pkg() {
    local found
    found=$(find "$src" -maxdepth 1 -type f -name "$1-[0-9]*-x86_64.pkg.tar.zst" | sort)
    if [ "$(printf '%s\n' "$found" | grep -c .)" -gt 1 ]; then
        echo "expected at most one $1 x86_64 package in $src, found: $found" >&2
        exit 1
    fi
    printf '%s' "$found"
}

fcitx5=$(find_pkg fcitx5-ngosen)
ibus=$(find_pkg ibus-ngosen)
wl=$(find_pkg wl-ngosen)
if [ -z "$fcitx5" ]; then
    echo "expected one fcitx5-ngosen x86_64 package in $src, found none" >&2
    exit 1
fi
# A package under a name this script does not know would be signed away silently.
for pkg in "$src"/*.pkg.tar.zst; do
    [ -e "$pkg" ] || continue
    # By name, so a trailing slash on $src cannot make the paths differ.
    case "$(basename "$pkg")" in
        "$(basename "$fcitx5")" | "$(basename "${ibus:-/}")" | "$(basename "${wl:-/}")") ;;
        *)
            echo "$(basename "$pkg") is not in the repository" >&2
            exit 1
            ;;
    esac
done

rm -rf "$out"
dir=$out/x86_64
mkdir -p "$dir"
for src_pkg in "$fcitx5" ${ibus:+"$ibus"} ${wl:+"$wl"}; do
    cp "$src_pkg" "$dir/"
    pkg=$dir/$(basename "$src_pkg")
    gpg --batch --yes --detach-sign -o "$pkg.sig" "$pkg"
    repo-add --sign --verify "$dir/ngosen.db.tar.gz" "$pkg"
done

# repo-add links ngosen.db to ngosen.db.tar.gz; Pages does not serve symlinks.
for link in "$dir"/*; do
    if [ -L "$link" ]; then
        cp --remove-destination "$(readlink -f "$link")" "$link"
    fi
done
rm -f "$dir"/*.old "$dir"/*.old.sig

gpg --batch --armor --export > "$out/ngosen.asc"
