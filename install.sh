#!/bin/bash
# Installs the Ngó Sen package built for this distribution from the latest GitHub release.
#
#   curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
#
# Everything lives in functions and main runs on the last line, so a download that is cut short
# cannot run half a script.
set -euo pipefail

RELEASE_URL=${NGOSEN_RELEASE_URL:-https://github.com/ngosen/ngosen/releases/latest/download}
SOURCE_GUIDE="https://github.com/ngosen/ngosen/blob/main/TU-DUNG.md"

assume_yes=0
setup_session=1
session_changes=()
keyboard_dropped=0
fcitx5_reloaded=0
manager=
pattern=
workdir=

say() { printf '%s\n' "$*"; }

die() {
    printf 'Lỗi: %s\n' "$*" >&2
    exit 1
}

unsupported() {
    die "chưa có gói dựng sẵn cho $1. Cài từ mã theo hướng dẫn: $SOURCE_GUIDE"
}

as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    else
        sudo "$@"
    fi
}

# The checksum list is the only thing vouching for the package, so neither may travel over plain
# http, not even after a redirect. file:// stays usable for testing against a local directory.
fetch() {
    curl --proto '=https,file' --proto-redir '=https' --tlsv1.2 -fL "$@"
}

usage() {
    say "Cách dùng: install.sh [--yes] [--no-setup | --uninstall]"
    say "  --yes        không hỏi lại trước khi cài hay gỡ"
    say "  --no-setup   chỉ cài gói, không tự đặt fcitx5 làm bộ gõ trên GNOME"
    say "  --uninstall  gỡ Ngó Sen và trả lại những gì lệnh cài đã đổi"
}

x86_64_only() {
    [ "$2" = x86_64 ] || unsupported "$1 trên máy $2"
}

# Sets manager and pattern; pattern is a regular expression for the package's file name.
detect_distribution() {
    local machine deb_arch
    machine=$(uname -m)
    case "$machine" in
        x86_64) deb_arch=amd64 ;;
        aarch64) deb_arch=arm64 ;;
        *) unsupported "máy $machine" ;;
    esac
    [ -r /etc/os-release ] || die "không đọc được /etc/os-release"

    local id like codename version
    # shellcheck disable=SC1091
    id=$(. /etc/os-release && printf '%s' "${ID:-}")
    # shellcheck disable=SC1091
    like=$(. /etc/os-release && printf '%s' "${ID_LIKE:-}")
    # shellcheck disable=SC1091
    codename=$(. /etc/os-release && printf '%s' "${UBUNTU_CODENAME:-${VERSION_CODENAME:-}}")
    # shellcheck disable=SC1091
    version=$(. /etc/os-release && printf '%s' "${VERSION_ID:-}")

    case "$id" in
        fedora)
            case "$version" in
                43 | 44) ;;
                *) unsupported "Fedora ${version:-không rõ phiên bản}" ;;
            esac
            manager=dnf
            pattern="fcitx5-ngosen-[0-9][^ /]*\\.fc${version}\\.${machine}\\.rpm"
            return
            ;;
        opensuse-tumbleweed)
            x86_64_only "openSUSE" "$machine"
            manager=zypper
            pattern='fcitx5-ngosen-[0-9][^ /]*\.opensuse-tumbleweed\.x86_64\.rpm'
            return
            ;;
        arch | cachyos | endeavouros)
            x86_64_only "$id" "$machine"
            manager=pacman
            pattern='fcitx5-ngosen-[0-9][^ /]*-x86_64\.pkg\.tar\.zst'
            return
            ;;
    esac

    case " $like " in
        *" arch "*)
            x86_64_only "$id" "$machine"
            manager=pacman
            pattern='fcitx5-ngosen-[0-9][^ /]*-x86_64\.pkg\.tar\.zst'
            return
            ;;
    esac

    if command -v apt-get >/dev/null 2>&1; then
        # Kali Rolling is built from Debian testing.
        [ "$id" = kali ] && codename=forky
        case "$codename" in
            bookworm | trixie | forky | noble | resolute | stonking)
                manager=apt
                pattern="fcitx5-ngosen_[0-9][^ /]*_${codename}_${deb_arch}\\.deb"
                return
                ;;
        esac
    fi

    # Ubuntu 22.04 (and Linux Mint 21) had packages up to 1.6.1.
    [ "$codename" = jammy ] && die "Ngó Sen từ 1.7.0 cần Ubuntu 24.04 trở lên (Linux Mint 22 trở lên). Bản cuối cho Ubuntu 22.04 là 1.6.1: https://github.com/ngosen/ngosen/releases/tag/ngosen-1.6.1-1"
    unsupported "${id:-bản phân phối này} ${codename:-$version}"
}

