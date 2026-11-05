#!/bin/bash
# Runs install.sh in a throwaway container: install, uninstall, then a reinstall over an fcitx5
# that is running without the addon. Checks packages and the user's files, not typing.
#
#   test/install-smoke.sh [--no-recommends] x11|gnome|kde|sway|hyprland <release-dir>
#
# Run as root in a fresh Debian, Ubuntu or Fedora container; <release-dir> holds the packages and
# SHA256SUMS laid out as in a release. It changes the system, so never run it on a real machine.

# check runs the helpers below by name.
# shellcheck disable=SC2329
set -euo pipefail

usage() {
    echo "usage: $0 [--no-recommends] x11|gnome|kde|sway|hyprland <release-dir>" >&2
    exit 2
}

no_recommends=0
if [ "${1:-}" = --no-recommends ]; then
    no_recommends=1
    shift
fi
[ $# -eq 2 ] || usage
session=$1
release_dir=$(realpath "$2")
case "$session" in
    x11 | gnome | kde | sway | hyprland) ;;
    *) usage ;;
esac
[ "$(id -u)" -eq 0 ] || {
    echo "must run as root inside a container" >&2
    exit 2
}

user=tester
home=/home/$user
installer_lang=
work=/srv/install-smoke
failures=0
bus=
stub_path=
compositor_env=()
desktop_config=
autostart_line=
case "$session" in
    sway)
        compositor_env=(WAYLAND_DISPLAY=wayland-1 SWAYSOCK=/tmp/sway-ipc.sock)
        desktop_config=$home/.config/sway/config
        autostart_line='exec fcitx5 -d'
        ;;
    hyprland)
        compositor_env=(WAYLAND_DISPLAY=wayland-1 HYPRLAND_INSTANCE_SIGNATURE=test)
        desktop_config=$home/.config/hypr/hyprland.lua
        autostart_line='hl.on("hyprland.start", function() hl.exec_cmd("fcitx5 -d") end)'
        ;;
esac

check() {
    local name=$1
    shift
    if "$@"; then
        echo "PASS: $name"
    else
        echo "FAIL: $name"
        failures=$((failures + 1))
    fi
}

finish() {
    if [ "$failures" -gt 0 ]; then
        echo "$failures check(s) failed"
        exit 1
    fi
    echo "all checks passed"
    exit 0
}

