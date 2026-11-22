#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The settings window imports its top-level modules by name, so each one must be installed next to
# main.py; a missing one only shows when a user opens the window.
set -u
build=${1:?usage: settings-gui-install.sh <build dir> <source dir>}
src=${2:?usage: settings-gui-install.sh <build dir> <source dir>}
dest=$(mktemp -d)
trap 'rm -rf "$dest"' EXIT
DESTDIR=$dest cmake --install "$build" >/dev/null || exit 1
gui=$(find "$dest" -type d -path '*/fcitx5-ngosen/settings-gui' | head -1)
[ -n "$gui" ] || {
    echo "settings-gui not installed" >&2
    exit 1
}
fail=0
for f in "$src"/settings-gui/*.py; do
    name=$(basename "$f")
    [ -f "$gui/$name" ] || {
        echo "not installed: $name" >&2
        fail=1
    }
done
# Each command in bin runs a script from the settings window's directory.
for cmd in fcitx5-ngosen-settings ngosen-check ngosen-save-log ngosen-toggle; do
    launcher=$(find "$dest" -path "*/bin/$cmd" | head -1)
    if [ ! -x "$launcher" ]; then
        echo "not installed: bin/$cmd" >&2
        fail=1
        continue
    fi
    script=$(sed -n 's/^exec "\([^"]*\)".*/\1/p' "$launcher")
    [ -x "$dest$script" ] || {
        echo "bin/$cmd runs a missing script: $script" >&2
        fail=1
    }
done
exit $fail