confirm() {
    [ "$assume_yes" -eq 1 ] && return 0
    # The script itself arrives on stdin when piped from curl, so the answer is read from the terminal.
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        die "không có cửa sổ dòng lệnh để hỏi lại. Chạy lại với --yes nếu đồng ý cài."
    fi
    local answer
    printf '%s [c/K] ' "$1"
    read -r answer <&3 || answer=
    exec 3<&-
    case "$answer" in
        c | C | y | Y) return 0 ;;
        *) return 1 ;;
    esac
}

# Prints "<sha256> <file name>" for the one package matching this distribution.
find_package() {
    local sums line count
    sums=$(fetch -sS "$RELEASE_URL/SHA256SUMS") ||
        die "không tải được danh sách gói từ $RELEASE_URL"
    line=$(printf '%s\n' "$sums" | grep -E "^[0-9a-f]{64}  ${pattern}\$" || true)
    count=$(printf '%s\n' "$line" | grep -c . || true)
    [ "$count" -eq 1 ] || die "bản phát hành mới nhất không có đúng một gói cho máy này (tìm thấy $count)"
    printf '%s %s\n' "${line%% *}" "${line##* }"
}

download_package() {
    local hash=$1 file=$2 got
    fetch --progress-bar -o "$workdir/$file" "$RELEASE_URL/$file" ||
        die "không tải được $file"
    got=$(sha256sum "$workdir/$file" | cut -d' ' -f1)
    [ "$got" = "$hash" ] || die "tệp $file tải về không khớp mã băm trong SHA256SUMS, không cài"
}

install_with_pacman() {
    local package=$1
    local -a overwrite=() path_list=()

    # A build installed with "cmake --install" left the same files owned by no package. Only the
    # paths this package ships may be overwritten.
    local addon=/usr/lib/fcitx5/libngosen.so
    if [ -e "$addon" ] && ! pacman -Qo "$addon" >/dev/null 2>&1; then
        say "Máy có bản cài từ mã; gói sẽ ghi đè các tệp của bản đó."
        mapfile -t path_list < <(bsdtar -tf "$package" | grep -v -E '^\.|/$')
        local path
        for path in "${path_list[@]}"; do
            overwrite+=(--overwrite "$path")
        done
    fi

    as_root pacman -U --noconfirm "${overwrite[@]}" "$package"
}

install_package() {
    local package=$1
    case "$manager" in
        dnf)
            as_root dnf install -y "$package"
            ;;
        zypper)
            as_root zypper --non-interactive install --allow-unsigned-rpm "$package"
            ;;
        apt)
            # fcitx5 only recommends its GTK and Qt modules and im-config, and MX turns recommends
            # off. Naming them also covers a reinstall, which apt skips along with its recommends.
            local extra candidate
            local -a extras=()
            # Kali's images ship without package lists, and old lists name versions the mirror dropped.
            as_root apt-get update
            for candidate in im-config fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 fcitx5-frontend-qt5 fcitx5-frontend-qt6; do
                extra=$(apt-cache policy "$candidate" 2>/dev/null | grep -m1 'Candidate:' || true)
                case "$extra" in
                    *'(none)'* | '') ;;
                    *) extras+=("$candidate") ;;
                esac
            done
            as_root apt-get install -y "$package" "${extras[@]}"
            ;;
        pacman)
            install_with_pacman "$package"
            ;;
    esac
}

# The session setup below changes only the invoking user's files, so it runs without sudo and
# only on GNOME, which starts IBus by default and needs every one of these steps.
wants_gnome_setup() {
    [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:GNOME:*) return 0 ;;
    esac
    return 1
}

