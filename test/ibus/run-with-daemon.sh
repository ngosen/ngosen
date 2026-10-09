#!/bin/sh
# run-with-daemon.sh <component dir> <dictionary> <test script>: runs the test against a private
# ibus-daemon that starts the engine from the build tree. Run it under dbus-run-session.
set -e
tmp=$(mktemp -d)
export HOME="$tmp" XDG_CONFIG_HOME="$tmp/config" XDG_CACHE_HOME="$tmp/cache"
export IBUS_ADDRESS="unix:path=$tmp/ibus" IBUS_COMPONENT_PATH="$1" NGOSEN_IBUS_DICTIONARY="$2"
ibus-daemon --address="$IBUS_ADDRESS" --panel=disable --emoji-extension=disable --xim=false --config=disable --cache=none &
daemon=$!
trap 'kill $daemon 2>/dev/null; rm -rf "$tmp"' EXIT
python3 "$3"
