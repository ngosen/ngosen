#!/bin/bash
# Types Telex into web spreadsheets, moving between cells, and checks what the cells hold, with Ngó Sen on
# in Gõ Sen mode. Two ways to send the keys, both through the input method like real typing:
#
#   misc/sheet-test/run.sh [page...]           inside an X11 session, with xdotool
#   VM=<domain> GUEST=... OPEN=... misc/sheet-test/run.sh [page...]
#                                              on a libvirt host, into a VM on any session (Wayland too)
#
#   BROWSER=firefox|chromium   browser to test (default firefox); FIREFOX= and CHROMIUM= name the binaries
#   CASES="down click"         cases to run (default all, see README.md)
#   DELAY=120                  ms between keys; the "fast" case always uses 30
#   RUNS=1                     times to run each case
#
# VM mode, for a guest with this directory and misc/editor-test/server.py copied to KIT:
#   VM        libvirt domain; keys go through virsh send-key, clicks through QMP input-send-event
#   GUEST     host command that runs its argument as a shell command in the guest, e.g. "ssh root@vm"
#   OPEN      guest command that runs its arguments as a program in the user's desktop session
#   KIT       guest path of the copied misc/ directory (default /tmp/ngosen-kit/misc)
#   SCREEN    guest screen size in pixels (default 1280x800)
#   LIBVIRT   libvirt URI (default qemu:///system)
#   HOLD      ms each key stays down (default 80, as a person types; virsh alone releases far sooner)
#
# Prints one tab-separated line per run: page, case, expected, got, ok/FAIL/skip.
set -u
here=$(cd "$(dirname "$0")" && pwd)
browser=${BROWSER:-firefox}
port=8766
pages=${*:-handsontable jspreadsheet xspreadsheet aggrid luckysheet univer}
vm=${VM:-}

if [ -n "$vm" ]; then
    : "${GUEST:?set GUEST to a command that runs shell commands in the guest}" "${OPEN:?set OPEN}"
    kit=${KIT:-/tmp/ngosen-kit/misc}
    report=/tmp/ngosen-sheet-report.json
    # Expanded in the guest, as the desktop user; a snap browser cannot use /tmp or hidden folders.
    # shellcheck disable=SC2016
    profile='$HOME/ngosen-sheet-test'
    virsh=(virsh -c "${LIBVIRT:-qemu:///system}")
    IFS=x read -r screen_w screen_h <<< "${SCREEN:-1280x800}"
    guest() { $GUEST "$1"; }
    # pkill -f also sees the guest shell's own command line: the bracket keeps the pattern from matching
    # itself, and the server is started by a separate command.
    guest "pkill -f '[s]erver.py $kit/sheet-test/pages'; rm -f $report"
    guest "systemd-run --quiet --collect python3 $kit/editor-test/server.py $kit/sheet-test/pages $report $port"
    trap 'guest "pkill -f \"[s]erver.py $kit/sheet-test/pages\"; pkill -f ngosen-sheet-tes[t]"
        guest "$OPEN sh -c '"'"'rm -rf $profile'"'"'"' EXIT
else
    work=$(mktemp -d)
    report=$work/last.json
    profile=$work/profile
    python3 "$here/../editor-test/server.py" "$here/pages" "$report" "$port" &
    server=$!
    trap 'kill $server 2>/dev/null; pkill -f "$profile" 2>/dev/null; rm -rf "$work"' EXIT
fi

# "_" is Down, "@" is Up, "^" is Return, "~" is Tab, "#" is F2, "<" is BackSpace, "*" clicks cell A3.
keys_for() {
    case $1 in
    down) echo "tieengs_vieetj_nhaanj_ddaay_laf_caau_|tiếng|việt|nhận|đây|là|câu" ;;
    enter | fast) echo "tieengs^vieetj^nhaanj^ddaay^laf^caau^|tiếng|việt|nhận|đây|là|câu" ;;
    tab) echo "tieengs~vieetj~nhaanj~ddaay~laf^|tiếng|việt|nhận|đây|là" ;;
    click) echo "tieengs*vieetj^|tiếng|việt" ;;
    # Same moves without Vietnamese, to tell a sheet's own behaviour from the input method's.
    plainclick) echo "abc*xyz^|abc|xyz" ;;
    back) echo "toan <s^|toán" ;;
    esac
}

read_report() {
    if [ -n "$vm" ]; then guest "cat $report 2>/dev/null"; else cat "$report" 2>/dev/null; fi
}

field() {
    read_report | python3 -I -c 'import json, sys
try:
    v = json.load(sys.stdin)
    for k in sys.argv[1].split("."):
        v = v.get(k) if isinstance(v, dict) else None
    print(" ".join(map(str, v)) if isinstance(v, list) else ("" if v is None else v))
except ValueError:
    print("")' "$1"
}