# Xfce, Cinnamon, MATE and the like on X11 start no input method of their own; on Debian and Ubuntu
# im-config sets the variables and starts fcitx5 at login. KDE and Wayland compositors are left out.
wants_x11_setup() {
    [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    [ "${XDG_SESSION_TYPE:-}" = x11 ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:GNOME:* | *:KDE:*) return 1 ;;
    esac
    # On apt im-config comes with the package, so it may not be there yet.
    command -v im-config >/dev/null 2>&1 || command -v apt-get >/dev/null 2>&1
}

# Plasma's autostart also starts fcitx5, but only one that KWin starts gets the Wayland input-method
# protocol, which Qt apps type through.
wants_kde_setup() {
    [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    [ "${XDG_SESSION_TYPE:-}" = wayland ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:KDE:*) ;;
        *) return 1 ;;
    esac
    command -v kwriteconfig6 >/dev/null 2>&1 && command -v kreadconfig6 >/dev/null 2>&1
}

config_home() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}"
}

enable_fcitx5_autostart() {
    local dir
    dir="$(config_home)/autostart"
    [ -e /etc/xdg/autostart/org.fcitx.Fcitx5.desktop ] && return 0
    [ -e "$dir/org.fcitx.Fcitx5.desktop" ] && return 0
    mkdir -p "$dir"
    cp /usr/share/applications/org.fcitx.Fcitx5.desktop "$dir/"
    session_changes+=("fcitx5 tự chạy khi đăng nhập")
}

# im-config 1.1 replaced -n with -w and -m with -r.
im_config_set() {
    im-config -n "$1" >/dev/null 2>&1 || im-config -w "$1" >/dev/null 2>&1
}

# Prints the input method im-config starts when the user has picked none.
im_config_auto() {
    im-config -m 2>/dev/null | sed -n 3p | grep . ||
        im-config -r 2>/dev/null | sed -n 's/^IM framework (auto): //p'
}

im_environment_entry() {
    printf 'GTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\nSDL_IM_MODULE=fcitx\n'
}

