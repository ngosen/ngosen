#!/bin/bash
# Checks which releases packaging/download-release-files.sh accepts, against a fake gh that serves
# releases from a directory: <repo>/<tag>/<files>, newest tag first in <repo>/tags.
#
#   test/download-release-files.sh
set -euo pipefail

top=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

mkdir -p "$work/bin"
cat >"$work/bin/gh" <<'EOF'
#!/bin/bash
# Only the two calls download-release-files.sh makes.
set -euo pipefail
cmd="$1 $2"
shift 2
repo= dir= tag= patterns=()
while [ $# -gt 0 ]; do
    case "$1" in
        --repo) repo=$2; shift 2 ;;
        --dir) dir=$2; shift 2 ;;
        --pattern) patterns+=("$2"); shift 2 ;;
        --limit) limit=$2; shift 2 ;;
        --exclude-drafts | --exclude-pre-releases) shift ;;
        --json | --jq) shift 2 ;;
        *) tag=$1; shift ;;
    esac
done
root=$FAKE_RELEASES/$repo
case "$cmd" in
    # Like gh, an empty list for a repository without releases.
    "release list") if [ -f "$root/tags" ]; then head -n "$limit" "$root/tags"; fi ;;
    "release download")
        for file in "$root/$tag"/*; do
            for pattern in "${patterns[@]}"; do
                # shellcheck disable=SC2053 # the pattern is a glob on purpose
                if [[ $(basename "$file") == $pattern ]]; then
                    cp "$file" "$dir/"
                    break
                fi
            done
        done
        ;;
esac
EOF
chmod +x "$work/bin/gh"
export PATH="$work/bin:$PATH" FAKE_RELEASES="$work/releases"

# release <repo> <tag> <file>...: a release whose SHA256SUMS lists every file given.
release() {
    local repo=$1 tag=$2
    shift 2
    mkdir -p "$FAKE_RELEASES/$repo/$tag"
    echo "$tag" >>"$FAKE_RELEASES/$repo/tags"
    for name in "$@"; do
        echo "$tag $name" >"$FAKE_RELEASES/$repo/$tag/$name"
    done
    (cd "$FAKE_RELEASES/$repo/$tag" && sha256sum -- * >SHA256SUMS)
}

# expect <name> <pass|fail> <files wanted, or the message a failure must print> <args>...
expect() {
    local name=$1 want=$2 files=$3 out got=pass
    shift 3
    out=$work/out-$name
    "$top/packaging/download-release-files.sh" "$@" "$out" >"$work/log-$name" 2>&1 || got=fail
    if [ "$got" != "$want" ]; then
        echo "FAIL: $name: expected the script to $want, it did $got:"
        sed 's/^/    /' "$work/log-$name"
        failures=$((failures + 1))
        return
    fi
    if [ "$want" = fail ]; then
        if ! grep -qF -- "$files" "$work/log-$name"; then
            echo "FAIL: $name: failed without saying '$files':"
            sed 's/^/    /' "$work/log-$name"
            failures=$((failures + 1))
        fi
        return 0
    fi
    local listed
    listed=$(find "$out" -mindepth 1 -printf '%f\n' | sort | tr '\n' ' ')
    if [ "$listed" != "$files" ]; then
        echo "FAIL: $name: downloaded '$listed', expected '$files'"
        failures=$((failures + 1))
    fi
}

mkdir -p "$FAKE_RELEASES/ngosen/none"
expect fcitx5-without-release fail 'no published release' ngosen/none 2 '*.deb'
expect wl-without-release pass "" --new-edition ngosen/none 2 '*.deb'

release ngosen/ibus ibus-ngosen-1.0.0-1 ibus-ngosen_1.0.0-1_noble_amd64.deb
expect ibus-without-opensuse pass "" ngosen/ibus 2 '*.opensuse-tumbleweed.x86_64.rpm'

release ngosen/wl-bad wl-ngosen-0.1.0-1 wl-ngosen-0.1.0.tar.gz
expect wl-without-deb fail 'has no file matching' --new-edition ngosen/wl-bad 2 '*.deb'

release ngosen/wl wl-ngosen-0.1.0-1 wl-ngosen_0.1.0-1_noble_amd64.deb wl-ngosen-0.1.0.tar.gz
expect wl-with-deb pass "wl-ngosen_0.1.0-1_noble_amd64.deb " --new-edition ngosen/wl 2 '*.deb'

release ngosen/tampered fcitx5-ngosen-1.0.0 fcitx5-ngosen_1.0.0-1_noble_amd64.deb
echo replaced >"$FAKE_RELEASES/ngosen/tampered/fcitx5-ngosen-1.0.0/fcitx5-ngosen_1.0.0-1_noble_amd64.deb"
expect tampered-file fail 'FAILED' ngosen/tampered 2 '*.deb'

[ "$failures" -eq 0 ] || exit 1
echo "all checks passed"
