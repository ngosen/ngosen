#!/bin/bash
# Installs the Ngó Sen package built for this distribution from the latest GitHub release.
#
#   curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
#
# Everything lives in functions and main runs on the last line, so a download that is cut short
# cannot run half a script.
set -euo pipefail

RELEASE_URL=${NGOSEN_RELEASE_URL:-https://github.com/ngosen/ngosen/releases/latest/download}
IBUS_RELEASE_URL=${NGOSEN_IBUS_RELEASE_URL:-https://github.com/ngosen/ibus-ngosen/releases/latest/download}
SOURCE_GUIDE="https://github.com/ngosen/ngosen/blob/main/TU-DUNG.md"
OSTREE_ISSUE="https://github.com/ngosen/ngosen/issues/492"

# fcitx5 or ibus; the IBus edition is for GNOME and has its own releases.
edition=fcitx5
package_name=fcitx5-ngosen
switch_fcitx5=0
switch_ibus=0
switched_from_fcitx5=0
assume_yes=0
setup_session=1
session_changes=()
keyboard_dropped=0
fcitx5_reloaded=0
package_changed=1
fcitx5_current=0
manager=
pattern=
# Packages apt upgrades along with the install on a distribution that needs it.
apt_upgrades=()
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
    say "Cách dùng: install.sh [--yes] [--ibus] [--no-setup | --uninstall]"
    say "  --yes        không hỏi lại trước khi cài hay gỡ"
    say "  --ibus       Ngó Sen cho GNOME: gõ qua IBus có sẵn của GNOME, không cần fcitx5"
    say "  --no-setup   chỉ cài gói, không tự đặt bộ gõ cho tài khoản này"
    say "  --uninstall  gỡ Ngó Sen và trả lại những gì lệnh cài đã đổi"
}

ibus_unsupported() {
    [ "$edition" = ibus ] || return 0
    die "chưa có Ngó Sen cho GNOME (bản IBus) trên $1. Bỏ --ibus để cài bản fcitx5."
}

# Image-based Fedora (Silverblue, Kinoite, Bazzite) layers packages with rpm-ostree; dnf cannot install.
is_ostree() { [ -e /run/ostree-booted ]; }

ostree_unsupported() {
    say "Máy này là Fedora dạng ảnh hệ thống (Silverblue, Kinoite, Bazzite...), không cài bằng dnf được."
    say "install.sh chưa hỗ trợ loại máy này. Chạy tay các lệnh sau:"
    say "  $1"
    say "  systemctl reboot"
    say "$2"
    say "Muốn install.sh hỗ trợ chính thức? Bấm 👍 hoặc kể máy bạn đang dùng ở: $OSTREE_ISSUE"
    exit 1
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

    # Fedora remixes (Ultramarine, Nobara) have an ID of their own but keep Fedora's release number.
    if [ "$id" = fedora ] || [[ " $like " == *" fedora "* ]]; then
        case "$version" in
            43 | 44) ;;
            *) unsupported "${id} ${version:-không rõ phiên bản}" ;;
        esac
        manager=dnf
        is_ostree && manager=rpm-ostree
        pattern="${package_name}-[0-9][^ /]*\\.fc${version}\\.${machine}\\.rpm"
        return
    fi

    case "$id" in
        opensuse-tumbleweed)
            x86_64_only "openSUSE" "$machine"
            manager=zypper
            pattern="${package_name}-[0-9][^ /]*\\.opensuse-tumbleweed\\.x86_64\\.rpm"
            return
            ;;
        arch | cachyos | endeavouros)
            x86_64_only "$id" "$machine"
            manager=pacman
            pattern="${package_name}-[0-9][^ /]*-x86_64\\.pkg\\.tar\\.zst"
            return
            ;;
    esac

    if command -v apt-get >/dev/null 2>&1; then
        # Kali Rolling is built from Debian testing.
        [ "$id" = kali ] && codename=forky
        # deepin 23 (beige) and 25 (crimson) ship glibc 2.38 and fcitx5 5.1, above what the bookworm build needs.
        case "$id:$codename" in
            deepin:beige | deepin:crimson)
                codename=bookworm
                # deepin's fcitx5 GTK modules declare GLib 2.76 but need the GLib in its repository;
                # on an image that has not been updated GTK apps then get no input method.
                apt_upgrades=(libglib2.0-0)
                ;;
            *:bookworm | *:trixie | *:forky | *:jammy | *:noble | *:resolute | *:stonking) ;;
            *) codename=$(debian_base "$codename") ;;
        esac
        case "$codename" in
            # The IBus edition needs libadwaita 1.5, which this lacks.
            bookworm) ibus_unsupported "$id $codename" ;;
        esac
        case "$codename" in
            bookworm | trixie | forky | noble | resolute | stonking)
                manager=apt
                pattern="${package_name}_[0-9][^ /]*_${codename}_${deb_arch}\\.deb"
                return
                ;;
        esac
    fi

    # Some Arch derivatives (Artix, Garuda) leave ID_LIKE out; pacman is what the package needs.
    if [[ " $like " == *" arch "* ]] || { command -v pacman >/dev/null 2>&1 && ! command -v apt-get >/dev/null 2>&1; }; then
        x86_64_only "$id" "$machine"
        manager=pacman
        pattern="${package_name}-[0-9][^ /]*-x86_64\\.pkg\\.tar\\.zst"
        return
    fi

    # Ubuntu 22.04 (and Linux Mint 21) had packages up to 1.6.4.
    [ "$codename" = jammy ] && die "Ngó Sen từ 1.7.0 cần Ubuntu 24.04 trở lên (Linux Mint 22 trở lên). Bản cuối cho Ubuntu 22.04 là 1.6.4: https://github.com/ngosen/ngosen/releases/tag/ngosen-1.6.4-1"
    unsupported "${id:-bản phân phối này} ${codename:-$version}"
}

