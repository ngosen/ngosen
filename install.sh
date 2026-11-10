#!/bin/bash
# Installs the Ngó Sen package built for this distribution from the latest GitHub release.
#
#   curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
#
# Everything lives in functions and main runs on the last line, so a download that is cut short
# cannot run half a script.
set -euo pipefail

FCITX5_RELEASE_URL=${NGOSEN_RELEASE_URL:-https://github.com/ngosen/ngosen/releases/latest/download}
RELEASE_URL=$FCITX5_RELEASE_URL
IBUS_RELEASE_URL=${NGOSEN_IBUS_RELEASE_URL:-https://github.com/ngosen/ibus-ngosen/releases/latest/download}
REPO_URL=${NGOSEN_REPO_URL:-https://ngosen.github.io/ngosen}
SOURCE_GUIDE="https://github.com/ngosen/ngosen/blob/main/TU-DUNG.md"
OSTREE_ISSUE="https://github.com/ngosen/ngosen/issues/492"
UPDATE_GUIDE="https://github.com/ngosen/ngosen#cập-nhật-bằng-trình-quản-lý-gói"
# The repository key is pinned here because this script is what users already trust; the
# release workflow refuses to publish a key with another checksum.
REPO_KEY_SHA256=c21e303174f29e12540e993893f20d0b76488bbaa6a8c2f5bea6f6a64ab03fb7
APT_LIST=/etc/apt/sources.list.d/ngosen.list
APT_KEYRING=/usr/share/keyrings/ngosen.gpg
# What the install added to the system's package sources, so the uninstall removes only that.
REPO_RECORD=/var/lib/ngosen/installer-repo
APT_LIST_MARK="# Added by the Ngó Sen installer; install.sh --uninstall removes it."

# fcitx5 or ibus; the IBus edition is for GNOME and has its own releases.
edition=fcitx5
package_name=fcitx5-ngosen
edition_given=0
edition_asked=0
other_package=
other_name=
switch_fcitx5=0
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
add_repo=1
repo_added=0
# The APT suite for this machine, set by detect_distribution.
repo_suite=
distribution="máy này"
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
    say "Cách dùng: install.sh [--yes] [--fcitx5 | --ibus] [--no-repo] [--no-setup | --uninstall]"
    say "  --yes        không hỏi lại trước khi cài hay gỡ; máy chưa có bản nào thì cài bản fcitx5"
    say "  --fcitx5     cài bản fcitx5, không hỏi chọn bản"
    say "  --ibus       Ngó Sen cho GNOME: gõ qua IBus có sẵn của GNOME, không cần fcitx5"
    say "  --no-repo    không thêm repo Ngó Sen (cần có repo để nhận version mới khi cập nhật hệ thống)"
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

# Sets manager, pattern and, on Fedora, distribution; pattern is a regular expression for the package's file name.
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
        # The published packages decide which releases are supported; see find_package.
        case "$version" in
            '' | *[!0-9]*) unsupported "${id} ${version:-không rõ phiên bản}" ;;
        esac
        distribution="${id} ${version} trên máy ${machine}"
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
                repo_suite=$codename
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

# One item of the list shown before "Tiếp tục?"; more arguments continue it on new lines.
step() {
    say "  - $1"
    shift
    local line
    for line in "$@"; do
        say "    $line"
    done
}

use_ibus_edition() {
    edition=ibus
    package_name=ibus-ngosen
    RELEASE_URL=$IBUS_RELEASE_URL
}

use_fcitx5_edition() {
    edition=fcitx5
    package_name=fcitx5-ngosen
    RELEASE_URL=$FCITX5_RELEASE_URL
}

# Where install.sh can set up the IBus edition for this account.
ibus_edition_fits() {
    local edition=ibus
    wants_gnome_setup || wants_ibus_x11_setup
}

# Without --ibus or --fcitx5 an update keeps the edition the machine has, and a first install
# asks, where both editions can be set up.
pick_edition() {
    [ "$edition_given" -eq 0 ] || return 0
    if is_package_installed ibus-ngosen && ! is_package_installed fcitx5-ngosen; then
        use_ibus_edition
        return 0
    fi
    is_package_installed fcitx5-ngosen && return 0
    [ "$assume_yes" -eq 0 ] && ibus_edition_fits && has_ibus_package || return 0
    { exec 3</dev/tty; } 2>/dev/null || return 0
    say ""
    say "Ngó Sen có hai bản cho máy này. Cả hai đổi Anh/Việt bằng Ctrl+Shift."
    say ""
    say "  1) Bản fcitx5 (nên chọn)"
    say "     Đủ tính năng, gồm cả emoji, Vim mode và chế độ riêng cho từng app."
    if wants_gnome_setup; then
        say "     Lệnh đặt fcitx5 thay bộ gõ IBus và cài extension Input Method Panel để hiện"
        say "     biểu tượng và menu bộ gõ trên thanh trên cùng."
    else
        say "     Lệnh đặt fcitx5 làm bộ gõ cho tài khoản này."
    fi
    say ""
    say "  2) Bản IBus"
    say "     Dùng IBus có sẵn, chung lõi gõ nên tính năng gần như bản fcitx5."
    say "     Tính năng mới có thể đến muộn hơn bản fcitx5."
    say ""
    local answer
    printf 'Chọn 1 hay 2? [Enter = 1]: '
    read -r answer <&3 || answer=
    exec 3<&-
    case "$answer" in
        2) use_ibus_edition ;;
        '' | 1) ;;
        *) die "không hiểu lựa chọn \"$answer\"; chạy lại và gõ 1 hoặc 2" ;;
    esac
    edition_asked=1
}

