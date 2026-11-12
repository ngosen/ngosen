#!/bin/bash
# Runs a program as USER inside that user's desktop session, with the environment of a process of that
# session (gnome-shell, plasmashell, cinnamon, ...) plus the user's systemd environment. For run.sh's
# OPEN in a VM, run as root:
#
#   in-session.sh USER PROCESS program [args...]
set -u
user=$1 proc=$2
shift 2
pid=$(pgrep -u "$user" -x "$proc" | head -1)
[ -n "$pid" ] || { echo "no $proc for $user" >&2; exit 1; }
env_args=()
while IFS= read -r -d '' kv; do env_args+=("$kv"); done < "/proc/$pid/environ"
# A compositor's own environment lacks the display variables its clients get; systemd has them.
while IFS= read -r kv; do env_args+=("$kv"); done < <(runuser -u "$user" -- env XDG_RUNTIME_DIR="/run/user/$(id -u "$user")" systemctl --user show-environment 2>/dev/null)
# A transient unit outlives the guest agent command; a background child would be killed with it.
systemd-run --quiet --collect --uid="$user" env -i "${env_args[@]}" "$@"