# Debian derivatives with a codename of their own (LMDE, Devuan, Parrot) still name the Debian
# release they are built on, in DEBIAN_CODENAME or /etc/debian_version.
debian_base() {
    local base
    # shellcheck disable=SC1091
    base=$(. /etc/os-release && printf '%s' "${DEBIAN_CODENAME:-}")
    if [ -n "$base" ]; then
        printf '%s' "$base"
        return
    fi
    case "$(cat /etc/debian_version 2>/dev/null)" in
        12.*) printf bookworm ;;
        13.*) printf trixie ;;
        *) printf '%s' "$1" ;;
    esac
}

# The IBus edition has its own releases, which cover fewer distributions.
has_ibus_package() {
    local sums ibus_pattern="ibus-ngosen${pattern#"$package_name"}"
    sums=$(fetch -sS "$IBUS_RELEASE_URL/SHA256SUMS" 2>/dev/null) || return 1
    grep -qE "^[0-9a-f]{64}  ${ibus_pattern}\$" <<<"$sums"
}

explain_gnome_setup() {
    say ""
    say "Lệnh sẽ đặt fcitx5 thay bộ gõ IBus của GNOME cho tài khoản này: fcitx5 tự chạy khi"
    say "đăng nhập, app gõ qua fcitx5, thêm biểu tượng và menu bộ gõ lên thanh trên cùng (tải"
    say "extension Input Method Panel từ extensions.gnome.org). Đăng xuất một lần là dùng được."
    say "Gỡ bằng --uninstall thì trả lại hết."
    if ibus_edition_in_use || ! has_ibus_package; then
        return 0
    fi
    say ""
    say "Thích đơn giản, dùng đúng đồ có sẵn của GNOME? Có bản Ngó Sen cho GNOME chạy trên IBus:"
    say "không cần fcitx5 hay extension, cài xong gõ được ngay, đổi Anh/Việt bằng Super+Space như"
    say "các nguồn nhập khác. Tính năng và trải nghiệm gõ không bằng bản fcitx5: chữ đang gõ có"
    say "gạch chân, không có Cứu chữ, Cứu dấu. Muốn bản này thì trả lời N rồi chạy:"
    say "  curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --ibus"
}

confirm() {
    [ "$assume_yes" -eq 1 ] && return 0
    # The script itself arrives on stdin when piped from curl, so the answer is read from the terminal.
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        die "không có cửa sổ dòng lệnh để hỏi lại. Chạy lại với --yes nếu đồng ý cài."
    fi
    local answer
    printf '%s [y/N] ' "$1"
    read -r answer <&3 || answer=
    exec 3<&-
    case "$answer" in
        [yY] | [yY][eE][sS] | [cC]) return 0 ;;
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
    if [ "$edition" = fcitx5 ] && [ -e "$addon" ] && ! pacman -Qo "$addon" >/dev/null 2>&1; then
        say "Máy có bản cài từ mã; gói sẽ ghi đè các tệp của bản đó."
        mapfile -t path_list < <(bsdtar -tf "$package" | grep -v -E '^\.|/$')
        local path
        for path in "${path_list[@]}"; do
            overwrite+=(--overwrite "$path")
        done
    fi

    as_root env NGOSEN_INSTALLER=1 pacman -U --needed --noconfirm "${overwrite[@]}" "$package"
}

# The settings window opens in GTK on these desktops when it can (settings-gui/main.py), so its Qt
# libraries are left out there.
wants_gtk_settings() {
    [ "$edition" = fcitx5 ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:GNOME:* | *:X-Cinnamon:*) return 0 ;;
    esac
    return 1
}

# Debian 12 has libadwaita 1.2, older than the GTK window needs.
apt_has_libadwaita() {
    local version
    version=$(LC_ALL=C apt-cache policy gir1.2-adw-1 2>/dev/null | sed -n 's/^ *Candidate: //p')
    [ -n "$version" ] && [ "$version" != '(none)' ] && dpkg --compare-versions "$version" ge 1.5
}

# Packages named with the package itself, so the package manager settles its choice of Qt or GTK.
# The Arch package keeps Qt, having no alternative dependencies.
settings_window_packages() {
    settings_packages=()
    if wants_gtk_settings; then
        case "$manager" in
            dnf) settings_packages=(python3-gobject gobject-introspection libadwaita) ;;
            zypper) settings_packages=(python3-gobject typelib-1_0-Adw-1) ;;
            apt) ! apt_has_libadwaita || settings_packages=(gir1.2-adw-1 python3-gi) ;;
        esac
    elif [ "$edition" = fcitx5 ]; then
        case "$manager" in
            dnf) settings_packages=(python3-QtPy python3-pyqt6 qt6-qtsvg) ;;
            zypper) settings_packages=(python3-QtPy python3-PyQt6 libQt6Svg6) ;;
        esac
    fi
}