# GNOME offers fcitx5 no Wayland input-method protocol, so apps have to load fcitx5's own modules.
set_im_environment() {
    local dir file
    dir="$(config_home)/environment.d"
    file="$dir/90-ngosen.conf"
    [ -e "$file" ] && return 0
    if grep -qs '^GTK_IM_MODULE=' "$dir"/*.conf; then
        say "Giữ nguyên biến bộ gõ đã đặt trong $dir."
        return 0
    fi
    mkdir -p "$dir"
    im_environment_entry >"$file"
    # Debian's im-config otherwise picks IBus for X11 sessions and overrides the variables above.
    if command -v im-config >/dev/null 2>&1; then
        im_config_set fcitx5 || :
    fi
    session_changes+=("app dùng fcitx5 thay IBus")
}

# KWin serves Wayland apps itself; X11 apps under XWayland reach fcitx5 only through XIM.
# GTK_IM_MODULE and QT_IM_MODULE stay unset, as fcitx5 asks for on Plasma.
set_xim_environment() {
    local dir file
    dir="$(config_home)/environment.d"
    file="$dir/90-ngosen.conf"
    [ -e "$file" ] && return 0
    if grep -qs '^XMODIFIERS=' "$dir"/*.conf; then
        return 0
    fi
    mkdir -p "$dir"
    echo 'XMODIFIERS=@im=fcitx' >"$file"
    session_changes+=("app X11 chạy qua XWayland (như Minecraft bản cũ) dùng fcitx5")
}

# Prints the fcitx5 name of the first GNOME keyboard layout, e.g. "us" or "fr-azerty".
gnome_layout() {
    local sources layout
    sources=$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null || true)
    layout=$(printf '%s' "$sources" | grep -o "('xkb', '[^']*')" | head -n 1 | cut -d"'" -f4 || true)
    layout=${layout:-us}
    printf '%s' "${layout/+/-}"
}

stop_fcitx5() {
    pkill -x fcitx5 || return 1
    local tries=10
    while [ "$tries" -gt 0 ] && pgrep -x fcitx5 >/dev/null; do
        sleep 0.5
        tries=$((tries - 1))
    done
}

has_ngosen_in_profile() {
    grep -qs '^Name=ngosen$' "$(config_home)/fcitx5/profile"
}

# The running fcitx5 starts a new copy of itself, which keeps the session's environment and stays
# in the session; started from here it would get the terminal's. Fails when none runs or when
# fcitx5 declines, as it does when KWin started it.
restart_fcitx5() {
    local old new tries=10
    old=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1) || return 1
    dbus-send --session --print-reply --dest=org.fcitx.Fcitx5 /controller \
        org.fcitx.Fcitx.Controller1.Restart >/dev/null 2>&1 || return 1
    while [ "$tries" -gt 0 ]; do
        sleep 0.5
        new=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1 || true)
        [ -n "$new" ] && [ "$new" != "$old" ] && return 0
        tries=$((tries - 1))
    done
    return 1
}

# An fcitx5 that ran without the addon drops ngosen from the profile as it exits, hence the second
# check.
reload_fcitx5() {
    local had_keyboard=0 tries=20
    has_keyboard_in_profile && had_keyboard=1
    has_ngosen_in_profile && restart_fcitx5 && has_ngosen_in_profile || return 1
    fcitx5_reloaded=1
    [ "$had_keyboard" -eq 1 ] || return 0
    # The migration addon stamps the settings after saving the profile when it drops the keyboard,
    # and the new fcitx5 may first wait for the old one to exit.
    while [ "$tries" -gt 0 ] && ! grep -qs '^SettingsVersion=' "$(config_home)/fcitx5/conf/ngosen.conf"; do
        sleep 0.25
        tries=$((tries - 1))
    done
    has_keyboard_in_profile || keyboard_dropped=1
}

has_keyboard_in_profile() {
    grep -qs '^Name=keyboard-' "$(config_home)/fcitx5/profile"
}

# One group of a keyboard and ngosen on the same layout, the case the migration addon drops.
has_lone_keyboard_in_profile() {
    awk '/^\[/ { item = /\/Items\// }
        !item && /^Default Layout=/ { layout = substr($0, 16) }
        item && /^Name=/ { items++ }
        item && /^Name=keyboard-/ { keyboards++; keyboard = substr($0, 15) }
        item && $0 == "Name=ngosen" { own++ }
        END { exit !(items == 2 && keyboards == 1 && own == 1 && (layout == "" || layout == keyboard)) }' \
        "$(config_home)/fcitx5/profile" 2>/dev/null
}

# The key the engine picks: settings from before its toggle key, including lotus.conf that is
# copied over on the next start, keep Ctrl+Space; defaults are left out of the file.
toggle_key_name() {
    local dir key
    dir="$(config_home)/fcitx5/conf"
    key=$(awk '/^\[/ { section = $0 } section == "[ToggleKey]" && /^0=/ { sub(/^0=/, ""); print; exit }' "$dir/ngosen.conf" 2>/dev/null)
    if [ -z "$key" ] && { [ -f "$dir/lotus.conf" ] || [ -f "$dir/ngosen.conf" ] || has_lone_keyboard_in_profile; } &&
        ! grep -qs '^SettingsVersion=' "$dir/ngosen.conf"; then
        # Not upgraded yet: Ngó Sen takes over fcitx5's switch key, Ctrl+Space by default.
        key=$(awk '/^\[/ { section = $0 } section == "[Hotkey/TriggerKeys]" && /^0=/ { sub(/^0=/, ""); print; exit }' "$(config_home)/fcitx5/config" 2>/dev/null)
        key=${key:-Control+space}
    fi
    case "${key:-}" in
        "" | Control+Shift_L | Control+Shift_R | Shift+Control_L | Shift+Control_R) printf 'Ctrl+Shift' ;;
        Control+space) printf 'Ctrl+Space' ;;
        *) printf '%s' "$key" ;;
    esac
}

report_dropped_keyboard() {
    if [ "$keyboard_dropped" -eq 1 ]; then
        say "Ngó Sen giờ là bộ gõ duy nhất trong fcitx5. Bấm $(toggle_key_name) để chuyển tiếng Anh/Việt."
    fi
}

write_ngosen_profile() {
    local profile=$1 layout=$2
    mkdir -p "$(dirname "$profile")"
    cat >"$profile" <<EOF
[Groups/0]
Name=Default
Default Layout=$layout
DefaultIM=ngosen

[Groups/0/Items/0]
Name=ngosen
Layout=

[GroupOrder]
0=Default
EOF
}

add_ngosen_to_profile() {
    local profile layout items
    profile="$(config_home)/fcitx5/profile"
    has_ngosen_in_profile && return 0
    if [ ! -f "$profile" ]; then
        write_ngosen_profile "$profile" "$(gnome_layout)"
    else
        items=$(grep -c '^\[Groups/0/Items/' "$profile" || true)
        # fcitx5's first profile holds one keyboard; Ngó Sen switches English itself, so it takes
        # the keyboard's place and keeps its layout.
        if [ "$items" -eq 1 ] && has_keyboard_in_profile && ! grep -qs '^\[Groups/[1-9]' "$profile"; then
            # English was typed on the keyboard's own layout, whatever the group default says.
            layout=$(sed -n 's/^Name=keyboard-//p' "$profile" | head -n 1)
            write_ngosen_profile "$profile" "$layout"
        else
            printf '\n[Groups/0/Items/%s]\nName=ngosen\nLayout=\n' "$items" >>"$profile"
            sed -i 's/^DefaultIM=keyboard-.*/DefaultIM=ngosen/' "$profile"
        fi
    fi
    session_changes+=("thêm Ngó Sen vào danh sách bộ gõ")
}

