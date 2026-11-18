#!/bin/bash
# Pushes the AUR package for the release tag the PKGBUILD names. Run as a user that may run makepkg,
# with AUR_SSH_KEY_FILE pointing at a key the AUR account accepts.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
key=${AUR_SSH_KEY_FILE:?set AUR_SSH_KEY_FILE}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Pinned so a spoofed host cannot take the push; matches the fingerprint on https://aur.archlinux.org.
printf '%s\n' 'aur.archlinux.org ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIEuBKrPzbawxA/k2g6NcyV5jmqwJ2s+zpgZGZ7tpLIcN' \
    > "$work/known_hosts"
export GIT_SSH_COMMAND="ssh -i $key -o IdentitiesOnly=yes -o UserKnownHostsFile=$work/known_hosts -o StrictHostKeyChecking=yes"

git clone -q ssh://aur@aur.archlinux.org/fcitx5-ngosen.git "$work/aur"
cp "$here/PKGBUILD" "$here/../arch/fcitx5-ngosen.install" "$work/aur/"
cd "$work/aur"
updpkgsums
makepkg --printsrcinfo > .SRCINFO

git add -A
if git diff --cached --quiet; then
    echo "AUR already has this version"
    exit 0
fi
version=$(sed -n 's/^\tpkgver = //p' .SRCINFO)-$(sed -n 's/^\tpkgrel = //p' .SRCINFO)
git -c user.name="Phi Nguyễn" -c user.email="nguyenphidt@gmail.com" commit -q -m "Update to $version"
git push -q origin HEAD:master
echo "pushed fcitx5-ngosen $version"