# The package scripts see NGOSEN_INSTALLER and leave the next steps to this script.
install_package() {
    local package=$1
    local -a settings_packages=()
    case "$manager" in
        dnf | zypper) settings_window_packages ;;
    esac
    case "$manager" in
        dnf)
            as_root env NGOSEN_INSTALLER=1 dnf install -y "$package" "${settings_packages[@]}"
            ;;
        zypper)
            as_root env NGOSEN_INSTALLER=1 zypper --non-interactive install --allow-unsigned-rpm "$package" "${settings_packages[@]}"
            ;;
        apt)
            # fcitx5 only recommends its GTK and Qt modules and im-config, and MX turns recommends
            # off. Naming them also covers a reinstall, which apt skips along with its recommends.
            # im-config 1.1 (Kali, deepin) fails without a dialog program but only recommends whiptail.
            # Only missing ones are named: naming an installed module upgrades it, which on a rolling
            # release pulls in part of a toolkit upgrade.
            local extra candidate
            local -a extras=() candidates=()
            if [ "$edition" = fcitx5 ]; then
                for candidate in im-config whiptail fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 fcitx5-frontend-qt5 fcitx5-frontend-qt6; do
                    is_package_installed "$candidate" || candidates+=("$candidate")
                done
            fi
            candidates+=("${apt_upgrades[@]}")
            # Kali's images ship without package lists, and old lists name versions the mirror dropped.
            # A broken third-party source fails the update while the distribution's lists still refresh;
            # the install below stops on its own if a package is really missing.
            if ! as_root apt-get update; then
                say "Một nguồn apt trên máy đang lỗi (xem dòng E: ở trên); vẫn thử cài tiếp."
            fi
            for candidate in "${candidates[@]}"; do
                # apt translates the label, "Ứng cử:" on a Vietnamese desktop.
                extra=$(LC_ALL=C apt-cache policy "$candidate" 2>/dev/null | grep -m1 'Candidate:' || true)
                case "$extra" in
                    *'(none)'* | '') ;;
                    *) extras+=("$candidate") ;;
                esac
            done
            # Modules new to the machine reach apps only after fcitx5 restarts.
            [ "${#extras[@]}" -eq 0 ] || package_changed=1
            settings_window_packages
            as_root env NGOSEN_INSTALLER=1 apt-get install -y "$package" "${extras[@]}" "${settings_packages[@]}"
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
    [ "$edition" = fcitx5 ] && [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
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
    [ "$edition" = fcitx5 ] && [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    [ "${XDG_SESSION_TYPE:-}" = wayland ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:KDE:*) ;;
        *) return 1 ;;
    esac
    command -v kwriteconfig6 >/dev/null 2>&1 && command -v kreadconfig6 >/dev/null 2>&1
}

# Outside GNOME nothing starts IBus by default; im-config does it on Debian and Ubuntu. Elsewhere
# the user sets it up, as the steps differ per distribution.
wants_ibus_x11_setup() {
    [ "$edition" = ibus ] && [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    [ "${XDG_SESSION_TYPE:-}" = x11 ] || return 1
    case ":${XDG_CURRENT_DESKTOP:-}:" in
        *:GNOME:* | *:KDE:*) return 1 ;;
    esac
    command -v im-config >/dev/null 2>&1
}

config_home() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}"
}

fcitx5_autostart_mask() {
    printf '[Desktop Entry]\nType=Application\nName=Fcitx 5\nHidden=true\n'
}

# Removes the mask the IBus edition's install wrote, if nobody changed it since.
unmask_fcitx5_autostart() {
    local mask edition=ibus
    mask="$(config_home)/autostart/org.fcitx.Fcitx5.desktop"
    [ -e "$mask" ] && [ "$(cat "$mask")" = "$(fcitx5_autostart_mask)" ] || return 0
    rm -f "$mask"
    forget_change "hidden fcitx5 autostart"
}

enable_fcitx5_autostart() {
    local dir
    dir="$(config_home)/autostart"
    unmask_fcitx5_autostart
    [ -e /etc/xdg/autostart/org.fcitx.Fcitx5.desktop ] && return 0
    [ -e "$dir/org.fcitx.Fcitx5.desktop" ] && return 0
    mkdir -p "$dir"
    cp /usr/share/applications/org.fcitx.Fcitx5.desktop "$dir/"
    session_changes+=("fcitx5 tự chạy khi đăng nhập")
}

# Lists what the install turned on outside its own files, so the uninstall turns off only that.
changes_file() {
    local dir="${XDG_STATE_HOME:-$HOME/.local/state}/ngosen"
    if [ "$edition" = ibus ]; then
        printf '%s' "$dir/ibus-install-changes"
    else
        printf '%s' "$dir/install-changes"
    fi
}

note_change() {
    local file
    file=$(changes_file)
    grep -qsxF -- "$1" "$file" && return 0
    if ! { mkdir -p "$(dirname "$file")" && printf '%s\n' "$1" >>"$file"; } 2>/dev/null; then
        say "Không ghi được $file; lệnh gỡ sẽ không trả lại: $1"
    fi
}

made_change() {
    grep -qsxF -- "$1" "$(changes_file)"
}

forget_change() {
    local file rest
    file=$(changes_file)
    made_change "$1" || return 0
    rest=$(grep -vxF -- "$1" "$file" || true)
    if [ -n "$rest" ]; then
        printf '%s\n' "$rest" >"$file"
    else
        rm -f "$file"
    fi
}

xinputrc_runs_fcitx5() {
    grep -qs '^run_im fcitx5$' "$HOME/.xinputrc"
}

xinputrc_runs_ibus() {
    grep -qs '^run_im ibus$' "$HOME/.xinputrc"
}

# im-config 1.1 replaced -n with -w and -m with -r.
im_config_set() {
    im-config -n "$1" >/dev/null 2>&1 || im-config -w "$1" >/dev/null 2>&1
}

# im-config 1.1 drops the choice with "auto"; given "REMOVE" it writes "run_im REMOVE".
im_config_is_1_1() {
    case "$(im-config -h 2>&1)" in
        *"-w <name>"*) return 0 ;;
        *) return 1 ;;
    esac
}

im_config_unset() {
    if im_config_is_1_1; then
        im-config -w auto >/dev/null 2>&1
    else
        im-config -n REMOVE >/dev/null 2>&1
    fi
}

im_config_unset_command() {
    if im_config_is_1_1; then
        echo "im-config -w auto"
    else
        echo "im-config -n REMOVE"
    fi
}

# Prints the input method im-config starts when the user has picked none.
im_config_auto() {
    im-config -m 2>/dev/null | sed -n 3p | grep . ||
        im-config -r 2>/dev/null | sed -n 's/^IM framework (auto): //p'
}