# Shown on an update, which keeps the edition without asking.
mention_other_edition() {
    [ "$edition_given" -eq 0 ] && [ "$edition_asked" -eq 0 ] || return 0
    if [ "$edition" = ibus ]; then
        say "Đang dùng bản IBus. Muốn đổi sang bản fcitx5 thì chạy lệnh cài với --fcitx5."
    elif ibus_edition_fits && is_package_installed fcitx5-ngosen && has_ibus_package; then
        say "Đang dùng bản fcitx5. Muốn đổi sang bản IBus thì chạy lệnh cài với --ibus."
    fi
}

replace_other=0
other_removed=0

# The IBus edition's package conflicts with the fcitx5 one. apt and dnf swap the two in one
# transaction, so a failed install keeps the old edition; zypper and pacman are not tested for that.
# ibus-ngosen 1.1.0 does not declare the conflict, so remove_other_package catches what is left.
check_other_edition() {
    local other=ibus-ngosen name="bản IBus"
    if [ "$edition" = ibus ]; then
        other=fcitx5-ngosen
        name="bản fcitx5"
    fi
    is_package_installed "$other" || return 0
    say ""
    say "Máy đang có $name của Ngó Sen (gói $other)."
    case "$manager" in
        apt | dnf) ;;
        *)
            local -a remove
            set_remove_command "$other"
            say "Gỡ gói đó trước rồi chạy lại lệnh này:  sudo ${remove[*]}"
            exit 1
            ;;
    esac
    replace_other=1
    other_package=$other
    other_name=$name
}

remove_other_package() {
    [ "$replace_other" -eq 1 ] && is_package_installed "$other_package" || return 0
    local -a remove
    set_remove_command "$other_package"
    as_root "${remove[@]}"
    say "Đã gỡ $other_name (gói $other_package)."
    other_removed=1
}

# Everything the install will change, listed before the one question that lets it go ahead.
show_plan() {
    local name="bản fcitx5"
    [ "$edition" = fcitx5 ] || name="bản IBus"
    say ""
    say "Lệnh sẽ:"
    step "cài Ngó Sen $name bằng $manager, từ gói:" "$RELEASE_URL/$1"
    if [ "$replace_other" -eq 1 ]; then
        step "gỡ gói $other_package trong cùng lần cài, nên mọi tài khoản trên máy đều mất $other_name." \
            "Cài lỗi thì $other_name vẫn giữ nguyên."
        if [ "$edition" = ibus ]; then
            step "để bản IBus tự chép kiểu gõ, gõ tắt, bảng phím riêng và từ điển riêng của bản fcitx5" \
                "ở lần chạy đầu"
        else
            step "không chép cài đặt của bản IBus sang: muốn mang theo thì sao lưu trong cửa sổ cài đặt" \
                "của bản IBus trước, rồi khôi phục ở bản này"
            step "bỏ nguồn nhập \"Tiếng Việt (Ngó Sen)\" của bản IBus khỏi tài khoản này; tài khoản khác" \
                "tự bỏ trong Cài đặt GNOME"
        fi
    elif [ "$edition" = fcitx5 ]; then
        step "giữ cấu hình trong ~/.config/fcitx5"
    fi
    if [ "$edition" = ibus ] && { wants_gnome_setup || wants_ibus_x11_setup; } && fcitx5_edition_in_use; then
        switch_fcitx5=1
        step "bỏ fcitx5 khỏi tài khoản này: trả lại những gì bản fcitx5 đã đặt, fcitx5 thôi tự chạy" \
            "khi đăng nhập"
    fi
    show_session_plan
    if wants_repo && apt_repo_missing; then
        step "thêm repo Ngó Sen để nhận version mới khi cập nhật hệ thống (bỏ qua bằng --no-repo)"
    fi
    say "Gỡ bằng --uninstall thì trả lại những gì lệnh đã đổi."
}