prepare_system() {
    if command -v apt-get >/dev/null 2>&1; then
        # MX Linux ships with recommends off, so fcitx5 arrives without its GTK and Qt modules.
        if [ "$no_recommends" -eq 1 ]; then
            echo 'APT::Install-Recommends "0";' >/etc/apt/apt.conf.d/99no-recommends
        fi
        apt-get update -q
        DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
            sudo curl ca-certificates procps dbus dbus-bin
        # apt translates what it prints, and most users run a Vietnamese desktop.
        if [ "$no_recommends" -eq 1 ]; then
            DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends locales
            localedef -i vi_VN -f UTF-8 vi_VN.UTF-8
            installer_lang=vi_VN.UTF-8
        fi
        # Like a fresh Kali image, so the installer has to fetch the lists itself.
        rm -rf /var/lib/apt/lists/*
    else
        dnf install -y -q sudo curl procps-ng dbus-daemon dbus-tools shadow-utils
        # The installer recognises KDE by kreadconfig6 and kwriteconfig6.
        [ "$session" = kde ] && dnf install -y -q kf6-kconfig
    fi
    useradd -m "$user"
    echo "$user ALL=(ALL) NOPASSWD: ALL" >/etc/sudoers.d/"$user"
    mkdir -p "$work"
    cp -r "$release_dir" "$work/release"
    cp "$(dirname "$0")/../install.sh" "$work/install.sh"
    chmod -R a+rX "$work"
}

as_user() {
    local desktop=XFCE type=x11
    case "$session" in
        gnome) desktop=GNOME type=wayland ;;
        kde) desktop=KDE type=wayland ;;
        sway) desktop=sway:wlroots type=wayland ;;
        hyprland) desktop=Hyprland type=wayland ;;
    esac
    sudo -u "$user" -H env XDG_SESSION_TYPE="$type" XDG_CURRENT_DESKTOP="$desktop" "${compositor_env[@]}" \
        NGOSEN_RELEASE_URL="file://$work/release" ${bus:+DBUS_SESSION_BUS_ADDRESS="$bus"} \
        ${stub_path:+PATH="$stub_path"} "$@"
}

installer() {
    as_user env ${installer_lang:+LANG="$installer_lang"} bash "$work/install.sh" "$@"
}

is_installed() {
    if command -v dpkg-query >/dev/null 2>&1; then
        [ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null)" = "install ok installed" ]
    else
        rpm -q "$1" >/dev/null 2>&1
    fi
}

is_removed() { ! is_installed "$1"; }

has_line() { grep -qFx -- "$2" "$1" 2>/dev/null; }
lacks_line() { ! has_line "$@"; }
lacks_keyboard() { ! grep -qs '^Name=keyboard-' "$1"; }

profile=$home/.config/fcitx5/profile
config=$home/.config/fcitx5/config
im_env=$home/.config/environment.d/90-ngosen.conf
compositor_env_file=$home/.config/environment.d/90-ngosen-sway-hyprland.conf
autostart=$home/.config/autostart/org.fcitx.Fcitx5.desktop
hidden_launcher=$home/.local/share/applications/org.fcitx.Fcitx5.desktop
kwinrc=$home/.config/kwinrc
kwin_im='InputMethod=/usr/share/applications/org.fcitx.Fcitx5.desktop'

# Asks the installed launcher, as the user on this desktop, which window it would open.
settings_toolkit() {
    as_user python3 -c 'import sys; sys.path.insert(0, sys.argv[1]); import main; print(main.toolkit())' \
        /usr/share/fcitx5-ngosen/settings-gui
}

opens_settings_in() { [ "$(settings_toolkit)" = "$1" ]; }

has_no_qt() {
    as_user python3 -c 'import sys; sys.path.insert(0, sys.argv[1]); import main; sys.exit(main.has_qt())' \
        /usr/share/fcitx5-ngosen/settings-gui
}

check_packages() {
    check "fcitx5-ngosen is installed" is_installed fcitx5-ngosen
    if [ "$session" = gnome ]; then
        check "the settings window opens in GTK" opens_settings_in gtk
        check "no Qt is installed for it" has_no_qt
    else
        check "the settings window opens in Qt" opens_settings_in qt
    fi
    if command -v apt-get >/dev/null 2>&1; then
        check "im-config is installed" is_installed im-config
        check "whiptail is installed" is_installed whiptail
        check "fcitx5-frontend-gtk3 is installed" is_installed fcitx5-frontend-gtk3
    fi
}

check_session_files() {
    if [ "$session" = x11 ]; then
        check ".xinputrc runs fcitx5" has_line "$home/.xinputrc" 'run_im fcitx5'
    elif [ "$session" = kde ]; then
        check "KWin starts fcitx5 as its input method" has_line "$kwinrc" "$kwin_im"
        check "XWayland apps find fcitx5 through XIM" has_line "$im_env" 'XMODIFIERS=@im=fcitx'
        check "environment.d leaves the GTK module unset" lacks_line "$im_env" 'GTK_IM_MODULE=fcitx'
    elif [ -n "$desktop_config" ]; then
        check "fcitx5 is running after install" fcitx5_running
        check "the final message names the toggle key" install_names Ctrl+Shift
        check "the final message has the autostart line" has_autostart_line
        check "the desktop config is untouched" desktop_config_untouched
    else
        check "environment.d selects fcitx" has_line "$im_env" 'GTK_IM_MODULE=fcitx'
        check "fcitx5 starts at login" test -e "$autostart"
        check "the Fcitx 5 launcher is hidden" has_line "$hidden_launcher" 'NoDisplay=true'
    fi
    check "profile lists ngosen" has_line "$profile" 'Name=ngosen'
    check "profile has no keyboard next to ngosen" lacks_keyboard "$profile"
    check "trigger keys are left to fcitx5" lacks_line "$config" '0=Control+Shift_L'
}

check_uninstalled_state() {
    check "fcitx5-ngosen is removed" is_removed fcitx5-ngosen
    if [ "$session" = x11 ]; then
        check ".xinputrc no longer runs fcitx5" lacks_line "$home/.xinputrc" 'run_im fcitx5'
        check ".xinputrc is removed" test ! -e "$home/.xinputrc"
    elif [ "$session" = kde ]; then
        # fcitx5 may serve other input methods, so KDE keeps it.
        check "KWin still starts fcitx5" has_line "$kwinrc" "$kwin_im"
        check "XWayland apps still find fcitx5" has_line "$im_env" 'XMODIFIERS=@im=fcitx'
    elif [ -n "$desktop_config" ]; then
        check "uninstall names the autostart line" grep -qF 'fcitx5 -d' "$install_log"
        check "the desktop config is still untouched" desktop_config_untouched
        check "uninstall removes the variables it wrote" test ! -e "$compositor_env_file"
    else
        check "environment.d file is removed" test ! -e "$im_env"
        check "autostart entry is removed" test ! -e "$autostart"
        check "hidden launcher is removed" test ! -e "$hidden_launcher"
    fi
    # fcitx5's own settings are the user's and stay.
    check "profile still lists ngosen" has_line "$profile" 'Name=ngosen'
}

# A container's first process may never reap the daemon, so a zombie counts as stopped.
fcitx5_running() {
    ps -u "$user" -o stat=,comm= | awk '$2 == "fcitx5" && $1 !~ /^Z/ { found = 1 } END { exit !found }'
}

# Arguments are extra environment variables for fcitx5.
start_fcitx5() {
    local tries=40
    as_user env "$@" fcitx5 -d >/dev/null 2>&1
    # The profile is loaded before fcitx5 takes its D-Bus name.
    until as_user dbus-send --session --print-reply --dest=org.freedesktop.DBus / \
        org.freedesktop.DBus.NameHasOwner string:org.fcitx.Fcitx5 2>/dev/null | grep -q 'boolean true'; do
        tries=$((tries - 1))
        [ "$tries" -gt 0 ] || return 1
        sleep 0.25
    done
}

stop_fcitx5() {
    local tries=40
    pkill -x -u "$user" fcitx5 || return 0
    while fcitx5_running; do
        tries=$((tries - 1))
        [ "$tries" -gt 0 ] || return 1
        sleep 0.25
    done
}

# An fcitx5 left running after --uninstall writes a profile without ngosen when it exits; the
# reinstall has to land its profile after that write.
check_reinstall_over_running_fcitx5() {
    local saved=$work/profile.saved
    # dnf removes fcitx5 along with the package; a user who installed fcitx5 directly keeps it.
    command -v fcitx5 >/dev/null || dnf install -y -q fcitx5
    bus=$(sudo -u "$user" dbus-daemon --session --fork --print-address=1 | head -n 1)

    cp "$profile" "$saved"
    check "fcitx5 starts without the addon" start_fcitx5
    check "fcitx5 stops" stop_fcitx5
    check "precondition: fcitx5 without the addon drops ngosen" lacks_line "$profile" 'Name=ngosen'
    cp "$saved" "$profile"

    check "fcitx5 starts again" start_fcitx5
    check "reinstall over a running fcitx5 succeeds" install_logged
    if [ "$session" = gnome ]; then
        check "a running fcitx5 after --uninstall still gets the explanation" explains_gnome_setup
    fi
    check "fcitx5 stops after the reinstall" stop_fcitx5
    check "profile lists ngosen after fcitx5 exits" has_line "$profile" 'Name=ngosen'

    pkill -u "$user" dbus-daemon || :
    bus=
}

addon_state() { stat -c '%i %Z' "$(find /usr/lib /usr/lib64 -maxdepth 3 -name libngosen.so | head -n 1)"; }
fcitx5_pid() { pgrep -u "$user" -x fcitx5 | head -n 1; }

check_second_run() {
    local addon pid
    bus=$(sudo -u "$user" dbus-daemon --session --fork --print-address=1 | head -n 1)
    check "fcitx5 starts before the second run" start_fcitx5
    pid=$(fcitx5_pid)
    addon=$(addon_state)
    check "a second run succeeds" install_logged
    check "it says this version is already installed" grep -qF "đã được cài từ trước" "$install_log"
    check "it leaves the package as it is" test "$(addon_state)" = "$addon"
    check "it leaves fcitx5 running" test "$(fcitx5_pid)" = "$pid"
    # An addon written after fcitx5 started has not been loaded yet.
    sleep 1
    touch "$(find /usr/lib /usr/lib64 -maxdepth 3 -name libngosen.so | head -n 1)"
    check "a run after the addon changed succeeds" install_logged
    check "it restarts fcitx5" test "$(fcitx5_pid)" != "$pid"
    check "it does not say nothing changed" refuses grep -qF "không có gì thay đổi" "$install_log"
    check "fcitx5 stops after the second run" stop_fcitx5
    pkill -u "$user" dbus-daemon || :
    bus=
}

# A profile from before Ngó Sen had its own toggle key lists a keyboard next to it, and fcitx5's
# Ctrl+Space switched between them; the first start of this version drops the keyboard.
check_old_profile_cleanup() {
    local settings=$home/.config/fcitx5/conf/ngosen.conf
    bus=$(sudo -u "$user" dbus-daemon --session --fork --print-address=1 | head -n 1)
    as_user mkdir -p "$(dirname "$settings")"
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=ngosen\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    printf 'InputMethod=Telex\n' | as_user tee "$settings" >/dev/null

    check "fcitx5 starts with the old profile" start_fcitx5
    check "fcitx5 stops" stop_fcitx5
    check "the keyboard next to ngosen is dropped" lacks_keyboard "$profile"
    check "ngosen stays in the profile" has_line "$profile" 'Name=ngosen'
    check "older settings keep Ctrl+Space" has_line "$settings" '0=Control+space'

    printf '\n[Groups/0/Items/1]\nName=keyboard-us\nLayout=\n' | as_user tee -a "$profile" >/dev/null
    check "fcitx5 starts with the keyboard added back" start_fcitx5
    check "fcitx5 stops again" stop_fcitx5
    check "a keyboard added back stays" has_line "$profile" 'Name=keyboard-us'

    # fcitx5 switches only between two input methods, so its own switch key moves to Ngó Sen.
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=ngosen\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    printf 'InputMethod=Telex\n' | as_user tee "$settings" >/dev/null
    printf '[Hotkey/TriggerKeys]\n0=Super+space\n' | as_user tee "$config" >/dev/null
    check "fcitx5 starts with its own switch key" start_fcitx5
    check "fcitx5 stops once more" stop_fcitx5
    check "fcitx5's switch key becomes the toggle key" has_line "$settings" '0=Super+space'
    as_user rm -f "$config"

    # Users who never changed a setting have no ngosen.conf at all.
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=ngosen\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    as_user rm -f "$settings"
    check "fcitx5 starts with an old profile and no settings" start_fcitx5
    check "fcitx5 stops after that" stop_fcitx5
    check "the keyboard is dropped without settings" lacks_keyboard "$profile"
    check "no settings keep Ctrl+Space" has_line "$settings" '0=Control+space'

    # A new user who adds Ngó Sen in the configtool starts fcitx5 before Ngó Sen ever loads.
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    as_user rm -f "$settings"
    check "fcitx5 starts before Ngó Sen is added" start_fcitx5
    check "fcitx5 stops before Ngó Sen is added" stop_fcitx5
    printf '\n[Groups/0/Items/1]\nName=ngosen\nLayout=\n' | as_user tee -a "$profile" >/dev/null
    check "fcitx5 starts with Ngó Sen just added" start_fcitx5
    check "fcitx5 stops with Ngó Sen just added" stop_fcitx5
    check "a new user keeps the keyboard" has_line "$profile" 'Name=keyboard-us'
    check "a new user gets Ctrl+Shift" lacks_line "$settings" '0=Control+space'

    # Without a profile fcitx5 builds one from the input methods listed for the desktop language.
    as_user rm -f "$profile" "$settings"
    # Some images (LMDE) set LC_ALL and LANGUAGE, which override LANG.
    check "fcitx5 starts on a Vietnamese desktop without a profile" start_fcitx5 -u LC_ALL -u LANGUAGE LANG=vi_VN.UTF-8
    check "fcitx5 stops on the Vietnamese desktop" stop_fcitx5
    check "a Vietnamese desktop gets ngosen in its first profile" has_line "$profile" 'Name=ngosen'
    check "a first profile keeps Ctrl+Shift" lacks_line "$settings" '0=Control+space'

    pkill -u "$user" dbus-daemon || :
    bus=
}

install_log=$work/install.log
install_logged() { installer --yes >"$install_log" 2>&1; }
# Keeps the output in the CI log too, for when the install fails.
install_shown() {
    installer --yes 2>&1 | tee "$install_log"
    return "${PIPESTATUS[0]}"
}
install_names() { grep -q "Bấm $1 " "$install_log"; }
explains_gnome_setup() { grep -qF "Lệnh sẽ đặt fcitx5 thay bộ gõ IBus" "$install_log"; }
lacks_gnome_explanation() { ! explains_gnome_setup; }
has_autostart_line() { grep -qF -- "$autostart_line" "$install_log"; }
lacks_autostart_line() { ! has_autostart_line; }
says_typing_works() { grep -qF "gõ được ngay" "$install_log"; }
leaves_steps_to_the_user() { grep -qF "Việc còn lại" "$install_log"; }
desktop_config_untouched() { [ "$(sha256sum "$desktop_config")" = "$desktop_config_hash" ]; }
lists_undone_under_a_heading() { grep -A1 -F "Đã trả lại những gì lệnh cài đã đặt:" "$install_log" | grep -q '^  - '; }
fcitx5_stopped() { ! fcitx5_running; }

# Without a running fcitx5 the installer names the key before the addon has run, so it must
# predict whether the keyboard goes.
check_toggle_key_message() {
    local settings=$home/.config/fcitx5/conf/ngosen.conf
    as_user rm -f "$settings" "$home/.xinputrc"
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=ngosen\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    check "install with an old profile succeeds" install_logged
    check "it names Ctrl+Space, kept for the lone keyboard" install_names Ctrl+Space
    check "no fcitx5 is started for the lone keyboard" fcitx5_stopped

    as_user rm -f "$settings" "$home/.xinputrc"
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=mozc\nLayout=\n\n[Groups/0/Items/2]\nName=ngosen\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    check "install next to another input method succeeds" install_logged
    check "it names Ctrl+Shift, as the keyboard stays" install_names Ctrl+Shift
    check "no fcitx5 is started next to another input method" fcitx5_stopped
}

check_edited_environment_kept() {
    echo 'MOZ_ENABLE_WAYLAND=1' | as_user tee -a "$im_env" >/dev/null
    check "uninstall after the edit succeeds" installer --yes --uninstall
    check "the edited environment.d file stays" has_line "$im_env" 'MOZ_ENABLE_WAYLAND=1'
}

changes=$home/.local/state/ngosen/install-changes
enabled_extensions=$home/.stub-enabled-extensions
uninstalled_extensions=$home/.stub-uninstalled-extensions
installed_extensions=$home/.stub-installed-extensions
reloads=$home/.stub-reloads
last_reload() { [ "$(tail -n 1 "$reloads" 2>/dev/null)" = "$1" ]; }

# Stand-ins for GNOME 50.1's tools: the lists of enabled and installed extensions are files, and
# the Input Method Panel counts as installed by the user unless the installed list exists.
use_gnome_stubs() {
    local dir=$work/stubs
    mkdir -p "$dir"
    cat >"$dir/gsettings" <<'STUB'
#!/bin/bash
state=$HOME/.stub-enabled-extensions
case "$1 $2 $3" in
    "get org.gnome.shell enabled-extensions") cat "$state" 2>/dev/null || echo "@as []" ;;
    "set org.gnome.shell enabled-extensions") printf '%s\n' "$4" >"$state" ;;
    "get org.gnome.desktop.input-sources sources") echo "[('xkb', 'us')]" ;;
    *) exit 1 ;;
esac
STUB
    printf '#!/bin/sh\necho "GNOME Shell 50.1"\n' >"$dir/gnome-shell"
    cat >"$dir/fcitx5" <<'STUB'
#!/bin/bash
if [ "${1:-}" = --version ]; then
    cat "$HOME/.stub-fcitx5-version" 2>/dev/null || echo 5.1.19
else
    exec /usr/bin/fcitx5 "$@"
fi
STUB
    cat >"$dir/gnome-extensions" <<'STUB'
#!/bin/bash
case "$1" in
    list)
        if [ "${2:-}" = --enabled ]; then
            grep -o "'[^']*'" "$HOME/.stub-enabled-extensions" 2>/dev/null | tr -d "'" || :
        else
            cat "$HOME/.stub-installed-extensions" 2>/dev/null || echo kimpanel@kde.org
        fi ;;
    install) echo kimpanel@kde.org >>"$HOME/.stub-installed-extensions" ;;
    uninstall) echo "$2" >>"$HOME/.stub-uninstalled-extensions" ;;
    *) exit 1 ;;
esac
STUB
    # Records each reload of the user manager and whether the fcitx5 environment existed then.
    cat >"$dir/systemctl" <<'STUB'
#!/bin/bash
if [ "$*" = "--user daemon-reload" ]; then
    if [ -e "$HOME/.config/environment.d/90-ngosen.conf" ]; then
        echo with-fcitx5 >>"$HOME/.stub-reloads"
    else
        echo without-fcitx5 >>"$HOME/.stub-reloads"
    fi
    exit 0
fi
[ -x /usr/bin/systemctl ] && exec /usr/bin/systemctl "$@"
exit 1
STUB
    # Only extensions.gnome.org is faked; the packages still come through the real curl.
    cat >"$dir/curl" <<'STUB'
#!/bin/bash
case "$*" in
    *extension-info*) echo '{"download_url": "/download-extension/kimpanel@kde.org.shell-extension.zip"}' ;;
    *download-extension*) while [ "$1" != -o ]; do shift; done; : >"$2" ;;
    *) exec /usr/bin/curl "$@" ;;
esac
STUB
    chmod a+rx "$dir"/*
    stub_path=$dir:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
}

extension_on() { grep -qF "'$1'" "$enabled_extensions"; }
extension_off() { ! extension_on "$1"; }

check_extensions_turned_back() {
    use_gnome_stubs
    as_user rm -f "$changes" "$enabled_extensions" "$uninstalled_extensions" "$im_env" "$reloads"
    check "install on GNOME 50.1 succeeds" installer --yes
    check "install has the user manager reread the environment" last_reload with-fcitx5
    check "install turns forward-keys on" extension_on forward-keys@ngosen.github.io
    check "install turns the Input Method Panel on" extension_on kimpanel@kde.org
    check "uninstall on GNOME 50.1 succeeds" installer --yes --uninstall
    check "uninstall has the user manager reread the environment" last_reload without-fcitx5
    check "uninstall turns forward-keys off" extension_off forward-keys@ngosen.github.io
    check "uninstall turns the Input Method Panel off" extension_off kimpanel@kde.org
    check "an extension the install did not add stays installed" test ! -e "$uninstalled_extensions"

    echo "['forward-keys@ngosen.github.io', 'kimpanel@kde.org']" | as_user tee "$enabled_extensions" >/dev/null
    check "install over extensions the user turned on succeeds" installer --yes
    check "uninstall after that succeeds" installer --yes --uninstall
    check "forward-keys the user turned on stays on" extension_on forward-keys@ngosen.github.io
    check "the Input Method Panel the user turned on stays on" extension_on kimpanel@kde.org
    check "and stays installed" test ! -e "$uninstalled_extensions"

    # A GNOME that keeps forwarded keys still needs the extension for app names until fcitx5 5.1.22.
    as_user rm -f "$changes" "$enabled_extensions"
    printf '#!/bin/sh\necho "GNOME Shell 50.4"\n' >"$work/stubs/gnome-shell"
    check "install on GNOME 50.4 succeeds" installer --yes
    check "it turns forward-keys on for app names" extension_on forward-keys@ngosen.github.io
    check "uninstall after that succeeds" installer --yes --uninstall
    echo 5.1.22 | as_user tee "$home/.stub-fcitx5-version" >/dev/null
    check "install on GNOME 50.4 with fcitx5 5.1.22 succeeds" installer --yes
    check "it leaves forward-keys off" extension_off forward-keys@ngosen.github.io
    check "uninstall after that succeeds" installer --yes --uninstall
    as_user rm -f "$home/.stub-fcitx5-version"
    printf '#!/bin/sh\necho "GNOME Shell 50.1"\n' >"$work/stubs/gnome-shell"

    as_user rm -f "$changes"
    echo "['ubuntu-appindicators@ubuntu.com']" | as_user tee "$enabled_extensions" >/dev/null
    : | as_user tee "$installed_extensions" >/dev/null
    check "install without the Input Method Panel succeeds" installer --yes
    check "it installs the Input Method Panel even with a tray" grep -qx kimpanel@kde.org "$installed_extensions"
    check "uninstall after that succeeds" installer --yes --uninstall
    check "uninstall removes the Input Method Panel it installed" grep -qx kimpanel@kde.org "$uninstalled_extensions"

    # An install from before the list of changes: nothing is undone, but the user is told how.
    as_user rm -f "$installed_extensions" "$uninstalled_extensions"
    check "install for the old-install case succeeds" installer --yes
    as_user rm -f "$changes"
    check "uninstall without a list succeeds" uninstall_logged
    check "it leaves forward-keys on" extension_on forward-keys@ngosen.github.io
    check "it says how to turn forward-keys off" grep -qF "gnome-extensions disable forward-keys@ngosen.github.io" "$install_log"
    stub_path=
}

uninstall_logged() { installer --yes --uninstall >"$install_log" 2>&1; }

fcitx5_mask=$home/.config/autostart/org.fcitx.Fcitx5.desktop
refuses() { ! "$@"; }
fcitx5_masked() { has_line "$fcitx5_mask" 'Hidden=true'; }
rejects_ibus_option() {
    ! installer --yes --ibus >"$install_log.ibus" 2>&1 && grep -qF 'không hiểu tuỳ chọn --ibus' "$install_log.ibus"
}

check_autostart_mask_removed() {
    use_gnome_stubs
    as_user rm -f "$changes"
    as_user mkdir -p "$(dirname "$fcitx5_mask")"
    printf '[Desktop Entry]\nType=Application\nName=Fcitx 5\nHidden=true\n' | as_user tee "$fcitx5_mask" >/dev/null
    check "install over the hidden autostart succeeds" install_logged
    check "it lets fcitx5 start at login again" refuses fcitx5_masked
    check "uninstall after that succeeds" uninstall_logged
    stub_path=
}

check_user_im_config_kept() {
    as_user rm -f "$changes"
    echo 'run_im fcitx5' | as_user tee "$home/.xinputrc" >/dev/null
    check "install over im-config set by the user succeeds" installer --yes
    check "uninstall after that succeeds" uninstall_logged
    check "im-config set by the user stays" has_line "$home/.xinputrc" 'run_im fcitx5'
    check "it says fcitx5 still starts at login" grep -qE "fcitx5 vẫn (chạy|còn trên máy)" "$install_log"
    check "it does not promise fcitx5 is gone" refuses grep -qE "GNOME dùng lại IBus|bỏ hẳn fcitx5" "$install_log"
    as_user rm -f "$home/.xinputrc"
}

apt_args=/srv/apt-get-args

# sudo keeps its own PATH, which on Kali leaves out /usr/local, so the wrapper takes apt-get's place.
log_apt_get() {
    dpkg-divert --quiet --rename --add /usr/bin/apt-get
    printf '#!/bin/sh\necho "$*" >>%s\nexec /usr/bin/apt-get.distrib "$@"\n' "$apt_args" >/usr/bin/apt-get
    chmod +x /usr/bin/apt-get
}

restore_apt_get() {
    rm -f /usr/bin/apt-get
    dpkg-divert --quiet --rename --remove /usr/bin/apt-get
}

names_frontend() { grep -q '^install .*fcitx5-frontend-' "$apt_args"; }

check_installed_frontends_left_alone() {
    log_apt_get
    : >"$apt_args"
    check "precondition: the GTK 3 module is installed" is_installed fcitx5-frontend-gtk3
    check "install with the modules in place succeeds" installer --yes
    check "precondition: apt-get install was logged" grep -q '^install ' "$apt_args"
    check "it does not name the installed modules" refuses names_frontend
    restore_apt_get
}

parent_name() { ps -o comm= -p "$(ps -o ppid= -p "$1" | tr -d ' ')"; }

# Runs fcitx5 as its child like KWin. KWin restarts it only when the setting's value changes; this
# one restarts it on every change signal, so the installer's wait for KWin is not tested here.
use_fake_kwin() {
    local dir=$work/kwin
    mkdir -p "$dir"
    cat >"$dir/kwin_wayland" <<'STUB'
#!/bin/bash
child=
start() { fcitx5 >/dev/null 2>&1 & child=$!; }
stop() { [ -z "$child" ] || { kill "$child" 2>/dev/null; wait "$child" 2>/dev/null; }; child=; }
start
while read -r line; do
    case "$line" in *member=ConfigChanged*) ;; *) continue ;; esac
    stop
    [ -z "$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod)" ] || start
done < <(dbus-monitor --session "type='signal',interface='org.kde.kconfig.notify',member='ConfigChanged'")
STUB
    chmod a+rx "$dir/kwin_wayland"
    fake_kwin=$dir/kwin_wayland
}

# The session bus and the config a logged-in compositor session already has.
prepare_compositor_session() {
    bus=$(sudo -u "$user" dbus-daemon --session --fork --print-address=1 | head -n 1)
    as_user mkdir -p "$(dirname "$desktop_config")"
    printf '# seeded by install-smoke\n' | as_user tee "$desktop_config" >/dev/null
    desktop_config_hash=$(sha256sum "$desktop_config")
}

# Arguments go to env for the installer. A clean slate each time, so nothing leaks between cases.
install_fresh() {
    stop_fcitx5
    as_user rm -f "$profile"
    as_user env "$@" bash "$work/install.sh" --yes >"$install_log" 2>&1
}

install_no_setup() { installer --yes --no-setup >"$install_log" 2>&1; }

# Other Wayland desktops and installs that opt out must not start anything.
expect_no_setup() {
    check "$1: it leaves the steps to the user" leaves_steps_to_the_user
    check "$1: it starts no fcitx5" fcitx5_stopped
    check "$1: it adds nothing to the profile" lacks_line "$profile" 'Name=ngosen'
}

check_sway_config_messages() {
    local copy
    as_user rm -f "$desktop_config"
    mkdir -p /etc/sway
    printf 'bindsym Mod4+Return exec foot\n' >/etc/sway/config
    check "install without a Sway config succeeds" install_fresh
    copy=$(sed -n 's/^  \(mkdir -p .*\)$/\1/p' "$install_log")
    check "it prints a command to copy the system config" test -n "$copy"
    check "that command works" as_user bash -c "$copy"
    check "it keeps the system config's key bindings" has_line "$desktop_config" 'bindsym Mod4+Return exec foot'

    as_user rm -f "$desktop_config"
    rm -f /etc/sway/config
    check "install with no Sway config at all succeeds" install_fresh
    check "it prints no copy command" refuses grep -qF 'cp -n' "$install_log"
    check "it still has the autostart line" has_autostart_line

    as_user mkdir -p "$home/.i3"
    printf '# i3\n' | as_user tee "$home/.i3/config" >/dev/null
    check "install with only an i3 config succeeds" install_fresh
    check "it points at the file Sway reads" grep -qF "$home/.i3/config" "$install_log"
    as_user rm -rf "$home/.i3"
}

check_hyprland_config_messages() {
    local conf_line='exec-once = fcitx5 -d' lua=$desktop_config conf=${desktop_config%.lua}.conf
    as_user rm -f "$lua"
    printf '# conf\n' | as_user tee "$conf" >/dev/null
    check "install with only hyprland.conf succeeds" install_fresh
    check "it prints the exec-once line" grep -qF "$conf_line" "$install_log"
    check "it does not print the Lua line" lacks_autostart_line

    as_user rm -f "$conf"
    check "install with no Hyprland config succeeds" install_fresh
    check "it prints both forms" grep -qF "$conf_line" "$install_log"
    check "and the Lua one" has_autostart_line
    check "it names no file to create" refuses grep -qF "$conf" "$install_log"
}

# Sway and Hyprland: what the final message says for each config, and when to stay out of the way.
check_sway_hyprland_setup() {
    prepare_compositor_session

    check "install with a config succeeds" install_fresh
    check "it says typing works" says_typing_works
    check "a second run succeeds" install_logged
    check "it still names the toggle key" install_names Ctrl+Shift
    check "it still has the autostart line" has_autostart_line
    check "it sets the module Qt 5 reads" has_line "$compositor_env_file" 'QT_IM_MODULE=fcitx'
    check "Qt 6 prefers fcitx5's module and falls back to Wayland" has_line "$compositor_env_file" 'QT_IM_MODULES=fcitx;wayland'
    check "it points X11 apps at fcitx5" has_line "$compositor_env_file" 'XMODIFIERS=@im=fcitx'
    check "it says those apps wait for the next login" grep -qF 'từ lần đăng nhập sau' "$install_log"
    check_compositor_env_is_kept

    local other=$home/other/${desktop_config#"$home"/.config/}
    as_user mkdir -p "$(dirname "$other")"
    as_user touch "$other"
    check "install with another XDG_CONFIG_HOME succeeds" install_fresh XDG_CONFIG_HOME="$home/other"
    check "it points at that directory" grep -qF "$other" "$install_log"
    check "fcitx5 stops after that directory" stop_fcitx5
    as_user rm -rf "$home/other"

    if [ "$session" = sway ]; then
        check_sway_config_messages
        check "install from a text console login succeeds" install_fresh XDG_SESSION_TYPE=tty XDG_CURRENT_DESKTOP=
        check "it adds ngosen from a text console login" has_line "$profile" 'Name=ngosen'
        check "it starts fcitx5 from a text console login" fcitx5_running
    else
        check_hyprland_config_messages
    fi

    check_no_compositor_setup
    check_next_to_other_input_methods
    check_fcitx5_that_dies

    as_user rm -f "$desktop_config"
    pkill -u "$user" dbus-daemon || :
    bus=
}

# A variable the user chose stays theirs, and undoing leaves a file they edited.
check_compositor_env_is_kept() {
    local own=$home/.config/environment.d/50-own.conf
    as_user mkdir -p "$(dirname "$own")"
    as_user rm -f "$compositor_env_file"
    printf 'QT_IM_MODULE=ibus\n' | as_user tee "$own" >/dev/null
    check "install with the user's own Qt module succeeds" install_logged
    check "it adds no file beside the user's variable" test ! -e "$compositor_env_file"
    printf 'QT_IM_MODULES=wayland\n' | as_user tee "$own" >/dev/null
    check "install with the user's own Qt module list succeeds" install_logged
    check "it adds no file beside the user's list" test ! -e "$compositor_env_file"
    as_user rm -f "$own"
    check "install without the user's variable succeeds" install_logged
    check "it writes the file again" test -e "$compositor_env_file"
}

check_no_compositor_setup() {
    as_user rm -f "$compositor_env_file"
    check "install without WAYLAND_DISPLAY succeeds" install_fresh -u WAYLAND_DISPLAY
    check "it writes no variables without WAYLAND_DISPLAY" test ! -e "$compositor_env_file"
    expect_no_setup "without WAYLAND_DISPLAY"
    check "install on labwc succeeds" install_fresh -u SWAYSOCK -u HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP=labwc:wlroots
    expect_no_setup "on labwc"
    check "fcitx5 stops after labwc" stop_fcitx5
    as_user rm -f "$profile"
    check "install with --no-setup succeeds" install_no_setup
    expect_no_setup "with --no-setup"
}

check_next_to_other_input_methods() {
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=unikey\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    check "install next to another input method succeeds" install_logged
    # Read before fcitx5 exits: without the unikey addon here it drops that entry as it saves.
    check "it keeps the keyboard" has_line "$profile" 'Name=keyboard-us'
    check "it keeps the other input method" has_line "$profile" 'Name=unikey'
    check "it adds ngosen" has_line "$profile" 'Name=ngosen'
    check "a keyboard default becomes ngosen" has_line "$profile" 'DefaultIM=ngosen'
    check "it names the key that turns input on" grep -qF 'bấm Ctrl+Space để bật Ngó Sen' "$install_log"
    check "it does not promise typing next to another input method" refuses says_typing_works
    check "fcitx5 stops next to another input method" stop_fcitx5
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=unikey\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=unikey\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    check "install with another default input method succeeds" install_logged
    check "it keeps that default" has_line "$profile" 'DefaultIM=unikey'
    check "it names that default" grep -qF 'Bộ gõ mặc định vẫn là unikey' "$install_log"
    check "it says how to reach Ngó Sen" grep -qF 'fcitx5-remote -s ngosen' "$install_log"
    check "it does not promise typing with another default" refuses says_typing_works
    check "fcitx5 stops with another default" stop_fcitx5
}

# A daemon that dies a moment after fcitx5 -d has returned, so only waiting catches it.
check_fcitx5_that_dies() {
    local dead=$work/dead-fcitx5
    mkdir -p "$dead"
    cat >"$dead/fcitx5" <<'STUB'
#!/bin/bash
[ "${1:-}" = -d ] || exec /usr/bin/fcitx5 "$@"
# The second command keeps bash from replacing this process, which carries the name fcitx5, with sleep.
( sleep 0.2; : ) >/dev/null 2>&1 &
STUB
    chmod -R a+rX "$dead"
    chmod a+x "$dead/fcitx5"
    stub_path=$dead:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
    check "install with an fcitx5 that dies succeeds" install_fresh
    check "it does not say typing works" refuses says_typing_works
    check "it names no toggle key that cannot work" refuses install_names Ctrl+Shift
    stub_path=
    check "fcitx5 stops after the stub" stop_fcitx5
}

# Prints the pid of the fcitx5 the fake KWin started, once it holds its bus name.
start_fake_kwin() {
    local pid tries=40
    as_user "$fake_kwin" >/dev/null 2>&1 &
    until pid=$(fcitx5_pid || true) && [ -n "$pid" ] && as_user dbus-send --session --print-reply \
        --dest=org.freedesktop.DBus / org.freedesktop.DBus.NameHasOwner string:org.fcitx.Fcitx5 \
        2>/dev/null | grep -q 'boolean true'; do
        tries=$((tries - 1))
        [ "$tries" -gt 0 ] || break
        sleep 0.25
    done
    echo "$pid"
}

stop_fake_kwin() {
    pkill -u "$user" -x kwin_wayland || :
    stop_fcitx5
}

# KWin's fcitx5 is the only one that gets the app name from Wayland apps; one the installer starts
# itself goes without it until the next login.
check_restart_through_kwin() {
    local pid new
    use_fake_kwin
    bus=$(sudo -u "$user" dbus-daemon --session --fork --print-address=1 | head -n 1)

    pid=$(start_fake_kwin)
    check "precondition: KWin started fcitx5" test "$(parent_name "$pid")" = kwin_wayland
    # An addon written after fcitx5 started has not been loaded yet.
    sleep 1
    touch "$(find /usr/lib /usr/lib64 -maxdepth 3 -name libngosen.so | head -n 1)"
    check "a run with fcitx5 under KWin succeeds" installer --yes
    new=$(fcitx5_pid || true)
    check "it restarts fcitx5" test -n "$new" -a "$new" != "$pid"
    check "KWin starts the new fcitx5" test "$(parent_name "$new")" = kwin_wayland
    check "KWin still starts fcitx5 as its input method" has_line "$kwinrc" "$kwin_im"
    stop_fake_kwin

    # Without ngosen in the profile the installer stops fcitx5 to add it.
    printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[GroupOrder]\n0=Default\n' |
        as_user tee "$profile" >/dev/null
    start_fake_kwin >/dev/null
    check "a run without ngosen in the profile succeeds" installer --yes
    new=$(fcitx5_pid || true)
    check "KWin starts fcitx5 after ngosen is added" test -n "$new" -a "$(parent_name "$new")" = kwin_wayland
    check "the profile lists ngosen" has_line "$profile" 'Name=ngosen'
    stop_fake_kwin

    pkill -u "$user" dbus-daemon || :
    bus=
}

main() {
    prepare_system
    [ -z "$desktop_config" ] || prepare_compositor_session

    echo "== install"
    # A failed install leaves the session files half done, so only the packages say anything.
    check "install succeeds" install_shown
    check_packages
    [ "$failures" -eq 0 ] || finish
    check "it refuses the removed --ibus option" rejects_ibus_option
    check "the installer no longer names the IBus package" refuses grep -qE -- 'ibus-ngosen|--ibus' "$work/install.sh"
    check "the package leaves the next steps to install.sh" refuses grep -qF -- "--- Cấu hình Ngó Sen ---" "$install_log"
    check_session_files
    if [ "$session" = gnome ]; then
        check "it explains the GNOME setup first" explains_gnome_setup
        check "a second install succeeds" install_logged
        check "an account already on fcitx5 gets no explanation" lacks_gnome_explanation
    fi
    # The later parts expect to start fcitx5 themselves.
    [ -z "$desktop_config" ] || check "fcitx5 stops before the uninstall" stop_fcitx5

    echo "== uninstall"
    check "uninstall succeeds" uninstall_logged
    if [ "$session" = gnome ]; then
        check "what it undid is listed under a heading" lists_undone_under_a_heading
    fi
    check_uninstalled_state

    echo "== reinstall over a running fcitx5"
    check_reinstall_over_running_fcitx5

    echo "== second run of the same version"
    check_second_run

    echo "== profile from before the toggle key"
    check_old_profile_cleanup

    if [ "$session" = x11 ]; then
        echo "== toggle key named before the addon runs"
        check_toggle_key_message
        echo "== im-config the user set"
        check_user_im_config_kept
        echo "== GTK and Qt modules already installed"
        check_installed_frontends_left_alone
    fi

    if [ "$session" != kde ]; then
        echo "== fcitx5 autostart hidden"
        check_autostart_mask_removed
    fi

    if [ -n "$desktop_config" ]; then
        echo "== Sway and Hyprland setup"
        check_sway_hyprland_setup
    fi

    if [ "$session" = kde ]; then
        echo "== fcitx5 that KWin started"
        check_restart_through_kwin
    fi

    if [ "$session" = gnome ] && command -v im-config >/dev/null 2>&1; then
        echo "== im-config the user set"
        check_user_im_config_kept
    fi

    if [ "$session" = gnome ]; then
        echo "== uninstall after the user edited environment.d"
        check_edited_environment_kept
        echo "== extensions on GNOME 50.1"
        check_extensions_turned_back
    fi

    finish
}

main
