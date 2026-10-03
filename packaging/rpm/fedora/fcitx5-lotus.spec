# Ngó Sen is a fork of fcitx5-lotus. Installed paths, the gettext domain and
# the source directory keep the upstream name so upstream patches still apply.
%global upstream_name fcitx5-lotus

Name:           fcitx5-ngosen
Version:        3.5.10
Release:        3%{?dist}
Summary:        Ngó Sen, a Vietnamese input method for fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/ngosen/ngosen
Source0:        %{url}/archive/v%{version}/%{upstream_name}-%{version}.tar.gz

# Both packages install the same files, so they cannot be installed together.
Conflicts:      %{upstream_name}
Obsoletes:      %{upstream_name} < %{version}-%{release}

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  gettext-devel
BuildRequires:  cmake(Fcitx5Core)
BuildRequires:  libinput-devel
BuildRequires:  pkgconfig(libudev)

BuildRequires:  golang
BuildRequires:  python3-devel
BuildRequires:  librsvg2-tools

%{?systemd_ordering}
Requires:       fcitx5
Requires:       python3-QtPy
Requires:       (python3-pyqt6 or python3-pyside6)
Requires:       python3-dbus
Requires:       acl
Requires(post): shadow-utils

%description
Ngó Sen is a Vietnamese input method for fcitx5, forked from fcitx5-lotus.

%prep
%setup -q -n %{upstream_name}-%{version}

%build
%cmake -DLOTUS_BYTECOMPILE_PYTHON:BOOL=OFF -DBUILD_TESTING:BOOL=ON
%cmake_build

%install
%cmake_install
%find_lang %{upstream_name}
%py_byte_compile %{__python3} %{buildroot}%{_datadir}/fcitx5-lotus

%check
%ctest

%files -f %{upstream_name}.lang
%{_datadir}/licenses/%{upstream_name}/GPL-3.0-or-later.txt
%{_datadir}/licenses/%{upstream_name}/LGPL-2.1-or-later.txt

%dir %{_datadir}/licenses/%{upstream_name}
%dir %{_modulesloaddir}
%{_bindir}/fcitx5-lotus-server
%{_bindir}/fcitx5-lotus-settings

%{_libdir}/fcitx5/liblotus.so

%{_modulesloaddir}/fcitx5-lotus.conf
%{_unitdir}/fcitx5-lotus-server@.service
%{_sysusersdir}/lotus.conf
%{_udevrulesdir}/99-lotus.rules

%{_datadir}/fcitx5/addon/lotus.conf
%{_datadir}/fcitx5/inputmethod/lotus.conf

%{_datadir}/fcitx5/lotus/
%{_datadir}/fcitx5-lotus/
%{_datadir}/applications/org.fcitx.Fcitx5.Addon.Lotus.Settings.desktop
%{_datadir}/metainfo/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml

%{_datadir}/icons/hicolor/scalable/apps/*fcitx-lotus*.svg
%{_datadir}/icons/hicolor/scalable/status/fcitx-lotus*.svg
%{_datadir}/icons/hicolor/*/status/fcitx-lotus*.png

%dir %{_datadir}/icons/breeze
%dir %{_datadir}/icons/breeze/status
%dir %{_datadir}/icons/breeze/status/22
%dir %{_datadir}/icons/breeze/status/24
%{_datadir}/icons/breeze/status/*/fcitx-lotus*.svg

%dir %{_datadir}/icons/breeze-dark
%dir %{_datadir}/icons/breeze-dark/status
%dir %{_datadir}/icons/breeze-dark/status/22
%dir %{_datadir}/icons/breeze-dark/status/24
%{_datadir}/icons/breeze-dark/status/*/fcitx-lotus*.svg

%post
%systemd_post fcitx5-lotus-server@.service

# Earlier packages put the service user in group input, which can read every keyboard.
if id -nG uinput_proxy 2>/dev/null | tr ' ' '\n' | grep -qx input; then
    gpasswd -d uinput_proxy input >/dev/null 2>&1 || :
fi
# The ACLs come from udev rules, which otherwise only reach devices plugged in later. Only the
# devices those rules act on are replayed.
udevadm control --reload-rules >/dev/null 2>&1 || :
udevadm trigger --subsystem-match=misc --sysname-match=uinput >/dev/null 2>&1 || :
udevadm trigger --subsystem-match=input --property-match=ID_INPUT_MOUSE=1 \
    --property-match=ID_INPUT_TOUCHPAD=1 --property-match=ID_INPUT_POINTINGSTICK=1 >/dev/null 2>&1 || :

if [ $1 -eq 1 ]; then
    echo "--- Cấu hình Ngó Sen ---"
    echo "Hướng dẫn sau cài đặt:"
    echo "1. Kích hoạt Server cho user của bạn:"
    echo "   sudo systemctl enable --now fcitx5-lotus-server@\$(whoami).service"
    echo ""
    echo "2. Cấu hình Fcitx5:"
    echo "   - Mở 'Fcitx5 Configuration', thêm bộ gõ Ngó Sen"
    echo ""
    echo "3. Lưu ý cho Wayland (KDE):"
    echo "   - Hãy chọn 'Fcitx 5' trong phần Virtual Keyboard của hệ thống."
    echo "------------------------------------------------"
elif [ $1 -eq 2 ]; then
    echo "--- Cấu hình Ngó Sen ---"
    echo "Hướng dẫn sau cập nhật:"
    echo "1. Khởi động lại Server cho user của bạn:"
    echo "   sudo systemctl restart fcitx5-lotus-server@\$(whoami).service"
    echo ""
    echo "2. Cấu hình Fcitx5:"
    echo "   - Mở 'Fcitx5 Configuration', nhấn restart để khởi động lại."
fi


%preun
%systemd_preun fcitx5-lotus-server@.service

%postun
%systemd_postun_with_restart fcitx5-lotus-server@.service

%changelog
* Sat Oct 03 2026 Nguyen Phi <nguyenphidt@gmail.com> - 3.5.10-3
- Take the service user out of group input on upgrade and apply the pointer ACLs to plugged-in devices.
- The server ignores key counts out of range, opens pointer devices only, and caps its backspace queue.
- The server no longer drops a client that connects as the previous one hangs up.
- The server and the addon identify each other by uid; CAP_SYS_PTRACE is dropped.
- udev no longer gives group input access to /dev/uinput and input devices.
- The About page links to this repository and no longer opens upstream's bug tracker.

* Sat Oct 03 2026 Nguyen Phi <nguyenphidt@gmail.com> - 3.5.10-2
- Rename the package to fcitx5-ngosen; it replaces fcitx5-lotus builds of this fork.

* Sat Sep 19 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com> - 3.5.10-1
- Added desktop notifications when switching typing modes via the mode menu.
- Fixed typing and key event handling for GTK4 applications on Wayland.
- Fixed focus loss issues in Chromium on X11 when using uinput modes.
- Fixed tray icon coloring to match KDE Plasma panel themes dynamically.
- Improved icon rendering by prioritizing scalable vector assets over raster images.
- Preserved per-app typing mode rules across configuration reloads and input context switches.
- Fixed input engine crashes when initialized without an external dictionary loaded.
- Fixed input lag caused by unhandled mouse/touchpad input events.
