#!/bin/bash
# Downloads the files matching PATTERN from the LIMIT newest releases of REPO into DIR, and checks
# each one against the SHA256SUMS of its release first, because the repositories sign whatever they
# are given and release files can be replaced after publishing.
#
#   packaging/download-release-files.sh [--new-edition] REPO LIMIT PATTERN DIR
#
# --new-edition is for an edition that may not have released yet: no release is skipped, but a
# release without a matching file fails, so a broken release cannot drop it from the repositories
# unnoticed. Without it, no release fails and a release may lack the pattern, as old ibus-ngosen
# releases lack openSUSE packages.
set -euo pipefail
new_edition=false
if [ "${1:-}" = --new-edition ]; then
    new_edition=true
    shift
fi
repo=$1 limit=$2 pattern=$3 dir=$4
mkdir -p "$dir"
tags=$(gh release list --repo "$repo" --exclude-drafts --exclude-pre-releases --limit "$limit" \
    --json tagName --jq '.[].tagName')
if [ -z "$tags" ]; then
    if "$new_edition"; then
        echo "no published release in $repo yet, skipping it"
        exit 0
    fi
    echo "no published release in $repo" >&2
    exit 1
fi
for tag in $tags; do
    tmp=$(mktemp -d)
    gh release download "$tag" --repo "$repo" --pattern "$pattern" --pattern SHA256SUMS --dir "$tmp"
    found=0
    for file in "$tmp"/*; do
        name=$(basename "$file")
        [ "$name" = SHA256SUMS ] && continue
        sum=$(awk -v f="$name" '$2 == f || $2 == "*" f { print $1 }' "$tmp/SHA256SUMS")
        if [ -z "$sum" ]; then
            echo "$repo $tag: $name is not in SHA256SUMS" >&2
            exit 1
        fi
        echo "$sum  $file" | sha256sum --check --quiet -
        mv "$file" "$dir/"
        found=$((found + 1))
    done
    rm -rf "$tmp"
    if "$new_edition" && [ "$found" = 0 ]; then
        echo "$repo $tag has no file matching $pattern" >&2
        exit 1
    fi
done
