#!/bin/bash
# Downloads the files matching PATTERN from the LIMIT newest releases of REPO into DIR, and checks
# each one against the SHA256SUMS of its release first, because the repositories sign whatever they
# are given and release files can be replaced after publishing.
#
#   packaging/download-release-files.sh REPO LIMIT PATTERN DIR
set -euo pipefail
repo=$1 limit=$2 pattern=$3 dir=$4
mkdir -p "$dir"
tags=$(gh release list --repo "$repo" --exclude-drafts --exclude-pre-releases --limit "$limit" \
    --json tagName --jq '.[].tagName')
if [ -z "$tags" ]; then
    echo "no published release in $repo" >&2
    exit 1
fi
for tag in $tags; do
    tmp=$(mktemp -d)
    gh release download "$tag" --repo "$repo" --pattern "$pattern" --pattern SHA256SUMS --dir "$tmp"
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
    done
    rm -rf "$tmp"
done