show_session_plan() {
    if wants_gnome_setup && [ "$edition" = ibus ]; then
        step "thêm \"Tiếng Việt (Ngó Sen)\" vào nguồn nhập của GNOME cho tài khoản này"
    elif wants_ibus_x11_setup; then
        step "đặt IBus và Ngó Sen làm bộ gõ cho tài khoản này"
    elif wants_gnome_setup; then
        # Only an earlier setup leaves this file; without it the account is about to get the changes.
        [ -e "$(config_home)/environment.d/90-ngosen.conf" ] ||
            step "đặt fcitx5 thay bộ gõ IBus của GNOME cho tài khoản này: fcitx5 tự chạy khi đăng nhập," \
                "app gõ qua fcitx5, thêm biểu tượng và menu bộ gõ lên thanh trên cùng (tải extension" \
                "Input Method Panel từ extensions.gnome.org)"
    elif wants_kde_setup; then
        step "đặt fcitx5 làm bộ gõ của KDE cho tài khoản này"
    elif wants_x11_setup; then
        step "đặt fcitx5 làm bộ gõ cho tài khoản này"
    elif wants_sway_hyprland_setup; then
        step "thêm Ngó Sen vào danh sách bộ gõ của fcitx5 và bật fcitx5. Nếu danh sách chỉ có một bàn" \
            "phím thì Ngó Sen thay bàn phím đó. File cấu hình của desktop được giữ nguyên."
    fi
}

# GNOME keeps the IBus source in its list after the package is gone, and the source no longer types.
remove_ibus_source() {
    [ "$(id -u)" -ne 0 ] && command -v gsettings >/dev/null 2>&1 || return 0
    local sources kept
    sources=$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null) || return 0
    case "$sources" in
        *"('ibus', 'ngosen')"*) ;;
        *) return 0 ;;
    esac
    kept=$(printf '%s' "$sources" | grep -o "('[^']*', '[^']*')" | grep -vxF "('ibus', 'ngosen')" |
        awk 'NR > 1 { printf ", " } { printf "%s", $0 }' || true)
    kept=${kept:+[$kept]}
    gsettings set org.gnome.desktop.input-sources sources "${kept:-@a(ss) []}" &&
        say "Đã bỏ nguồn nhập \"Tiếng Việt (Ngó Sen)\" của bản IBus khỏi tài khoản này."
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
    [ "$count" -ne 0 ] || unsupported "$distribution"
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
            local -a erase=()
            [ "$replace_other" -eq 0 ] || erase=(--allowerasing)
            as_root env NGOSEN_INSTALLER=1 dnf install -y "${erase[@]}" "$package" "${settings_packages[@]}"
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

# Succeeds when the APT source file $1 has an enabled deb entry for the repository at $2, in the
# one-line format or in deb822 (.sources).
apt_file_lists_repo() {
    local format=list
    case "$1" in
        *.sources) format=deb822 ;;
    esac
    awk -v format="$format" -v want="${2%/}" '
        function same(uri) { sub(/\/$/, "", uri); return uri == want }
        function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
        function stanza(   uris, n, i) {
            if (tolower(trim(field["enabled"])) !~ /^(no|false|off|without|disable|0)$/ && (" " trim(field["types"]) " ") ~ /[ \t]deb[ \t]/) {
                n = split(trim(field["uris"]), uris, /[ \t]+/)
                for (i = 1; i <= n; i++)
                    if (same(uris[i])) found = 1
            }
            split("", field)
            key = ""
        }
        format == "list" {
            if ($1 != "deb")
                next
            i = 2
            if ($i ~ /^\[/) {
                while (i < NF && $i !~ /\]$/)
                    i++
                i++
            }
            if (same($i))
                found = 1
            next
        }
        /^#/ { next }
        /^[ \t]*$/ { stanza(); next }
        /^[ \t]/ { if (key != "") field[key] = field[key] " " $0; next }
        {
            key = tolower(substr($0, 1, index($0, ":") - 1))
            field[key] = substr($0, index($0, ":") + 1)
        }
        END {
            if (format == "deb822")
                stanza()
            exit !found
        }
    ' "$1"
}

