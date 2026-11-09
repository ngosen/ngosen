#!/bin/bash
# Checks how install.sh tells whether the Ngó Sen APT repository is enabled, against the sample
# source files that the settings window's check reads too, and that it offers only the suites the
# repository is built for.
#
#   test/install-apt-repo.sh <source dir>
set -euo pipefail

top=${1:?usage: install-apt-repo.sh <source dir>}
data=$top/test/data/apt-sources
failures=0

# install.sh runs main on its last line, so only the functions under test are loaded.
# shellcheck source=/dev/null
source <(sed -n '/^apt_file_lists_repo() {/,/^}/p; /^apt_repo_file() {/,/^}/p' "$top/install.sh")

while read -r file want; do
    case "$file" in '' | '#'*) continue ;; esac
    got=no
    apt_file_lists_repo "$data/$file" https://ngosen.github.io/ngosen/apt && got=yes
    if [ "$got" != "$want" ]; then
        echo "FAIL: $file: expected $want, got $got"
        failures=$((failures + 1))
    fi
done <"$data/expected"

# apt_repo_file walks sources.list and both kinds of files under sources.list.d.
etc=$(mktemp -d)
trap 'rm -rf "$etc"' EXIT
mkdir -p "$etc/sources.list.d"
cp "$data/other.list" "$etc/sources.list"
cp "$data/commented.list" "$etc/sources.list.d/a.list"
cp "$data/enabled.sources" "$etc/sources.list.d/b.sources"
found=$(apt_repo_file "$etc" https://ngosen.github.io/ngosen/apt || true)
if [ "$found" != "$etc/sources.list.d/b.sources" ]; then
    echo "FAIL: apt_repo_file found '$found'"
    failures=$((failures + 1))
fi
rm "$etc/sources.list.d/b.sources"
if apt_repo_file "$etc" https://ngosen.github.io/ngosen/apt >/dev/null; then
    echo "FAIL: apt_repo_file found a repository in disabled sources"
    failures=$((failures + 1))
fi

# A suite the installer picks but the repository lacks would break apt update on that machine.
installer_suites=$(grep -m1 -E '^ *bookworm \| ' "$top/install.sh" | tr -d ' )' | tr '|' '\n' | sort)
repo_suites=$(sed -n 's/^codenames="\(.*\)"$/\1/p' "$top/packaging/build-apt-repo.sh" | tr ' ' '\n' | sort)
if [ -z "$installer_suites" ] || [ "$installer_suites" != "$repo_suites" ]; then
    echo "FAIL: install.sh suites differ from the repository's:" "${installer_suites//$'\n'/ }" "/" "${repo_suites//$'\n'/ }"
    failures=$((failures + 1))
fi

[ "$failures" -eq 0 ] || exit 1
echo "all checks passed"