# Kiosk mode puts the page at the screen's top left, so page positions are screen positions.
browser_command() {
    local url="http://127.0.0.1:$port/$1.html"
    if [ "$browser" = firefox ]; then
        echo "${FIREFOX:-firefox} --kiosk --profile $profile --no-remote $url"
    else
        echo "${CHROMIUM:-chromium} --kiosk --user-data-dir=$profile --password-store=basic --no-first-run --no-default-browser-check $url"
    fi
}

open_page() {
    if [ -n "$vm" ]; then
        # Two commands: the second names the profile, so pkill would match its own shell.
        guest "pkill -f ngosen-sheet-tes[t]; sleep 1.5"
        guest "$OPEN sh -c 'rm -rf $profile; mkdir -p $profile; exec $(browser_command "$1")'"
        return
    fi
    pkill -f "$profile" 2>/dev/null
    sleep 1.5
    rm -rf "$profile"
    mkdir -p "$profile"
    # shellcheck disable=SC2091
    $(browser_command "$1") >/dev/null 2>&1 &
}

# Waits for a report from a page loaded after the last one.
wait_ready() {
    local old=$1 id t
    for ((i = 0; i < 80; i++)); do
        id=$(field id)
        t=$(field t)
        if [ -n "$id" ] && [ "$id" != "$old" ] && [[ $t != ERR* ]]; then
            return 0
        fi
        sleep 0.5
    done
    return 1
}

qmp() {
    "${virsh[@]}" qemu-monitor-command "$vm" "{\"execute\":\"input-send-event\",\"arguments\":{\"events\":[$1]}}" >/dev/null
}

click_cell() {
    local at x y
    at=$(field "click.$1")
    [ -n "$at" ] || return 1
    read -r x y <<< "$at"
    if [ -z "$vm" ]; then
        xdotool mousemove "$x" "$y" click 1
        return
    fi
    qmp "{\"type\":\"abs\",\"data\":{\"axis\":\"x\",\"value\":$((x * 32767 / (screen_w - 1)))}},{\"type\":\"abs\",\"data\":{\"axis\":\"y\",\"value\":$((y * 32767 / (screen_h - 1)))}}"
    sleep 0.2
    qmp '{"type":"btn","data":{"down":true,"button":"left"}}'
    sleep 0.08
    qmp '{"type":"btn","data":{"down":false,"button":"left"}}'
}

# One key: an xdotool keysym, or the evdev name virsh send-key takes.
key_name() {
    if [ -n "$vm" ]; then
        case $1 in
        "_") echo KEY_DOWN ;; "@") echo KEY_UP ;; "^") echo KEY_ENTER ;; "~") echo KEY_TAB ;;
        "#") echo KEY_F2 ;; "<") echo KEY_BACKSPACE ;; " ") echo KEY_SPACE ;;
        *) echo "KEY_${1^^}" ;;
        esac
        return
    fi
    case $1 in
    "_") echo Down ;; "@") echo Up ;; "^") echo Return ;; "~") echo Tab ;;
    "#") echo F2 ;; "<") echo BackSpace ;; " ") echo space ;;
    *) echo "$1" ;;
    esac
}

# virsh send-key cannot space keys closer than one call, so in VM mode "fast" is only as fast as that.
send_keys() {
    local delay=$1
    shift
    [ $# -gt 0 ] || return 0
    if [ -z "$vm" ]; then
        xdotool key --delay "$delay" "$@"
        return
    fi
    for k in "$@"; do
        "${virsh[@]}" send-key "$vm" --holdtime "${HOLD:-80}" "$k" >/dev/null
        sleep "$(printf '%d.%03d' $((delay / 1000)) $((delay % 1000)))"
    done
}

type_keys() {
    local text=$1 delay=$2 args=() c
    for ((i = 0; i < ${#text}; i++)); do
        c=${text:i:1}
        if [ "$c" = "*" ]; then
            send_keys "$delay" "${args[@]}"
            args=()
            sleep 0.3
            click_cell a3
            sleep 0.3
        else
            args+=("$(key_name "$c")")
        fi
    done
    send_keys "$delay" "${args[@]}"
}

for page in $pages; do
    for c in ${CASES:-down enter tab fast click back}; do
        spec=$(keys_for "$c")
        keys=${spec%%|*}
        want=${spec#*|}
        delay=${DELAY:-120}
        [ "$c" = fast ] && delay=30
        for ((run = 0; run < ${RUNS:-1}; run++)); do
            old=$(field id)
            open_page "$page"
            if ! wait_ready "$old"; then
                printf '%s\t%s\t%s\t-\tNOLOAD\n' "$page" "$c" "$want"
                continue
            fi
            if [ "$c" = down ] && [ "$(field arrows)" != True ]; then
                printf '%s\t%s\t%s\t-\tskip\n' "$page" "$c" "$want"
                break
            fi
            sleep 2
            click_cell a1
            sleep 0.5
            type_keys "$keys" "$delay"
            sleep 1.5
            got=$(field t)
            verdict=FAIL
            [ "$got" = "$want" ] && verdict=ok
            printf '%s\t%s\t%s\t%s\t%s\n' "$page" "$c" "$want" "$got" "$verdict"
        done
    done
done