# Prints the first file under the APT directory $1 that enables the repository at $2.
apt_repo_file() {
    local file
    for file in "$1/sources.list" "$1"/sources.list.d/*.list "$1"/sources.list.d/*.sources; do
        [ -r "$file" ] || continue
        # apt skips files with other characters in their names.
        case "${file##*/}" in *[!A-Za-z0-9_.-]*) continue ;; esac
        if apt_file_lists_repo "$file" "$2"; then
            printf '%s\n' "$file"
            return 0
        fi
    done
    return 1
}

# A test build would lose its next fixes to the published release, so it gets no repository.
wants_repo() {
    [ "$add_repo" -eq 1 ] && [ "$manager" = apt ] || return 1
    case "$RELEASE_URL" in
        */releases/download/next-build | */releases/download/pr-*-build) return 1 ;;
    esac
}

# Files of the user's own, even a disabled source, are never written over.
apt_repo_missing() {
    ! apt_repo_file /etc/apt "$REPO_URL/apt" >/dev/null && [ ! -e "$APT_LIST" ] && [ ! -e "$APT_KEYRING" ]
}

# A release upgrade disables third-party lists and leaves them on the old codename; the installer
# may rewrite ngosen.list only when it wrote that file itself.
own_apt_list() {
    [ -r "$REPO_RECORD" ] && [ "$(head -n 1 "$APT_LIST" 2>/dev/null)" = "$APT_LIST_MARK" ]
}

apt_list_has_suite() {
    awk -v suite="$repo_suite" '$1 == "deb" && $(NF - 1) == suite { ok = 1 } END { exit !ok }' "$APT_LIST"
}

add_apt_repo() {
    local found own=0 key="$workdir/ngosen.gpg" list="$workdir/ngosen.list" record="$workdir/installer-repo"
    own_apt_list && own=1
    if found=$(apt_repo_file /etc/apt "$REPO_URL/apt"); then
        if [ "$found" != "$APT_LIST" ] || [ "$own" -eq 0 ] || apt_list_has_suite; then
            say "Repo Ngó Sen đã có trong $found."
            return 0
        fi
    elif [ "$own" -eq 0 ]; then
        for found in "$APT_LIST" "$APT_KEYRING"; do
            if [ -e "$found" ]; then
                say "Chưa thêm repo Ngó Sen: $found đã có sẵn nhưng repo chưa bật. Xem $UPDATE_GUIDE"
                return 1
            fi
        done
    fi
    if ! fetch -sS -o "$key" "$REPO_URL/apt/ngosen.gpg"; then
        say "Không tải được key của repo Ngó Sen, chưa thêm repo."
        return 1
    fi
    if [ "$(sha256sum "$key" | cut -d' ' -f1)" != "$REPO_KEY_SHA256" ]; then
        say "Mã băm của key tải về không đúng, chưa thêm repo Ngó Sen."
        return 1
    fi
    printf '%s\ndeb [signed-by=%s] %s %s main\n' "$APT_LIST_MARK" "$APT_KEYRING" "$REPO_URL/apt" "$repo_suite" >"$list"
    [ "$own" -eq 0 ] || cp "$APT_LIST" "$workdir/ngosen.list.old"
    # Undoing removes only what this run wrote: either neither file was there, or both are ours.
    if ! as_root install -o root -g root -m 644 "$key" "$APT_KEYRING" ||
        ! as_root install -o root -g root -m 644 "$list" "$APT_LIST" ||
        ! as_root apt-get update -qq --error-on=any -o Dir::Etc::sourcelist="$APT_LIST" \
            -o Dir::Etc::sourceparts=- -o APT::Get::List-Cleanup=0; then
        if [ "$own" -eq 1 ]; then
            as_root install -o root -g root -m 644 "$workdir/ngosen.list.old" "$APT_LIST"
        else
            as_root rm -f "$APT_LIST" "$APT_KEYRING"
        fi
        say "Không thêm được repo Ngó Sen (xem lỗi ở trên); Ngó Sen vẫn đã cài."
        return 1
    fi
    repo_added=1
    say "Đã thêm repo Ngó Sen: version mới sẽ tới khi cập nhật hệ thống."
    if ! { printf 'apt\n' && sha256sum "$APT_LIST" "$APT_KEYRING"; } >"$record" ||
        ! as_root install -D -o root -g root -m 644 "$record" "$REPO_RECORD"; then
        say "Không ghi được $REPO_RECORD; lệnh gỡ sẽ để lại repo."
    fi
}