# fcitx5 writes its profile back when it exits, so it is stopped before the profile is edited.
configure_fcitx5() {
    local was_running=0
    reload_fcitx5 && return 0
    # Stopped before the check too: one running without the addon has already dropped ngosen.
    stop_fcitx5 && was_running=1
    add_ngosen_to_profile
    if [ "$was_running" -eq 1 ]; then
        fcitx5 -d >/dev/null 2>&1 || :
    fi
}

# Adds an extension to GNOME's list; it starts at the next login. Returns 1 when it is already
# listed and 2 when the setting cannot be read.
enable_shell_extension() {
    local uuid=$1 enabled
    enabled=$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null) || return 2
    case "$enabled" in
        *"'$uuid'"*) return 1 ;;
        "@as []" | "[]") enabled="['$uuid']" ;;
        *) enabled="${enabled%]}, '$uuid']" ;;
    esac
    gsettings set org.gnome.shell enabled-extensions "$enabled"
}

shell_version() {
    gnome-shell --version 2>/dev/null | grep -o '[0-9][0-9]*\.[0-9][0-9]*' | head -n 1 || true
}

has_tray() {
    gnome-extensions list --enabled 2>/dev/null | grep -q appindicator
}

# GNOME 50.0 to 50.3 drop keys an input method forwards; the packaged extension delivers them.
enable_forward_keys_extension() {
    local uuid=forward-keys@ngosen.github.io version rc=0
    [ -d "/usr/share/gnome-shell/extensions/$uuid" ] || return 0
    version=$(shell_version)
    case "$version" in
        50.0 | 50.1 | 50.2 | 50.3) ;;
        *) return 0 ;;
    esac
    enable_shell_extension "$uuid" || rc=$?
    case "$rc" in
        0) session_changes+=("bật extension sửa lỗi gõ của GNOME $version") ;;
        2) say "Chưa bật được extension sửa lỗi gõ; sau khi đăng nhập lại chạy: gnome-extensions enable $uuid" ;;
    esac
}