im_environment_entry() {
    printf 'GTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\nSDL_IM_MODULE=fcitx\n'
}

# The systemd user manager reads environment.d only when it starts, and it can outlive a logout;
# without a reload the next login keeps the variables from before the change.
reload_user_environment() {
    command -v systemctl >/dev/null 2>&1 || return 0
    systemctl --user daemon-reload >/dev/null 2>&1 || :
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
    reload_user_environment
    # Debian's im-config otherwise picks IBus for X11 sessions and overrides the variables above.
    if command -v im-config >/dev/null 2>&1 && ! xinputrc_runs_fcitx5 && im_config_set fcitx5; then
        note_change im-config
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
    reload_user_environment
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

wait_for_new_fcitx5() {
    local old=$1 new tries=10
    while [ "$tries" -gt 0 ]; do
        sleep 0.5
        new=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1 || true)
        [ -n "$new" ] && [ "$new" != "$old" ] && return 0
        tries=$((tries - 1))
    done
    return 1
}

started_by_kwin() {
    [ "$(ps -o comm= -p "$(ps -o ppid= -p "$1" 2>/dev/null | tr -d ' ')" 2>/dev/null)" = kwin_wayland ]
}

kwin_input_method_available() {
    dbus-send --session --print-reply --dest=org.kde.KWin /VirtualKeyboard \
        org.freedesktop.DBus.Properties.Get string:org.kde.kwin.VirtualKeyboard string:available \
        2>/dev/null | grep -q 'boolean true'
}

# Only an fcitx5 that KWin starts gets the Wayland input-method protocol, and KWin starts it again
# only when the setting changes. KWin reads the setting when the change signal arrives, so it stays
# cleared until KWin has let go of the old one.
restart_fcitx5_in_kwin() {
    local launcher old tries=20 failed=0
    launcher=$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod 2>/dev/null) || return 1
    [ -n "$launcher" ] || return 1
    old=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1 || true)
    kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "" || return 1
    # Without the setting KDE would start no input method at the next login either.
    trap 'kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "$launcher"; exit 130' INT TERM HUP
    while [ "$tries" -gt 0 ] && kwin_input_method_available; do
        sleep 0.1
        tries=$((tries - 1))
    done
    kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "$launcher" ||
        kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "$launcher" || failed=1
    trap - INT TERM HUP
    [ "$failed" -eq 0 ] || return 1
    wait_for_new_fcitx5 "$old"
}

# The running fcitx5 starts a new copy of itself, which keeps the session's environment and stays
# in the session; started from here it would get the terminal's. KWin's fcitx5 declines, so KWin
# starts that one again.
restart_fcitx5() {
    local old
    old=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1) || return 1
    if started_by_kwin "$old"; then
        restart_fcitx5_in_kwin
        return
    fi
    dbus-send --session --print-reply --dest=org.fcitx.Fcitx5 /controller \
        org.fcitx.Fcitx.Controller1.Restart >/dev/null 2>&1 || return 1
    wait_for_new_fcitx5 "$old"
}

installed_package_version() {
    case "$manager" in
        apt) dpkg-query -W -f='${Version}' "$package_name" ;;
        pacman) pacman -Q "$package_name" | cut -d' ' -f2 ;;
        *) rpm -q --qf '%{EPOCHNUM}:%{VERSION}-%{RELEASE}' "$package_name" ;;
    esac 2>/dev/null
}

package_file_version() {
    case "$manager" in
        apt) dpkg-deb -f "$1" Version ;;
        pacman) pacman -Qp "$1" | cut -d' ' -f2 ;;
        *) rpm -qp --qf '%{EPOCHNUM}:%{VERSION}-%{RELEASE}' "$1" ;;
    esac 2>/dev/null
}

# True when the running fcitx5 started after the installed addon was written, so it runs that one.
fcitx5_runs_installed_addon() {
    local pid addon elapsed
    pid=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1) || return 1
    addon=$(find /usr/lib /usr/lib64 -maxdepth 3 -name libngosen.so 2>/dev/null | head -n 1)
    [ -n "$addon" ] || return 1
    elapsed=$(ps -o etimes= -p "$pid") || return 1
    [ "$(($(date +%s) - elapsed))" -gt "$(stat -c %Z "$addon")" ]
}

# An fcitx5 that ran without the addon drops ngosen from the profile as it exits, hence the second
# check.
reload_fcitx5() {
    local had_keyboard=0 tries=20
    if [ "$package_changed" -eq 0 ] && has_ngosen_in_profile && fcitx5_runs_installed_addon; then
        fcitx5_current=1
        return 0
    fi
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
    local was_running=0 in_kwin=0 pid
    reload_fcitx5 && return 0
    pid=$(pgrep -u "$(id -u)" -x fcitx5 | head -n 1 || true)
    [ -n "$pid" ] && started_by_kwin "$pid" && in_kwin=1
    # Stopped before the check too: one running without the addon has already dropped ngosen.
    stop_fcitx5 && was_running=1
    add_ngosen_to_profile
    [ "$was_running" -eq 1 ] || return 0
    [ "$in_kwin" -eq 1 ] && restart_fcitx5_in_kwin && return 0
    # A late fcitx5 from KWin would quit if this one already held the bus name.
    pgrep -u "$(id -u)" -x fcitx5 >/dev/null || fcitx5 -d >/dev/null 2>&1 || :
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
        0)
            note_change "extension $uuid"
            session_changes+=("bật extension sửa lỗi gõ của GNOME $version")
            ;;
        2) say "Chưa bật được extension sửa lỗi gõ; sau khi đăng nhập lại chạy: gnome-extensions enable $uuid" ;;
    esac
}