# Removes what add_apt_repo wrote, unless something still needs it or the user changed it.
remove_ngosen_repo() {
    local -a files
    [ -r "$REPO_RECORD" ] || return 0
    mapfile -t files < <(sed 1d "$REPO_RECORD" | cut -d' ' -f3-)
    if is_package_installed ibus-ngosen; then
        say "Giữ repo Ngó Sen vì ibus-ngosen còn nhận version mới từ đó."
        return 0
    fi
    # A file the user already deleted is no reason to keep the other one.
    if ! sed 1d "$REPO_RECORD" | sha256sum --quiet --ignore-missing -c - >/dev/null 2>&1 &&
        { [ -e "${files[0]}" ] || [ -e "${files[1]}" ]; }; then
        say "Giữ repo Ngó Sen vì tệp repo hoặc key đã bị sửa sau khi cài. Gỡ tay:  sudo rm ${files[*]}"
        return 0
    fi
    as_root rm -f "${files[@]}" "$REPO_RECORD"
    as_root rmdir --ignore-fail-on-non-empty "$(dirname "$REPO_RECORD")"
    say "Đã gỡ repo Ngó Sen."
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

# Each compositor sets its own variable. XDG_CURRENT_DESKTOP comes from the display manager, so it
# is missing when Sway starts from a text console.
wants_sway_hyprland_setup() {
    [ "$edition" = fcitx5 ] && [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ] || return 1
    [ -n "${WAYLAND_DISPLAY:-}" ] || return 1
    [ -n "${SWAYSOCK:-}" ] || [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]
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
    unblock_fcitx5_activation
    mask="$(config_home)/autostart/org.fcitx.Fcitx5.desktop"
    [ -e "$mask" ] && [ "$(cat "$mask")" = "$(fcitx5_autostart_mask)" ] || return 0
    rm -f "$mask"
    forget_change "hidden fcitx5 autostart"
    session_changes+=("fcitx5 tự chạy khi đăng nhập")
}

fcitx5_activation_block() {
    printf '[D-BUS Service]\nName=org.fcitx.Fcitx5\nExec=/bin/false\n'
}

fcitx5_activation_file() {
    printf '%s' "${XDG_DATA_HOME:-$HOME/.local/share}/dbus-1/services/org.fcitx.Fcitx5.service"
}

# The bus reads a services directory it did not see at login only when told to.
reload_session_bus() {
    command -v gdbus >/dev/null 2>&1 || return 0
    gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.ReloadConfig >/dev/null 2>&1 || :
}

# A message to fcitx5's bus name starts it, from GNOME's extensions among others; it then takes
# IBus's place and, as it builds a profile without Ngó Sen, writes GNOME's input sources over.
block_fcitx5_activation() {
    local file
    file=$(fcitx5_activation_file)
    [ -e /usr/share/dbus-1/services/org.fcitx.Fcitx5.service ] && [ ! -e "$file" ] || return 0
    mkdir -p "$(dirname "$file")"
    fcitx5_activation_block >"$file"
    note_change "blocked fcitx5 activation"
    reload_session_bus
}

unblock_fcitx5_activation() {
    local file edition=ibus
    file=$(fcitx5_activation_file)
    [ -e "$file" ] && [ "$(cat "$file")" = "$(fcitx5_activation_block)" ] || return 0
    rm -f "$file"
    forget_change "blocked fcitx5 activation"
    reload_session_bus
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

compositor_environment_entry() {
    printf 'QT_IM_MODULE=fcitx\nQT_IM_MODULES=fcitx;wayland\nXMODIFIERS=@im=fcitx\n'
}

# Qt 5 has no text-input protocol for these compositors and X11 apps need XIM. Qt 6 prefers fcitx5's
# module too: when a Qt 6 app on the Wayland protocol closes, Hyprland keeps sending keys through the
# input method, and the next app without that protocol loses letters. The list falls back to Wayland
# for a Qt 6 app that has no such module.
set_compositor_environment() {
    local dir file
    dir="$(config_home)/environment.d"
    file="$dir/90-ngosen-sway-hyprland.conf"
    [ -e "$file" ] && return 0
    grep -qs -E '^(QT_IM_MODULES?|XMODIFIERS)=' "$dir"/*.conf && return 0
    mkdir -p "$dir"
    compositor_environment_entry >"$file"
    reload_user_environment
}

undo_compositor_environment() {
    local file
    file="$(config_home)/environment.d/90-ngosen-sway-hyprland.conf"
    [ -e "$file" ] && [ "$(cat "$file")" = "$(compositor_environment_entry)" ] || return 0
    rm -f "$file"
    reload_user_environment
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
fcitx5_trigger_key() {
    local key
    key=$(awk '/^\[/ { section = $0 } section == "[Hotkey/TriggerKeys]" && /^0=/ { sub(/^0=/, ""); print; exit }' "$(config_home)/fcitx5/config" 2>/dev/null)
    printf '%s' "${key:-Control+space}"
}

key_display_name() {
    case "$1" in
        "" | Control+Shift_L | Control+Shift_R | Shift+Control_L | Shift+Control_R) printf 'Ctrl+Shift' ;;
        Control+space) printf 'Ctrl+Space' ;;
        *) printf '%s' "$1" ;;
    esac
}

toggle_key_name() {
    local dir key
    dir="$(config_home)/fcitx5/conf"
    key=$(awk '/^\[/ { section = $0 } section == "[ToggleKey]" && /^0=/ { sub(/^0=/, ""); print; exit }' "$dir/ngosen.conf" 2>/dev/null)
    if [ -z "$key" ] && { [ -f "$dir/lotus.conf" ] || [ -f "$dir/ngosen.conf" ] || has_lone_keyboard_in_profile; } &&
        ! grep -qs '^SettingsVersion=' "$dir/ngosen.conf"; then
        # Not upgraded yet: Ngó Sen takes over fcitx5's switch key, Ctrl+Space by default.
        key=$(fcitx5_trigger_key)
    fi
    key_display_name "${key:-}"
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

# fcitx5 before 5.1.22 names every app on GNOME after the shell; Ngó Sen then learns the app from GNOME's
# portal, or from the packaged extension where the portal is missing. GNOME 50.0 to 50.3 also drop keys an
# input method forwards, which the extension delivers.
fcitx5_names_apps_on_gnome() {
    local version
    version=$(fcitx5 --version 2>/dev/null | head -n 1) || return 1
    [ -n "$version" ] || return 1
    [ "$(printf '%s\n' "$version" 5.1.22 | sort -V | head -n 1)" = 5.1.22 ]
}

has_gnome_portal() {
    local dir IFS=:
    for dir in ${XDG_DATA_DIRS:-/usr/local/share} /usr/share; do
        [ -e "$dir/dbus-1/services/org.freedesktop.impl.portal.desktop.gnome.service" ] && return 0
    done
    return 1
}

enable_forward_keys_extension() {
    local uuid=forward-keys@ngosen.github.io version rc=0
    [ -d "/usr/share/gnome-shell/extensions/$uuid" ] || return 0
    version=$(shell_version)
    case "$version" in
        50.0 | 50.1 | 50.2 | 50.3) ;;
        4[6-9].* | 50.*) { fcitx5_names_apps_on_gnome || has_gnome_portal; } && return 0 ;;
        *) return 0 ;;
    esac
    enable_shell_extension "$uuid" || rc=$?
    case "$rc" in
        0)
            note_change "extension $uuid"
            session_changes+=("bật extension sửa lỗi gõ trên GNOME $version")
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

fcitx5_is_running() {
    pgrep -u "$(id -u)" -x fcitx5 >/dev/null
}

setup_sway_hyprland_session() {
    say ""
    configure_fcitx5
    set_compositor_environment
    fcitx5_is_running || fcitx5 -d >/dev/null 2>&1 || :
    # fcitx5 -d returns before the daemon has had a chance to fail.
    sleep 1
    report_sway_hyprland_session
}

# fcitx5 starts on the first entry of its list, a keyboard, so the switch key of an input method
# does nothing until fcitx5's own key turns input on.
has_other_input_methods() {
    [ "$(grep -c '^\[Groups/0/Items/' "$(config_home)/fcitx5/profile" 2>/dev/null)" -gt 1 ] && ! has_lone_keyboard_in_profile
}

report_other_input_methods() {
    local default trigger
    default=$(sed -n 's/^DefaultIM=//p' "$(config_home)/fcitx5/profile" | head -n 1)
    trigger=$(key_display_name "$(fcitx5_trigger_key)")
    say "Xong. fcitx5 đang chạy."
    if [ "$default" = ngosen ]; then
        say "Máy còn bộ gõ khác, nên bấm $trigger để bật Ngó Sen. Sau đó bấm $(toggle_key_name) để chuyển qua lại tiếng Việt và tiếng Anh."
    else
        say "Bộ gõ mặc định vẫn là $default, bấm $trigger để bật. Muốn chuyển sang Ngó Sen:  fcitx5-remote -s ngosen"
    fi
}

report_sway_hyprland_session() {
    if ! fcitx5_is_running; then
        say "Đã cài xong nhưng fcitx5 chưa chạy được. Thử bật trong terminal của phiên này:  fcitx5 -d"
    elif [ "$keyboard_dropped" -eq 1 ]; then
        say "Xong. fcitx5 đang chạy, gõ được ngay."
        report_dropped_keyboard
    elif has_other_input_methods; then
        report_other_input_methods
    else
        say "Xong. fcitx5 đang chạy, gõ được ngay."
        say "Bấm $(toggle_key_name) để chuyển qua lại tiếng Việt và tiếng Anh."
    fi
    if [ -e "$(config_home)/environment.d/90-ngosen-sway-hyprland.conf" ]; then
        say "App Qt 5 và app X11 qua XWayland gõ được từ lần đăng nhập sau, nếu vào desktop từ màn hình đăng nhập."
    fi
    say ""
    if [ -n "${SWAYSOCK:-}" ]; then
        report_sway_autostart
    else
        report_hyprland_autostart
    fi
}

# In the order Sway looks for its config.
report_sway_autostart() {
    local dir config source
    dir=$(config_home)
    for config in "$HOME/.sway/config" "$dir/sway/config" "$HOME/.i3/config" "$dir/i3/config"; do
        if [ -e "$config" ]; then
            say "Để fcitx5 tự chạy ở lần đăng nhập sau, nếu chưa có thì dán dòng này vào $config:"
            say "  exec fcitx5 -d"
            return
        fi
    done
    for source in /etc/sway/config /etc/i3/config; do
        if [ -e "$source" ]; then
            # A new file with only this line would hide the system config and its key bindings.
            say "Để fcitx5 tự chạy ở lần đăng nhập sau, chép cấu hình của máy trước:"
            say "  mkdir -p \"$dir/sway\" && cp -n $source \"$dir/sway/config\""
            say "rồi dán dòng này vào $dir/sway/config:"
            say "  exec fcitx5 -d"
            return
        fi
    done
    say "Để fcitx5 tự chạy ở lần đăng nhập sau, dán dòng này vào file cấu hình Sway đang dùng:"
    say "  exec fcitx5 -d"
}

# hl.exec_cmd at the top level would run while the config loads, before the Wayland socket exists,
# and again on every reload.
report_hyprland_autostart() {
    local dir lua_line='hl.on("hyprland.start", function() hl.exec_cmd("fcitx5 -d") end)' conf_line='exec-once = fcitx5 -d'
    dir="$(config_home)/hypr"
    if [ -e "$dir/hyprland.lua" ]; then
        say "Để fcitx5 tự chạy ở lần đăng nhập sau, nếu chưa có thì dán dòng này vào $dir/hyprland.lua:"
        say "  $lua_line"
    elif [ -e "$dir/hyprland.conf" ]; then
        say "Để fcitx5 tự chạy ở lần đăng nhập sau, nếu chưa có thì dán dòng này vào $dir/hyprland.conf:"
        say "  $conf_line"
    else
        # Hyprland writes its own config when it finds none, so this one lives elsewhere and a new
        # file here could hide it.
        say "Để fcitx5 tự chạy ở lần đăng nhập sau, dán vào file cấu hình Hyprland đang dùng:"
        say "  file .lua:   $lua_line"
        say "  file .conf:  $conf_line"
    fi
}

report_session_changes() {
    local change
    if [ "${#session_changes[@]}" -eq 0 ]; then
        report_dropped_keyboard
        if [ "$fcitx5_current" -eq 1 ] && { [ "$repo_added" -eq 1 ] || [ "$other_removed" -eq 1 ]; }; then
            say "Xong. Ngó Sen đã là bản này và đã đặt xong."
        elif [ "$fcitx5_current" -eq 1 ]; then
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
    block_fcitx5_activation
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

# ibus restart would pick up the new engine, but GNOME then stops sending the field type until
# the next login, so the user logs in again instead.
setup_ibus_session() {
    local change
    say ""
    [ "$switch_fcitx5" -eq 1 ] && switch_from_fcitx5
    use_ibus_im_config
    add_ibus_source
    [ "$switched_from_fcitx5" -eq 1 ] && make_ngosen_current
    if [ "${#session_changes[@]}" -gt 0 ]; then
        say "Đã đặt Ngó Sen cho GNOME:"
        for change in "${session_changes[@]}"; do
            say "  - $change"
        done
        say ""
    fi
    say "Xong. Đăng xuất rồi đăng nhập lại là gõ được tiếng Việt."
    say "Nhấp Ctrl+Shift để đổi Anh/Việt; thanh trên cùng hiện V hoặc E."
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
    if [ "${#session_changes[@]}" -gt 0 ]; then
        say "Đã đặt IBus và Ngó Sen làm bộ gõ:"
        for change in "${session_changes[@]}"; do
            say "  - $change"
        done
        say ""
    fi
    say "Xong. Đăng xuất rồi đăng nhập lại là gõ được tiếng Việt."
    say "Nhấp Ctrl+Shift để đổi Anh/Việt. Ctrl+Space chuyển giữa Ngó Sen và các bộ gõ khác của IBus."
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
    unblock_fcitx5_activation
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
    [ "$edition_given" -eq 0 ] && is_package_installed ibus-ngosen || return 0
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

# The package is gone; what this account set up for it goes too, except with --no-setup, where only
# the source that no longer types is dropped. The local edition makes changes_file name the IBus
# edition's list.
remove_ibus_setup() {
    local edition=ibus
    if [ "$setup_session" -eq 1 ]; then
        undo_ibus_session
        rm -f "$(changes_file)"
    fi
    remove_ibus_source
}

uninstall() {
    local -a remove
    pick_uninstall_edition
    set_remove_command "$package_name"
    say "Ngó Sen sẽ được gỡ bằng: ${remove[*]}"
    if wants_gnome_setup || wants_x11_setup || wants_ibus_x11_setup; then
        say "Các thay đổi lệnh cài đã làm cho tài khoản này cũng được trả lại."
    fi
    [ ! -r "$REPO_RECORD" ] || say "Repo Ngó Sen do lệnh cài thêm cũng được gỡ, trừ khi máy còn ibus-ngosen hoặc tệp đã bị sửa."
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa gỡ gì."
        exit 0
    }
    if [ "$edition" = ibus ] && { wants_gnome_setup || wants_ibus_x11_setup; }; then
        remove_ibus_setup
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
    # Only after the package is gone, so a failed removal keeps its updates.
    remove_ngosen_repo
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
    elif wants_sway_hyprland_setup; then
        undo_compositor_environment
        say "Xong. Bỏ dòng fcitx5 -d đã dán vào file cấu hình (nếu có), rồi đăng xuất và đăng nhập lại."
    else
        say "Xong. Khởi động lại fcitx5:  fcitx5 -rd"
    fi
}

main() {
    local remove_package=0
    while [ $# -gt 0 ]; do
        case "$1" in
            -y | --yes) assume_yes=1 ;;
            --no-repo) add_repo=0 ;;
            --ibus)
                use_ibus_edition
                edition_given=1
                ;;
            --fcitx5)
                use_fcitx5_edition
                edition_given=1
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
    case "$REPO_URL" in
        *[[:space:]]*) die "NGOSEN_REPO_URL không được có khoảng trắng" ;;
        https://* | file:///*) ;;
        *) die "NGOSEN_REPO_URL phải bắt đầu bằng https://" ;;
    esac

    command -v curl >/dev/null 2>&1 || die "cần có curl"
    command -v sha256sum >/dev/null 2>&1 || die "cần có sha256sum"
    if [ "$(id -u)" -ne 0 ]; then
        command -v sudo >/dev/null 2>&1 || die "cần có sudo, hoặc chạy bằng root"
    fi

    detect_distribution
    pick_edition
    # The package name is part of the file pattern, and some releases have no IBus edition.
    [ "$edition" = fcitx5 ] || detect_distribution

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

    check_other_edition
    mention_other_edition
    show_plan "$file"
    confirm "Tiếp tục?" || {
        say "Đã dừng, chưa cài gì."
        exit 0
    }

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
    remove_other_package
    [ "$package_changed" -eq 1 ] || say "Bản này đã được cài từ trước, không cài lại."
    # Before the session setup, whose branches return early; a repository that fails stays a note.
    if wants_repo; then
        add_apt_repo || :
    fi
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
    [ "$replace_other" -eq 0 ] || remove_ibus_setup
    if [ "$setup_session" -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
        unmask_fcitx5_autostart
    fi
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
    if wants_sway_hyprland_setup; then
        setup_sway_hyprland_session
        return
    fi

    say ""
    if [ "$(id -u)" -ne 0 ] && reload_fcitx5; then
        report_dropped_keyboard
        if [ "$fcitx5_current" -eq 1 ] && { [ "$repo_added" -eq 1 ] || [ "$other_removed" -eq 1 ]; }; then
            say "Xong. Ngó Sen đã là bản này."
        elif [ "$fcitx5_current" -eq 1 ]; then
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