# Stock GNOME has no tray, so fcitx5's status icon and menu need the Input Method Panel extension,
# which is published only on extensions.gnome.org.
install_kimpanel() {
    local uuid=kimpanel@kde.org major info url rc=0
    has_tray && return 0
    if ! gnome-extensions list 2>/dev/null | grep -qx "$uuid"; then
        major=$(shell_version)
        major=${major%%.*}
        [ -n "$major" ] || return 0
        info=$(fetch -sS "https://extensions.gnome.org/extension-info/?uuid=$uuid&shell_version=$major" 2>/dev/null) || info=
        url=$(printf '%s' "$info" | grep -o '"download_url": *"/[^"]*"' | cut -d'"' -f4 || true)
        if [ -z "$url" ] || ! fetch -sS -o "$workdir/kimpanel.zip" "https://extensions.gnome.org$url" 2>/dev/null ||
            ! gnome-extensions install --force "$workdir/kimpanel.zip" >/dev/null 2>&1; then
            say "Chưa cài được extension Input Method Panel; cài tay ở https://extensions.gnome.org/extension/261/"
            return 0
        fi
    fi
    enable_shell_extension "$uuid" || rc=$?
    if [ "$rc" -eq 0 ]; then
        session_changes+=("cài extension Input Method Panel: biểu tượng bộ gõ trên thanh trên cùng")
    fi
}

FCITX5_LAUNCHERS=(org.fcitx.Fcitx5.desktop org.fcitx.fcitx5-migrator.desktop fcitx5-configtool.desktop)

applications_dir() {
    printf '%s' "${XDG_DATA_HOME:-$HOME/.local/share}/applications"
}

hidden_launcher_entry() {
    printf '[Desktop Entry]\nType=Application\nName=%s\nNoDisplay=true\n' "${1%.desktop}"
}

# A user entry with the same id overrides the system one, so this hides it for this user only.
hide_launcher() {
    local id=$1 dir
    dir=$(applications_dir)
    [ -e "/usr/share/applications/$id" ] || return 1
    [ -e "$dir/$id" ] && return 1
    mkdir -p "$dir"
    hidden_launcher_entry "$id" >"$dir/$id"
}

# Autostart replaces the "Fcitx 5" launcher and nobody here migrates from fcitx4. The settings
# launcher goes only when the tray or the Input Method Panel menu opens the same settings.
hide_fcitx5_launchers() {
    local hidden=0 id
    local -a ids=("${FCITX5_LAUNCHERS[@]:0:2}")
    if has_tray || gsettings get org.gnome.shell enabled-extensions 2>/dev/null | grep -q "'kimpanel@kde.org'"; then
        ids+=(fcitx5-configtool.desktop)
    fi
    for id in "${ids[@]}"; do
        hide_launcher "$id" && hidden=$((hidden + 1))
    done
    if [ "$hidden" -gt 0 ]; then
        session_changes+=("ẩn $hidden biểu tượng fcitx5 thừa trong danh sách app")
    fi
}

setup_gnome_session() {
    say ""
    enable_fcitx5_autostart
    set_im_environment
    configure_fcitx5
    enable_forward_keys_extension
    install_kimpanel
    hide_fcitx5_launchers
    report_session_changes
}

setup_x11_session() {
    say ""
    if ! grep -qs '^run_im fcitx5$' "$HOME/.xinputrc"; then
        im_config_set fcitx5 || die "im-config không đặt được fcitx5"
        session_changes+=("app dùng fcitx5, fcitx5 tự chạy khi đăng nhập")
    fi
    configure_fcitx5
    report_session_changes
}

setup_kde_session() {
    local launcher=/usr/share/applications/org.fcitx.Fcitx5.desktop
    say ""
    if [ "$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod)" != "$launcher" ]; then
        kwriteconfig6 --file kwinrc --group Wayland --key InputMethod "$launcher"
        session_changes+=("KDE tự chạy fcitx5 làm bộ gõ (Virtual Keyboard: Fcitx 5)")
    fi
    set_xim_environment
    configure_fcitx5
    if [ "${#session_changes[@]}" -eq 0 ] && [ "$fcitx5_reloaded" -eq 0 ]; then
        say "Xong. Đăng xuất rồi đăng nhập lại để dùng bản mới."
        return
    fi
    report_session_changes
}

