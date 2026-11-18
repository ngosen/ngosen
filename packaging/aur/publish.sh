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
# The source archive goes here, so it never lands in the AUR checkout.
export SRCDEST=$work/src
mkdir "$SRCDEST"

srcinfo_version() {
    local epoch
    epoch=$(sed -n 's/^\tepoch = //p' "$1")
    echo "${epoch:+$epoch:}$(sed -n 's/^\tpkgver = //p' "$1")-$(sed -n 's/^\tpkgrel = //p' "$1")"
}

git clone -q ssh://aur@aur.archlinux.org/fcitx5-ngosen.git "$work/aur"
cd "$work/aur"
old=$([ -f .SRCINFO ] && srcinfo_version .SRCINFO || echo 0)
cp "$here/PKGBUILD" "$here/../arch/fcitx5-ngosen.install" .
updpkgsums
makepkg --printsrcinfo > .SRCINFO
new=$(srcinfo_version .SRCINFO)

# Rerunning an older release must not take the AUR back.
if [ "$(vercmp "$new" "$old")" -lt 0 ]; then
    echo "AUR has $old, newer than $new; not pushing" >&2
    exit 1
fi

git add PKGBUILD .SRCINFO fcitx5-ngosen.install
if git diff --cached --quiet; then
    echo "AUR already has $new"
    exit 0
fi
git -c user.name="Phi Nguyễn" -c user.email="nguyenphidt@gmail.com" commit -q -m "Update to $new"
git push -q origin HEAD:master
echo "pushed fcitx5-ngosen $new"