# Only the Input Method Panel draws fcitx5's popups inside GNOME Shell, such as the mode menu in the
# overview search, and stock GNOME has no tray for the menu either. It is published only on
# extensions.gnome.org.
install_kimpanel() {
    local uuid=kimpanel@kde.org major info url rc=0
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
        note_change "installed $uuid"
    fi
    enable_shell_extension "$uuid" || rc=$?
    if [ "$rc" -eq 0 ]; then
        note_change "extension $uuid"
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
    if ! xinputrc_runs_fcitx5; then
        im_config_set fcitx5 || die "im-config không đặt được fcitx5"
        note_change im-config
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
    if [ "${#session_changes[@]}" -eq 0 ] && [ "$fcitx5_reloaded" -eq 0 ] && [ "$fcitx5_current" -eq 0 ]; then
        say "Xong. Đăng xuất rồi đăng nhập lại để dùng bản mới."
        return
    fi
    report_session_changes
}

report_session_changes() {
    local change
    if [ "${#session_changes[@]}" -eq 0 ]; then
        report_dropped_keyboard
        if [ "$fcitx5_current" -eq 1 ]; then
            say "Xong. Ngó Sen đã là bản này và đã đặt xong, không có gì thay đổi."
        elif [ "$fcitx5_reloaded" -eq 1 ]; then
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

# Turns im-config back to the system default if the install picked fcitx5 there.
undo_im_config() {
    if made_change im-config && xinputrc_runs_fcitx5 && command -v im-config >/dev/null 2>&1; then
        im_config_unset || :
    fi
}

# Installs from before the list of changes are left alone, as nothing tells whether the install or
# the user picked fcitx5.
report_unknown_changes() {
    local uuid line
    local -a left=()
    [ -e "$(changes_file)" ] && return 0
    if command -v gnome-extensions >/dev/null 2>&1; then
        for uuid in forward-keys@ngosen.github.io kimpanel@kde.org; do
            if gnome-extensions list --enabled 2>/dev/null | grep -qx "$uuid"; then
                left+=("extension $uuid vẫn bật (có thể do bản cài cũ bật); muốn tắt thì chạy: gnome-extensions disable $uuid")
            fi
        done
    fi
    [ "${#left[@]}" -eq 0 ] || say "Chưa trả lại được:"
    for line in "${left[@]}"; do
        say "  - $line"
    done
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
        reload_user_environment
        undone+=("app dùng lại IBus")
    elif [ -e "$im_env" ]; then
        say "Giữ nguyên $im_env vì đã được sửa sau khi cài."
    fi
    undo_im_config
    autostart="$config/autostart/org.fcitx.Fcitx5.desktop"
    # Not cmp: diffutils is missing on minimal systems.
    if [ -e "$autostart" ] &&
        [ "$(cat "$autostart")" = "$(cat /usr/share/applications/org.fcitx.Fcitx5.desktop 2>/dev/null)" ]; then
        rm -f "$autostart"
        undone+=("fcitx5 thôi tự chạy khi đăng nhập")
    fi
    for id in "${FCITX5_LAUNCHERS[@]}"; do
        if [ -e "$data/$id" ] && [ "$(cat "$data/$id")" = "$(hidden_launcher_entry "$id")" ]; then
            rm -f "$data/$id"
        fi
    done
    if made_change "extension forward-keys@ngosen.github.io"; then
        disable_shell_extension forward-keys@ngosen.github.io || :
    fi
    if made_change "extension kimpanel@kde.org"; then
        disable_shell_extension kimpanel@kde.org || :
    fi
    if made_change "installed kimpanel@kde.org"; then
        gnome-extensions uninstall kimpanel@kde.org >/dev/null 2>&1 || :
        undone+=("gỡ extension Input Method Panel")
    fi
    [ "${#undone[@]}" -eq 0 ] || say "Đã trả lại những gì bản fcitx5 đặt:"
    for id in "${undone[@]}"; do
        say "  - $id"
    done
}

# Any of these keeps apps on fcitx5, where the IBus edition never sees a key.
fcitx5_edition_in_use() {
    [ -e "$(config_home)/environment.d/90-ngosen.conf" ] || xinputrc_runs_fcitx5 ||
        pgrep -u "$(id -u)" -x fcitx5 >/dev/null
}

# Switching takes the other edition away from this account, so --yes does not answer it.
confirm_switch() {
    local yes=$assume_yes rc=0
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        die "tài khoản này đang dùng $1; chạy lệnh cài trong cửa sổ dòng lệnh để trả lời có chuyển hay không."
    fi
    exec 3<&-
    assume_yes=0
    confirm "$2" || rc=1
    assume_yes=$yes
    return "$rc"
}

# The package's autostart entry stays, so a user entry with Hidden=true keeps fcitx5 from starting.
hide_fcitx5_autostart() {
    local file
    file="$(config_home)/autostart/org.fcitx.Fcitx5.desktop"
    [ -e /etc/xdg/autostart/org.fcitx.Fcitx5.desktop ] || return 0
    if [ -e "$file" ]; then
        [ "$(cat "$file")" = "$(fcitx5_autostart_mask)" ] ||
            say "  - giữ nguyên $file vì đã được sửa; fcitx5 có thể vẫn tự chạy khi đăng nhập"
        return 0
    fi
    mkdir -p "$(dirname "$file")"
    fcitx5_autostart_mask >"$file"
    note_change "hidden fcitx5 autostart"
    session_changes+=("fcitx5 thôi tự chạy khi đăng nhập")
}

# The local edition makes changes_file name the fcitx5 edition's list.
undo_fcitx5_setup() {
    local edition=fcitx5
    undo_gnome_session
    rm -f "$(changes_file)"
}

switch_from_fcitx5() {
    undo_fcitx5_setup
    say ""
    hide_fcitx5_autostart
    stop_fcitx5 || :
    switched_from_fcitx5=1
}

# Ubuntu 24.04 still sets the fcitx5 variables from ~/.xinputrc in a GNOME session.
use_ibus_im_config() {
    xinputrc_runs_fcitx5 && command -v im-config >/dev/null 2>&1 || return 0
    im_config_set ibus || return 0
    note_change im-config
    session_changes+=("im-config chọn IBus thay fcitx5")
}

ibus_source_list() {
    gsettings get org.gnome.desktop.input-sources sources 2>/dev/null
}

ibus_engine_list() {
    gsettings get org.freedesktop.ibus.general preload-engines 2>/dev/null
}

add_ibus_source() {
    local sources layout
    if ! sources=$(ibus_source_list); then
        say "Chưa thêm được Ngó Sen; thêm tay ở Cài đặt › Bàn phím › Nguồn nhập."
        return 0
    fi
    case "$sources" in
        *"('ibus', 'ngosen')"*) return 0 ;;
        "@a(ss) []" | "[]")
            # An empty list means GNOME types on the system layout, which has to stay first.
            layout=$(localectl status 2>/dev/null | sed -n 's/^ *X11 Layout: *//p' | cut -d, -f1)
            sources="[('xkb', '${layout:-us}'), ('ibus', 'ngosen')]"
            ;;
        *) sources="${sources%]}, ('ibus', 'ngosen')]" ;;
    esac
    gsettings set org.gnome.desktop.input-sources sources "$sources"
    note_change "input source ibus ngosen"
    session_changes+=("thêm Ngó Sen vào nguồn nhập của GNOME")
}