report_session_changes() {
    local change
    if [ "${#session_changes[@]}" -eq 0 ]; then
        report_dropped_keyboard
        if [ "$fcitx5_reloaded" -eq 1 ]; then
            say "Xong. fcitx5 đã nạp bản mới, gõ được ngay."
        else
            say "Xong. Khởi động lại fcitx5 để dùng bản mới:  fcitx5 -rd"
        fi
        return
    fi
    say "Đã đặt fcitx5 và Ngó Sen làm bộ gõ:"
    for change in "${session_changes[@]}"; do
        say "  - $change"
    done
    if [ "$keyboard_dropped" -eq 1 ]; then
        say "  - Ngó Sen là bộ gõ duy nhất trong fcitx5"
    fi
    say ""
    say "Xong. Đăng xuất rồi đăng nhập lại là gõ được tiếng Việt."
    say "Bấm $(toggle_key_name) để chuyển qua lại tiếng Việt và tiếng Anh."
    say "Tài khoản khác trên máy này: đăng nhập vào tài khoản đó rồi chạy lại lệnh cài."
}

# Removes from GNOME's list; returns 1 when it was not there.
disable_shell_extension() {
    local uuid=$1 enabled updated pattern
    enabled=$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null) || return 1
    pattern="'${uuid//./\\.}'"
    updated=$(printf '%s' "$enabled" | sed -e "s/$pattern, //; s/, $pattern//; s/$pattern//")
    [ "$updated" = "$enabled" ] && return 1
    [ "$updated" = "[]" ] && updated="@as []"
    gsettings set org.gnome.shell enabled-extensions "$updated"
}

# Undoes setup_gnome_session. Files are removed only when they still hold what the setup wrote,
# so anything the user changed stays. fcitx5's own profile and keys stay too: without autostart
# fcitx5 no longer runs, and GNOME starts IBus again at the next login.
undo_gnome_session() {
    local config data id autostart im_env
    local -a undone=()
    config=$(config_home)
    data=$(applications_dir)
    im_env="$config/environment.d/90-ngosen.conf"
    if [ -e "$im_env" ] && [ "$(cat "$im_env")" = "$(im_environment_entry)" ]; then
        rm -f "$im_env"
        undone+=("app dùng lại IBus")
    elif [ -e "$im_env" ]; then
        say "  - giữ nguyên $im_env vì đã được sửa sau khi cài"
    fi
    if grep -qs '^run_im fcitx5$' "$HOME/.xinputrc" && command -v im-config >/dev/null 2>&1; then
        im_config_set REMOVE || :
    fi
    autostart="$config/autostart/org.fcitx.Fcitx5.desktop"
    if cmp -s /usr/share/applications/org.fcitx.Fcitx5.desktop "$autostart"; then
        rm -f "$autostart"
        undone+=("fcitx5 thôi tự chạy khi đăng nhập")
    fi
    for id in "${FCITX5_LAUNCHERS[@]}"; do
        if [ -e "$data/$id" ] && [ "$(cat "$data/$id")" = "$(hidden_launcher_entry "$id")" ]; then
            rm -f "$data/$id"
        fi
    done
    disable_shell_extension forward-keys@ngosen.github.io || :
    if disable_shell_extension kimpanel@kde.org; then
        gnome-extensions uninstall kimpanel@kde.org >/dev/null 2>&1 || :
        undone+=("gỡ extension Input Method Panel")
    fi
    for id in "${undone[@]}"; do
        say "  - $id"
    done
}

uninstall() {
    local -a remove
    if command -v apt-get >/dev/null 2>&1; then
        remove=(apt-get remove -y fcitx5-ngosen)
    elif command -v dnf >/dev/null 2>&1; then
        remove=(dnf remove -y fcitx5-ngosen)
    elif command -v zypper >/dev/null 2>&1; then
        remove=(zypper --non-interactive remove fcitx5-ngosen)
    elif command -v pacman >/dev/null 2>&1; then
        remove=(pacman -R --noconfirm fcitx5-ngosen)
    else
        die "không biết trình quản lý gói của máy này"
    fi
    say "Ngó Sen sẽ được gỡ bằng: ${remove[*]}"
    if wants_gnome_setup || wants_x11_setup; then
        say "Các thay đổi lệnh cài đã làm cho tài khoản này cũng được trả lại."
    fi
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa gỡ gì."
        exit 0
    }
    if wants_gnome_setup; then
        undo_gnome_session
    elif wants_x11_setup && grep -qs '^run_im fcitx5$' "$HOME/.xinputrc"; then
        im_config_set REMOVE || :
    fi
    as_root "${remove[@]}"
    say ""
    if wants_gnome_setup; then
        say "Xong. Đăng xuất rồi đăng nhập lại để GNOME dùng lại IBus."
    elif wants_x11_setup && [ "$(im_config_auto)" = fcitx5 ]; then
        # The system default picks any installed input method, and fcitx5 itself stays installed.
        say "Xong. fcitx5 vẫn còn trên máy nên vẫn tự chạy khi đăng nhập, gõ tiếng Anh như bàn phím."
        say "Gỡ hẳn fcitx5:  sudo apt remove fcitx5"
    elif wants_x11_setup; then
        say "Xong. Đăng xuất rồi đăng nhập lại để bỏ hẳn fcitx5."
    elif wants_kde_setup; then
        # fcitx5 may serve other input methods, so KDE keeps it.
        say "Xong. Đăng xuất rồi đăng nhập lại. fcitx5 vẫn là bộ gõ của KDE, gõ tiếng Anh như bàn phím."
        say "Bỏ fcitx5: System Settings → Virtual Keyboard (Bàn phím ảo) → None."
    else
        say "Xong. Khởi động lại fcitx5:  fcitx5 -rd"
    fi
}

