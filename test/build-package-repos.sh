#!/bin/bash
# Builds one package repository from small fake packages, signed with a throwaway key, with the two
# older editions and with all three, and checks that every package ends up in it. CI runs each kind
# where its tools live, as package-repos.yml does.
#
#   test/build-package-repos.sh <apt|arch|zypper>
set -euo pipefail

kind=${1:?usage: build-package-repos.sh <apt|arch|zypper>}
top=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

export GNUPGHOME=$work/gnupg
mkdir -m 700 "$GNUPGHOME"
gpg --batch --quiet --passphrase '' --quick-gen-key 'Package repository test <test@example.invalid>' ed25519 sign never

fake_deb() { # <name> <version> <codename> <arch> <dir>
    local root=$work/deb-root
    rm -rf "$root"
    mkdir -p "$root/DEBIAN"
    printf 'Package: %s\nVersion: %s\nArchitecture: %s\nMaintainer: Test <test@example.invalid>\nDescription: test\n' \
        "$1" "$2" "$4" >"$root/DEBIAN/control"
    dpkg-deb --build --root-owner-group "$root" "$5/$1_$2_$3_$4.deb" >/dev/null
}

fake_arch() { # <name> <version> <dir>
    local root=$work/arch-root
    rm -rf "$root"
    mkdir -p "$root"
    printf 'pkgname = %s\npkgbase = %s\npkgver = %s\npkgdesc = test\nbuilddate = 0\npackager = Test\nsize = 0\narch = x86_64\n' \
        "$1" "$1" "$2" >"$root/.PKGINFO"
    (cd "$root" && bsdtar --zstd -cf "$3/$1-$2-x86_64.pkg.tar.zst" .PKGINFO)
}

fake_rpm() { # <name> <version> <dir>
    local spec=$work/$1.spec
    printf 'Name: %s\nVersion: %s\nRelease: 1\nSummary: test\nLicense: MIT\n%%description\ntest\n%%files\n' \
        "$1" "$2" >"$spec"
    rpmbuild --quiet -bb --target x86_64 --define "_topdir $work/rpmbuild" "$spec" >/dev/null 2>&1
    mv "$work/rpmbuild/RPMS/x86_64/$1-$2-1.x86_64.rpm" "$3/$1-$2-1.opensuse-tumbleweed.x86_64.rpm"
}

# packages <dir> <editions...>: the packages a release of each edition would bring.
packages() {
    local dir=$1 edition codename arch
    shift
    mkdir -p "$dir"
    for edition in "$@"; do
        case "$kind" in
            apt)
                for codename in bookworm trixie forky noble resolute stonking; do
                    # Like the real releases, the newer editions skip bookworm.
                    [ "$edition" != fcitx5-ngosen ] && [ "$codename" = bookworm ] && continue
                    for arch in amd64 arm64; do
                        fake_deb "$edition" 1.0.0-1 "$codename" "$arch" "$dir"
                    done
                done
                ;;
            arch) fake_arch "$edition" 1.0.0-1 "$dir" ;;
            zypper) fake_rpm "$edition" 1.0.0 "$dir" ;;
        esac
    done
}

build() { # <src> <out>
    case "$kind" in
        apt) "$top/packaging/build-apt-repo.sh" "$1" "$2" ;;
        arch) "$top/packaging/build-arch-repo.sh" "$1" "$2" ;;
        zypper) "$top/packaging/build-zypper-repo.sh" "$1" "$2" ;;
    esac
}

# The package names the built repository offers, one per line.
offered() {
    case "$kind" in
        apt) sed -n 's/^Package: //p' "$1"/dists/*/main/binary-*/Packages | sort -u ;;
        arch) bsdtar -tf "$1/x86_64/ngosen.db.tar.gz" | sed -n 's|^\(.*\)-[^-]*-[^-]*/$|\1|p' | sort -u ;;
        zypper)
            local primary
            primary=$(find "$1/repodata" -name '*primary.xml.*' ! -name '*.zck' | head -n1)
            case "$primary" in
                *.gz) zcat "$primary" ;;
                *.zst) zstdcat "$primary" ;;
                *.xz) xzcat "$primary" ;;
                *) cat "$primary" ;;
            esac | sed -n 's|.*<name>\(.*\)</name>.*|\1|p' | sort -u
            ;;
    esac
}

check() { # <case> <editions...>
    local name=$1 want got
    shift
    packages "$work/$name-src" "$@"
    if ! build "$work/$name-src" "$work/$name-out" >"$work/$name.log" 2>&1; then
        echo "FAIL: $name: the build failed:"
        sed 's/^/    /' "$work/$name.log"
        failures=$((failures + 1))
        return
    fi
    want=$(printf '%s\n' "$@" | sort)
    got=$(offered "$work/$name-out")
    if [ "$got" != "$want" ]; then
        echo "FAIL: $name: the repository offers '${got//$'\n'/ }', expected '${want//$'\n'/ }'"
        failures=$((failures + 1))
    fi
}

check two-editions fcitx5-ngosen ibus-ngosen
check three-editions fcitx5-ngosen ibus-ngosen wl-ngosen

# A package the script has no name for must stop the build rather than go missing.
packages "$work/unknown-src" fcitx5-ngosen ibus-ngosen other-ngosen
if build "$work/unknown-src" "$work/unknown-out" >"$work/unknown.log" 2>&1 ||
    ! grep -q 'other-ngosen.* is not in the repository' "$work/unknown.log"; then
    echo "FAIL: unknown-package: the build did not stop at other-ngosen:"
    sed 's/^/    /' "$work/unknown.log"
    failures=$((failures + 1))
fi

[ "$failures" -eq 0 ] || exit 1
echo "all $kind checks passed"
