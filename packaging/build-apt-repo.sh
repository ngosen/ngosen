#!/bin/bash
# Builds the signed APT repository from release .deb files named by collect-release-files.sh
# (fcitx5-ngosen_<version>_<codename>_<arch>.deb). Each codename is its own suite with its own
# pool, because every distribution gets a build with the same version but different contents.
# Signs with the default secret key in the caller's GnuPG home.
set -euo pipefail

src=${1:?usage: build-apt-repo.sh <debs-dir> <output-dir>}
out=${2:?usage: build-apt-repo.sh <debs-dir> <output-dir>}
codenames="bookworm trixie forky jammy noble resolute"
archs="amd64 arm64"

rm -rf "$out"
mkdir -p "$out"

for codename in $codenames; do
    pool=$out/pool/$codename
    mkdir -p "$pool"
    for deb in "$src"/fcitx5-ngosen_*_"${codename}"_*.deb; do
        [ -e "$deb" ] || continue
        cp "$deb" "$pool/"
    done
    if [ -z "$(ls -A "$pool")" ]; then
        echo "no packages for $codename in $src" >&2
        exit 1
    fi

    for arch in $archs; do
        dir=$out/dists/$codename/main/binary-$arch
        mkdir -p "$dir"
        # Run from the repository root, so Filename: fields are relative to it.
        (cd "$out" && apt-ftparchive --arch "$arch" packages "pool/$codename") > "$dir/Packages"
        gzip -9nk "$dir/Packages"
    done

    # Written outside the suite first, so Release never lists itself.
    release=$(mktemp)
    (cd "$out/dists/$codename" && apt-ftparchive \
        -o APT::FTPArchive::Release::Origin="Ngo Sen" \
        -o APT::FTPArchive::Release::Label="Ngo Sen" \
        -o APT::FTPArchive::Release::Suite="$codename" \
        -o APT::FTPArchive::Release::Codename="$codename" \
        -o APT::FTPArchive::Release::Architectures="$archs" \
        -o APT::FTPArchive::Release::Components=main \
        release .) > "$release"
    mv "$release" "$out/dists/$codename/Release"
    chmod 644 "$out/dists/$codename/Release"
    gpg --batch --yes --clearsign -o "$out/dists/$codename/InRelease" "$out/dists/$codename/Release"
    gpg --batch --yes --armor --detach-sign -o "$out/dists/$codename/Release.gpg" \
        "$out/dists/$codename/Release"
done

gpg --batch --export > "$out/ngosen.gpg"
gpg --batch --armor --export > "$out/ngosen.asc"