main() {
    local remove_package=0
    while [ $# -gt 0 ]; do
        case "$1" in
            -y | --yes) assume_yes=1 ;;
            --no-setup) setup_session=0 ;;
            --uninstall) remove_package=1 ;;
            -h | --help)
                usage
                exit 0
                ;;
            *) die "không hiểu tuỳ chọn $1" ;;
        esac
        shift
    done

    if [ "$remove_package" -eq 1 ]; then
        uninstall
        return
    fi

    case "$RELEASE_URL" in
        https://* | file:///*) ;;
        *) die "NGOSEN_RELEASE_URL phải bắt đầu bằng https://" ;;
    esac

    command -v curl >/dev/null 2>&1 || die "cần có curl"
    command -v sha256sum >/dev/null 2>&1 || die "cần có sha256sum"
    if [ "$(id -u)" -ne 0 ]; then
        command -v sudo >/dev/null 2>&1 || die "cần có sudo, hoặc chạy bằng root"
    fi

    detect_distribution

    local found hash file
    found=$(find_package)
    hash=${found%% *}
    file=${found##* }
    case "$file" in
        *[!A-Za-z0-9._+~-]* | '') die "tên tệp lạ trong SHA256SUMS: $file" ;;
    esac

    say "Ngó Sen sẽ được cài bằng $manager từ gói:"
    say "  $RELEASE_URL/$file"
    say "Cấu hình trong ~/.config/fcitx5 được giữ."
    if wants_gnome_setup; then
        say "Sau đó fcitx5 được đặt làm bộ gõ cho tài khoản này, thay IBus của GNOME."
    elif wants_kde_setup; then
        say "Sau đó fcitx5 được đặt làm bộ gõ của KDE cho tài khoản này."
    elif wants_x11_setup; then
        say "Sau đó fcitx5 được đặt làm bộ gõ cho tài khoản này."
    fi
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa cài gì."
        exit 0
    }

    workdir=$(mktemp -d)
    trap 'rm -rf "$workdir"' EXIT
    # apt reads the package as an unprivileged user.
    chmod 755 "$workdir"

    download_package "$hash" "$file"
    install_package "$workdir/$file"
    if wants_gnome_setup; then
        setup_gnome_session
        return
    fi
    if wants_kde_setup; then
        setup_kde_session
        return
    fi
    if wants_x11_setup; then
        setup_x11_session
        return
    fi

    say ""
    if [ "$(id -u)" -ne 0 ] && reload_fcitx5; then
        report_dropped_keyboard
        say "Xong. fcitx5 đã nạp bản mới, gõ được ngay."
        return
    fi
    say "Xong. Việc còn lại:"
    say "  1. Khởi động lại fcitx5:  fcitx5 -rd"
    say "     KDE Wayland: đăng xuất rồi đăng nhập lại."
    say "  2. Mở 'Fcitx5 Configuration', thêm bộ gõ Ngó Sen."
    say "  3. KDE Wayland: System Settings → Virtual Keyboard → chọn 'Fcitx 5'."
}

main "$@"