# Outside GNOME ibus-daemon loads the engines in this list when it starts.
add_ibus_engine() {
    local engines
    if ! engines=$(ibus_engine_list); then
        say "Chưa thêm được Ngó Sen; thêm tay ở IBus Preferences (ibus-setup)."
        return 0
    fi
    case "$engines" in
        *"'ngosen'"*) return 0 ;;
        # Ngó Sen has no English mode of its own here, so a plain keyboard goes first.
        "@as []" | "[]") engines="['xkb:us::eng', 'ngosen']" ;;
        *) engines="${engines%]}, 'ngosen']" ;;
    esac
    gsettings set org.freedesktop.ibus.general preload-engines "$engines"
    note_change "ibus engine ngosen"
    session_changes+=("thêm Ngó Sen vào danh sách bộ gõ của IBus")
}

# Cinnamon keeps Super for its menu, so IBus's default Super+Space never reaches IBus.
add_ibus_trigger() {
    local triggers
    triggers=$(gsettings get org.freedesktop.ibus.general.hotkey triggers 2>/dev/null) || return 0
    case "$triggers" in
        *"'<Control>space'"*) return 0 ;;
        "@as []" | "[]") triggers="['<Control>space']" ;;
        *) triggers="${triggers%]}, '<Control>space']" ;;
    esac
    gsettings set org.freedesktop.ibus.general.hotkey triggers "$triggers"
    note_change "ibus trigger ctrl+space"
    session_changes+=("thêm phím chuyển Ctrl+Space cho IBus")
}

# GNOME logs in on the first of the recently used sources. Someone switching from the fcitx5
# edition was typing Vietnamese, so they start on Ngó Sen rather than on the keyboard.
make_ngosen_current() {
    local recent
    recent=$(gsettings get org.gnome.desktop.input-sources mru-sources 2>/dev/null) || return 0
    recent=$(printf '%s' "$recent" | sed "s/^@a(ss) //; s/('ibus', 'ngosen'),\? *//; s/, *]$/]/")
    case "$recent" in
        "[]") recent="[('ibus', 'ngosen')]" ;;
        *) recent="[('ibus', 'ngosen'), ${recent#[}" ;;
    esac
    gsettings set org.gnome.desktop.input-sources mru-sources "$recent"
}

# A running ibus-daemon finds a newly installed engine only when it starts again.
restart_ibus() {
    pgrep -u "$(id -u)" -x ibus-daemon >/dev/null || return 1
    ibus restart >/dev/null 2>&1
}

setup_ibus_session() {
    local restarted=0 change
    say ""
    [ "$switch_fcitx5" -eq 1 ] && switch_from_fcitx5
    use_ibus_im_config
    add_ibus_source
    [ "$switched_from_fcitx5" -eq 1 ] && make_ngosen_current
    restart_ibus && restarted=1
    if [ "${#session_changes[@]}" -gt 0 ]; then
        say "Đã đặt Ngó Sen cho GNOME:"
        for change in "${session_changes[@]}"; do
            say "  - $change"
        done
        say ""
    fi
    if [ "$switched_from_fcitx5" -eq 1 ] || [ "$restarted" -eq 0 ]; then
        say "Xong. Đăng xuất rồi đăng nhập lại là gõ được tiếng Việt."
    else
        say "Xong. Gõ được tiếng Việt ngay."
    fi
    say "Bấm Super+Space để chuyển qua lại Ngó Sen và bàn phím thường."
    say "App Electron đang mở (VS Code, Discord, Zalo…): đóng rồi mở lại để gõ được tiếng Việt."
    say "Tài khoản khác trên máy này: đăng nhập vào tài khoản đó rồi chạy lại lệnh cài."
}

setup_ibus_x11_session() {
    local change
    say ""
    [ "$switch_fcitx5" -eq 1 ] && switch_from_fcitx5
    if ! xinputrc_runs_ibus; then
        # im-config leaves alone a ~/.xinputrc it did not write, yet the -w fallback still succeeds.
        im_config_set ibus || :
        if xinputrc_runs_ibus; then
            note_change im-config
            session_changes+=("app dùng IBus, IBus tự chạy khi đăng nhập")
        else
            say "Chưa đặt được IBus vì $HOME/.xinputrc do bạn tự viết; sửa dòng run_im trong đó thành: run_im ibus"
        fi
    fi
    add_ibus_engine
    add_ibus_trigger
    restart_ibus || :
    if [ "${#session_changes[@]}" -gt 0 ]; then
        say "Đã đặt IBus và Ngó Sen làm bộ gõ:"
        for change in "${session_changes[@]}"; do
            say "  - $change"
        done
        say ""
    fi
    say "Xong. Đăng xuất rồi đăng nhập lại là gõ được tiếng Việt."
    say "Bấm Ctrl+Space để chuyển qua lại Ngó Sen và bàn phím thường."
    say "Tài khoản khác trên máy này: đăng nhập vào tài khoản đó rồi chạy lại lệnh cài."
}

