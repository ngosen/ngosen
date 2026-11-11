#!/bin/bash
# Types Telex into web spreadsheets, moving between cells, and checks what the cells hold. Run it inside
# an X11 session with Ngó Sen on in Gõ Sen mode; it types and clicks with xdotool, so the keys go through
# the input method.
#
#   misc/sheet-test/run.sh [page...]           all pages when none are given
#
#   BROWSER=firefox|chromium   browser to test (default firefox)
#   CASES="down click"         cases to run (default all, see README.md)
#   DELAY=120                  ms between keys; the "fast" case always uses 30
#   RUNS=1                     times to run each case
#
# Prints one tab-separated line per run: page, case, expected, got, ok/FAIL/skip.
set -u
here=$(cd "$(dirname "$0")" && pwd)
browser=${BROWSER:-firefox}
port=8766
work=$(mktemp -d)
report=$work/last.json
profile=$work/profile
pages=${*:-handsontable jspreadsheet xspreadsheet aggrid luckysheet univer}

python3 "$here/../editor-test/server.py" "$here/pages" "$report" "$port" &
server=$!
trap 'kill $server 2>/dev/null; pkill -f "$profile" 2>/dev/null; rm -rf "$work"' EXIT

# "_" is Down, "@" is Up, "^" is Return, "~" is Tab, "#" is F2, "<" is BackSpace, "*" clicks cell A3.
keys_for() {
    case $1 in
    down) echo "tieengs_vieetj_nhaanf_ddaay_laf_caau_|tiếng|việt|nhận|đây|là|câu" ;;
    enter | fast) echo "tieengs^vieetj^nhaanf^ddaay^laf^caau^|tiếng|việt|nhận|đây|là|câu" ;;
    tab) echo "tieengs~vieetj~nhaanf~ddaay~laf^|tiếng|việt|nhận|đây|là" ;;
    click) echo "tieengs*vieetj^|tiếng|việt" ;;
    edit) echo "toan^@#s^|toán" ;;
    esac
}

field() {
    python3 -I -c 'import json, sys
try:
    v = json.load(open(sys.argv[1], encoding="utf-8"))
    for k in sys.argv[2].split("."):
        v = v.get(k) if isinstance(v, dict) else None
    print(" ".join(map(str, v)) if isinstance(v, list) else ("" if v is None else v))
except (OSError, ValueError):
    print("")' "$report" "$1"
}

open_page() {
    pkill -f "$profile" 2>/dev/null
    sleep 1.5
    rm -rf "$profile"
    mkdir -p "$profile"
    # Kiosk mode puts the page at the screen's top left, so page positions are screen positions.
    if [ "$browser" = firefox ]; then
        firefox --kiosk --profile "$profile" --no-remote "http://127.0.0.1:$port/$1.html" >/dev/null 2>&1 &
    else
        chromium --kiosk --user-data-dir="$profile" --password-store=basic --no-first-run --no-default-browser-check \
            "http://127.0.0.1:$port/$1.html" >/dev/null 2>&1 &
    fi
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

click_cell() {
    local at
    at=$(field "click.$1")
    [ -n "$at" ] || return 1
    # shellcheck disable=SC2086
    xdotool mousemove $at click 1
}

type_keys() {
    local text=$1 delay=$2 args=() c
    for ((i = 0; i < ${#text}; i++)); do
        c=${text:i:1}
        case $c in
        "_") args+=(Down) ;;
        "@") args+=(Up) ;;
        "^") args+=(Return) ;;
        "~") args+=(Tab) ;;
        "#") args+=(F2) ;;
        "<") args+=(BackSpace) ;;
        " ") args+=(space) ;;
        "*")
            [ ${#args[@]} -gt 0 ] && xdotool key --delay "$delay" "${args[@]}"
            args=()
            sleep 0.3
            click_cell a3
            sleep 0.3
            ;;
        *) args+=("$c") ;;
        esac
    done
    [ ${#args[@]} -gt 0 ] && xdotool key --delay "$delay" "${args[@]}"
}

for page in $pages; do
    for c in ${CASES:-down enter tab fast click edit}; do
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
