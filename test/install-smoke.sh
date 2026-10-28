#!/bin/bash
# Runs install.sh in a throwaway container: install, uninstall, then a reinstall over an fcitx5
# that is running without the addon. Checks packages and the user's files, not typing.
#
#   test/install-smoke.sh [--no-recommends] x11|gnome|kde <release-dir> [<ibus-release-dir>]
#
# Run as root in a fresh Debian, Ubuntu or Fedora container; <release-dir> holds the packages and
# SHA256SUMS laid out as in a release. It changes the system, so never run it on a real machine.
# With <ibus-release-dir> on GNOME it also switches to the IBus edition and back.

# check runs the helpers below by name.
# shellcheck disable=SC2329
set -euo pipefail

usage() {
    echo "usage: $0 [--no-recommends] x11|gnome|kde <release-dir> [<ibus-release-dir>]" >&2
    exit 2
}

no_recommends=0
if [ "${1:-}" = --no-recommends ]; then
    no_recommends=1
    shift
fi
[ $# -eq 2 ] || [ $# -eq 3 ] || usage
session=$1
release_dir=$(realpath "$2")
ibus_release_dir=${3:+$(realpath "$3")}
case "$session" in
    x11 | gnome | kde) ;;
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
    [ -z "$ibus_release_dir" ] || cp -r "$ibus_release_dir" "$work/ibus-release"
    cp "$(dirname "$0")/../install.sh" "$work/install.sh"
    chmod -R a+rX "$work"
}

as_user() {
    local desktop=XFCE type=x11
    case "$session" in
        gnome) desktop=GNOME type=wayland ;;
        kde) desktop=KDE type=wayland ;;
    esac
    sudo -u "$user" -H env XDG_SESSION_TYPE="$type" XDG_CURRENT_DESKTOP="$desktop" \
        NGOSEN_RELEASE_URL="file://$work/release" NGOSEN_IBUS_RELEASE_URL="file://$work/ibus-release" ${bus:+DBUS_SESSION_BUS_ADDRESS="$bus"} \
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
autostart=$home/.config/autostart/org.fcitx.Fcitx5.desktop
hidden_launcher=$home/.local/share/applications/org.fcitx.Fcitx5.desktop
kwinrc=$home/.config/kwinrc
kwin_im='InputMethod=/usr/share/applications/org.fcitx.Fcitx5.desktop'

check_packages() {
    check "fcitx5-ngosen is installed" is_installed fcitx5-ngosen
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
    elif [ "$session" = kde ]; then
        # fcitx5 may serve other input methods, so KDE keeps it.
        check "KWin still starts fcitx5" has_line "$kwinrc" "$kwin_im"
        check "XWayland apps still find fcitx5" has_line "$im_env" 'XMODIFIERS=@im=fcitx'
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
    check "reinstall over a running fcitx5 succeeds" installer --yes
    check "fcitx5 stops after the reinstall" stop_fcitx5
    check "profile lists ngosen after fcitx5 exits" has_line "$profile" 'Name=ngosen'

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

    # Without a profile fcitx5 builds one from the input methods listed for the desktop language.
    as_user rm -f "$profile" "$settings"
    # Some images (LMDE) set LC_ALL and LANGUAGE, which override LANG.
    check "fcitx5 starts on a Vietnamese desktop without a profile" start_fcitx5 -u LC_ALL -u LANGUAGE LANG=vi_VN.UTF-8
    check "fcitx5 stops on the Vietnamese desktop" stop_fcitx5
    check "a Vietnamese desktop gets ngosen in its first profile" has_line "$profile" 'Name=ngosen'

    pkill -u "$user" dbus-daemon || :
    bus=
}

install_log=$work/install.log
install_logged() { installer --yes >"$install_log" 2>&1; }
install_names() { grep -q "Bấm $1 " "$install_log"; }
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
    "get org.gnome.desktop.input-sources sources") cat "$HOME/.stub-input-sources" 2>/dev/null || echo "[('xkb', 'us')]" ;;
    "set org.gnome.desktop.input-sources sources") printf '%s\n' "$4" >"$HOME/.stub-input-sources" ;;
    *) exit 1 ;;
esac
STUB
    printf '#!/bin/sh\necho "GNOME Shell 50.1"\n' >"$dir/gnome-shell"
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
    as_user rm -f "$changes" "$enabled_extensions" "$uninstalled_extensions"
    check "install on GNOME 50.1 succeeds" installer --yes
    check "install turns forward-keys on" extension_on forward-keys@ngosen.github.io
    check "install turns the Input Method Panel on" extension_on kimpanel@kde.org
    check "uninstall on GNOME 50.1 succeeds" installer --yes --uninstall
    check "uninstall turns forward-keys off" extension_off forward-keys@ngosen.github.io
    check "uninstall turns the Input Method Panel off" extension_off kimpanel@kde.org
    check "an extension the install did not add stays installed" test ! -e "$uninstalled_extensions"

    echo "['forward-keys@ngosen.github.io', 'kimpanel@kde.org']" | as_user tee "$enabled_extensions" >/dev/null
    check "install over extensions the user turned on succeeds" installer --yes
    check "uninstall after that succeeds" installer --yes --uninstall
    check "forward-keys the user turned on stays on" extension_on forward-keys@ngosen.github.io
    check "the Input Method Panel the user turned on stays on" extension_on kimpanel@kde.org
    check "and stays installed" test ! -e "$uninstalled_extensions"

    as_user rm -f "$changes" "$enabled_extensions"
    : | as_user tee "$installed_extensions" >/dev/null
    check "install without the Input Method Panel succeeds" installer --yes
    check "it installs the Input Method Panel" grep -qx kimpanel@kde.org "$installed_extensions"
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