# Undoes setup_ibus_session. fcitx5 stays masked when only the IBus edition was installed, since
# at login it would take the keys from IBus without anything set up to use it.
undo_ibus_session() {
    local sources updated
    if made_change "input source ibus ngosen" && sources=$(ibus_source_list); then
        updated=$(printf '%s' "$sources" | sed "s/, ('ibus', 'ngosen')//; s/('ibus', 'ngosen'), //; s/('ibus', 'ngosen')//")
        [ "$updated" = "[]" ] && updated="@a(ss) []"
        gsettings set org.gnome.desktop.input-sources sources "$updated"
        say "  - bỏ Ngó Sen khỏi nguồn nhập"
    fi
    if made_change "ibus engine ngosen" && sources=$(ibus_engine_list); then
        updated=$(printf '%s' "$sources" | sed "s/, 'ngosen'//; s/'ngosen', //; s/'ngosen'//")
        [ "$updated" = "[]" ] && updated="@as []"
        gsettings set org.freedesktop.ibus.general preload-engines "$updated"
        say "  - bỏ Ngó Sen khỏi danh sách bộ gõ của IBus"
    fi
    if made_change "ibus trigger ctrl+space" &&
        sources=$(gsettings get org.freedesktop.ibus.general.hotkey triggers 2>/dev/null); then
        updated=$(printf '%s' "$sources" | sed "s/, '<Control>space'//; s/'<Control>space', //; s/'<Control>space'//")
        [ "$updated" = "[]" ] && updated="@as []"
        gsettings set org.freedesktop.ibus.general.hotkey triggers "$updated"
    fi
    if made_change im-config && xinputrc_runs_ibus && command -v im-config >/dev/null 2>&1; then
        im_config_unset || :
    fi
    if is_package_installed fcitx5-ngosen; then
        unmask_fcitx5_autostart
    fi
}

is_package_installed() {
    if command -v dpkg-query >/dev/null 2>&1; then
        [ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null)" = "install ok installed" ]
    elif command -v pacman >/dev/null 2>&1; then
        pacman -Q "$1" >/dev/null 2>&1
    else
        rpm -q "$1" >/dev/null 2>&1
    fi
}

# Without --ibus the fcitx5 edition goes, unless it is not installed or the user picks the other.
pick_uninstall_edition() {
    [ "$edition" = fcitx5 ] && is_package_installed ibus-ngosen || return 0
    if ! is_package_installed fcitx5-ngosen ||
        { [ "$assume_yes" -eq 0 ] && confirm "Máy có cả Ngó Sen cho fcitx5 và cho GNOME (IBus). Gỡ bản cho GNOME?"; }; then
        edition=ibus
        package_name=ibus-ngosen
    fi
}

# Sets the caller's remove array to the command that removes package $1.
set_remove_command() {
    if is_ostree; then
        ostree_unsupported "sudo rpm-ostree uninstall $1" "Cấu hình trong ~/.config/fcitx5 được giữ."
    fi
    if command -v apt-get >/dev/null 2>&1; then
        remove=(apt-get remove -y "$1")
    elif command -v dnf >/dev/null 2>&1; then
        remove=(dnf remove -y "$1")
    elif command -v zypper >/dev/null 2>&1; then
        remove=(zypper --non-interactive remove "$1")
    elif command -v pacman >/dev/null 2>&1; then
        remove=(pacman -R --noconfirm "$1")
    else
        die "không biết trình quản lý gói của máy này"
    fi
}

# Both editions would take the same keys, so the IBus one goes when this account uses it.
ibus_edition_in_use() {
    is_package_installed ibus-ngosen || return 1
    ibus_source_list | grep -qF "('ibus', 'ngosen')" || ibus_engine_list | grep -qF "'ngosen'"
}

# The local edition makes changes_file name the IBus edition's list.
switch_from_ibus() {
    local edition=ibus
    local -a remove
    set_remove_command ibus-ngosen
    say ""
    say "Bỏ Ngó Sen bản IBus:"
    undo_ibus_session
    rm -f "$(changes_file)"
    as_root "${remove[@]}"
}

