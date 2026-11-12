#!/bin/bash
# Builds the signed openSUSE Tumbleweed repository from release RPMs named by
# collect-release-files.sh (fcitx5-ngosen-<version>.opensuse-tumbleweed.x86_64.rpm), and the
# ibus-ngosen and wl-ngosen ones their releases name the same way.
# zypper checks the signed repomd.xml, which lists every package's checksum.
# Signs with the default secret key in the caller's GnuPG home. Needs createrepo_c.
set -euo pipefail

src=${1:?usage: build-zypper-repo.sh <rpms-dir> <output-dir>}
out=${2:?usage: build-zypper-repo.sh <rpms-dir> <output-dir>}
url=${3:-https://ngosen.github.io/ngosen/opensuse}

rm -rf "$out"
mkdir -p "$out/x86_64"
found=0
for rpm in "$src"/{fcitx5,ibus,wl}-ngosen-*.opensuse-tumbleweed.x86_64.rpm; do
    [ -e "$rpm" ] || continue
    cp "$rpm" "$out/x86_64/"
    found=1
done
if [ "$found" = 0 ]; then
    echo "no openSUSE packages in $src" >&2
    exit 1
fi

createrepo_c --quiet "$out"
# A package under a name this script does not know would be signed away silently.
primary=$(find "$out/repodata" -name '*primary.xml.*' ! -name '*.zck' | head -n1)
case "$primary" in
    *.gz) read_primary="zcat" ;;
    *.zst) read_primary="zstdcat" ;;
    *.xz) read_primary="xzcat" ;;
    *) read_primary="cat" ;;
esac
listed=$("$read_primary" "$primary" | sed -n 's|.*<location href="x86_64/\([^"]*\)".*|\1|p')
for rpm in "$src"/*.rpm; do
    [ -e "$rpm" ] || continue
    if ! grep -qxF "$(basename "$rpm")" <<<"$listed"; then
        echo "$(basename "$rpm") is not in the repository" >&2
        exit 1
    fi
done
gpg --batch --yes --armor --detach-sign -o "$out/repodata/repomd.xml.asc" "$out/repodata/repomd.xml"
gpg --batch --armor --export > "$out/repodata/repomd.xml.key"

cat > "$out/ngosen.repo" <<EOF
[ngosen]
name=Ngo Sen
baseurl=$url
type=rpm-md
enabled=1
autorefresh=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=$url/repodata/repomd.xml.key
EOF