sources=$home/.stub-input-sources
fcitx5_mask=$home/.config/autostart/org.fcitx.Fcitx5.desktop

# Switching away from fcitx5 is asked on the terminal even with --yes; script gives it one.
ibus_install_answering() {
    printf '%s\n' "$1" | as_user script -qec "bash $work/install.sh --yes --ibus" /dev/null >"$install_log" 2>&1
}
ibus_install_without_terminal() { installer --yes --ibus </dev/null >"$install_log" 2>&1; }
refuses() { ! "$@"; }
has_ngosen_source() { grep -qF "('ibus', 'ngosen')" "$sources"; }
lacks_ngosen_source() { ! has_ngosen_source; }
fcitx5_masked() { has_line "$fcitx5_mask" 'Hidden=true'; }

check_ibus_edition() {
    use_gnome_stubs
    as_user rm -f "$changes" "$enabled_extensions" "$sources" "$im_env"
    check "fcitx5 edition install succeeds" installer --yes
    # Debian starts fcitx5 through im-config; Fedora and Arch ship this entry.
    mkdir -p /etc/xdg/autostart
    cp /usr/share/applications/org.fcitx.Fcitx5.desktop /etc/xdg/autostart/
    check "IBus install without a terminal refuses to switch" refuses ibus_install_without_terminal
    check "answering no to the switch succeeds" ibus_install_answering k
    check "and installs nothing" is_removed ibus-ngosen
    check "and keeps the fcitx5 edition set up" test -e "$im_env"
    check "answering yes to the switch succeeds" ibus_install_answering c
    check "it installs the IBus edition" is_installed ibus-ngosen
    check "it removes the fcitx5 environment" test ! -e "$im_env"
    check "it keeps fcitx5 from starting at login" fcitx5_masked
    check "it adds Ngó Sen to the input sources" has_ngosen_source
    check "fcitx5 edition install again succeeds" installer --yes
    check "it lets fcitx5 start at login again" refuses fcitx5_masked
    check "uninstalling the IBus edition succeeds" installer --yes --uninstall --ibus
    check "it removes the IBus edition" is_removed ibus-ngosen
    check "it removes Ngó Sen from the input sources" lacks_ngosen_source
    check "the fcitx5 edition stays" is_installed fcitx5-ngosen
    rm -f /etc/xdg/autostart/org.fcitx.Fcitx5.desktop
    stub_path=
}

check_user_im_config_kept() {
    as_user rm -f "$changes"
    echo 'run_im fcitx5' | as_user tee "$home/.xinputrc" >/dev/null
    check "install over im-config set by the user succeeds" installer --yes
    check "uninstall after that succeeds" installer --yes --uninstall
    check "im-config set by the user stays" has_line "$home/.xinputrc" 'run_im fcitx5'
}

apt_args=/srv/apt-get-args

# sudo keeps its own PATH, so the wrapper goes in a directory on it that comes before /usr/bin.
log_apt_get() {
    printf '#!/bin/sh\necho "$*" >>%s\nexec /usr/bin/apt-get "$@"\n' "$apt_args" >/usr/local/sbin/apt-get
    chmod +x /usr/local/sbin/apt-get
}

names_frontend() { grep -q '^install .*fcitx5-frontend-' "$apt_args"; }

check_installed_frontends_left_alone() {
    log_apt_get
    : >"$apt_args"
    check "precondition: the GTK 3 module is installed" is_installed fcitx5-frontend-gtk3
    check "install with the modules in place succeeds" installer --yes
    check "precondition: apt-get install was logged" grep -q '^install ' "$apt_args"
    check "it does not name the installed modules" refuses names_frontend
    rm -f /usr/local/sbin/apt-get
}

main() {
    prepare_system

    echo "== install"
    # A failed install leaves the session files half done, so only the packages say anything.
    check "install succeeds" installer --yes
    check_packages
    [ "$failures" -eq 0 ] || finish
    check_session_files

    echo "== uninstall"
    check "uninstall succeeds" installer --yes --uninstall
    check_uninstalled_state

    echo "== reinstall over a running fcitx5"
    check_reinstall_over_running_fcitx5

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

    if [ "$session" = gnome ]; then
        echo "== uninstall after the user edited environment.d"
        check_edited_environment_kept
        echo "== extensions on GNOME 50.1"
        check_extensions_turned_back
        if [ -n "$ibus_release_dir" ]; then
            echo "== GNOME edition (IBus)"
            check_ibus_edition
        fi
    fi

    finish
}

main