uninstall() {
    local -a remove
    pick_uninstall_edition
    set_remove_command "$package_name"
    say "Ngó Sen sẽ được gỡ bằng: ${remove[*]}"
    if wants_gnome_setup || wants_x11_setup || wants_ibus_x11_setup; then
        say "Các thay đổi lệnh cài đã làm cho tài khoản này cũng được trả lại."
    fi
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa gỡ gì."
        exit 0
    }
    if [ "$edition" = ibus ] && { wants_gnome_setup || wants_ibus_x11_setup; }; then
        undo_ibus_session
        rm -f "$(changes_file)"
    elif wants_gnome_setup; then
        undo_gnome_session
    elif wants_x11_setup; then
        undo_im_config
    fi
    if [ "$edition" = fcitx5 ] && { wants_gnome_setup || wants_x11_setup; }; then
        report_unknown_changes
        rm -f "$(changes_file)"
    fi
    as_root "${remove[@]}"
    say ""
    if [ "$edition" = ibus ]; then
        say "Xong. Đăng xuất rồi đăng nhập lại để gỡ hẳn Ngó Sen."
    elif wants_x11_setup && [ "$(im_config_auto)" = fcitx5 ]; then
        # The system default picks any installed input method, and fcitx5 itself stays installed.
        say "Xong. fcitx5 vẫn còn trên máy nên vẫn tự chạy khi đăng nhập, gõ tiếng Anh như bàn phím."
        say "Gỡ hẳn fcitx5:  sudo apt remove fcitx5"
    elif { wants_gnome_setup || wants_x11_setup; } && command -v im-config >/dev/null 2>&1 && xinputrc_runs_fcitx5; then
        say "Xong. im-config vẫn chọn fcitx5 nên fcitx5 vẫn chạy khi đăng nhập. Muốn bỏ thì chạy lệnh dưới"
        say "rồi đăng xuất và đăng nhập lại:  $(im_config_unset_command)"
    elif wants_gnome_setup; then
        say "Xong. Đăng xuất rồi đăng nhập lại để GNOME dùng lại IBus."
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
            --ibus)
                edition=ibus
                package_name=ibus-ngosen
                RELEASE_URL=$IBUS_RELEASE_URL
                ;;
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

    if [ "$manager" = rpm-ostree ]; then
        if [ "$edition" = ibus ]; then
            ostree_unsupported "sudo rpm-ostree install $RELEASE_URL/$file" "Sau khi khởi động lại, thêm Ngó Sen vào Settings → Keyboard → Input Sources."
        fi
        ostree_unsupported "sudo rpm-ostree install $RELEASE_URL/$file" "Sau khi khởi động lại, mở 'Fcitx5 Configuration' và thêm Ngó Sen."
    fi

    if [ "$edition" = ibus ]; then
        say "Ngó Sen cho GNOME sẽ được cài bằng $manager từ gói:"
        say "  $RELEASE_URL/$file"
    else
        say "Ngó Sen sẽ được cài bằng $manager từ gói:"
        say "  $RELEASE_URL/$file"
        say "Cấu hình trong ~/.config/fcitx5 được giữ."
    fi
    if wants_gnome_setup && [ "$edition" = ibus ]; then
        say "Sau đó Ngó Sen được thêm vào nguồn nhập của GNOME cho tài khoản này."
        fcitx5_edition_in_use && switch_fcitx5=1
    elif wants_ibus_x11_setup; then
        say "Sau đó IBus và Ngó Sen được đặt làm bộ gõ cho tài khoản này."
        fcitx5_edition_in_use && switch_fcitx5=1
    elif wants_gnome_setup; then
        # Only an earlier setup leaves this file; without it the account is about to get the changes.
        [ -e "$(config_home)/environment.d/90-ngosen.conf" ] || explain_gnome_setup
    elif wants_kde_setup; then
        say "Sau đó fcitx5 được đặt làm bộ gõ của KDE cho tài khoản này."
    elif wants_x11_setup; then
        say "Sau đó fcitx5 được đặt làm bộ gõ cho tài khoản này."
    fi
    if [ "$edition" = fcitx5 ] && [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] && ibus_edition_in_use; then
        say "Ngó Sen bản IBus đang dùng ở tài khoản này sẽ được gỡ."
        switch_ibus=1
    fi
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa cài gì."
        exit 0
    }
    if [ "$switch_fcitx5" -eq 1 ] &&
        ! confirm_switch fcitx5 "Tài khoản này đang gõ bằng fcitx5. Bỏ fcitx5 và chuyển sang Ngó Sen bản IBus?"; then
        say "Đã dừng, chưa cài gì. Bản fcitx5 vẫn dùng như cũ."
        exit 0
    fi
    if [ "$switch_ibus" -eq 1 ] &&
        ! confirm_switch "Ngó Sen bản IBus" "Tài khoản này đang gõ bằng Ngó Sen bản IBus. Gỡ bản IBus và chuyển sang bản fcitx5?"; then
        say "Đã dừng, chưa cài gì. Bản IBus vẫn dùng như cũ."
        exit 0
    fi

    workdir=$(mktemp -d)
    trap 'rm -rf "$workdir"' EXIT
    # apt reads the package as an unprivileged user.
    chmod 755 "$workdir"

    download_package "$hash" "$file"
    if is_package_installed "$package_name" &&
        [ "$(installed_package_version)" = "$(package_file_version "$workdir/$file")" ]; then
        package_changed=0
    fi
    install_package "$workdir/$file"
    [ "$package_changed" -eq 1 ] || say "Bản này đã được cài từ trước, không cài lại."
    if [ "$edition" = ibus ]; then
        if wants_gnome_setup; then
            setup_ibus_session
        elif wants_ibus_x11_setup; then
            setup_ibus_x11_session
        else
            say ""
            say "Xong. Bản này làm cho GNOME; trên desktop khác thì tự đặt:"
            command -v im-config >/dev/null 2>&1 &&
                say "  - chạy im-config -n ibus để app dùng IBus thay bộ gõ đang dùng"
            say "  - mở IBus Preferences (ibus-setup) và thêm Tiếng Việt (Ngó Sen) vào danh sách bộ gõ"
            say "  - đăng xuất rồi đăng nhập lại"
        fi
        return
    fi
    [ "$switch_ibus" -eq 1 ] && switch_from_ibus
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
        if [ "$fcitx5_current" -eq 1 ]; then
            say "Xong. Ngó Sen đã là bản này, không có gì thay đổi."
        else
            say "Xong. fcitx5 đã nạp bản mới, gõ được ngay."
        fi
        return
    fi
    say "Xong. Việc còn lại:"
    say "  1. Khởi động lại fcitx5:  fcitx5 -rd"
    say "     KDE Wayland: đăng xuất rồi đăng nhập lại."
    say "  2. Mở 'Fcitx5 Configuration', thêm bộ gõ Ngó Sen."
    say "  3. KDE Wayland: System Settings → Virtual Keyboard → chọn 'Fcitx 5'."
}

main "$@"
