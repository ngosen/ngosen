# Ngó Sen is a fork of fcitx5-lotus. Installed paths, the gettext domain and
# the source directory keep the upstream name so upstream patches still apply.
%global upstream_name fcitx5-lotus

Name:           fcitx5-ngosen
Version:        3.5.10
Release:        4
Summary:        Ngó Sen, a Vietnamese input method for fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/ngosen/ngosen
Source0:        %{url}/archive/v%{version}/%{upstream_name}-%{version}.tar.gz

# Both packages install the same files, so they cannot be installed together.
Conflicts:      %{upstream_name}
Obsoletes:      %{upstream_name} < %{version}-%{release}

BuildRequires:  cmake
BuildRequires:  kf6-extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  glibc-devel
BuildRequires:  fcitx5-devel
BuildRequires:  libinput-devel
BuildRequires:  systemd-devel

BuildRequires:  go
BuildRequires:  sysuser-tools
Requires(pre):  sysuser-shadow >= 3.1
BuildRequires:  rsvg-convert

%{?systemd_ordering}
Requires:       fcitx5
Requires:       python3-QtPy
Requires:       (python3-PyQt6 or python3-pyside6)
Requires:       python3-dbus-python
Requires:       acl
# Loaded with dlopen at runtime, so the automatic soname requires miss them.
Requires:       libxcb1
Requires:       libxcb-xtest0
Requires:       libxcb-xinput0
Requires(post): shadow

%description
Ngó Sen is a Vietnamese input method for fcitx5, forked from fcitx5-lotus.

%prep
%setup -q -n %{upstream_name}-%{version}
find . -type f -name '*.py' -exec sed -i '1s|^#!.*env python3|#!/usr/bin/python3|' {} +

%build
%cmake -DLOTUS_BYTECOMPILE_PYTHON:BOOL=OFF -DBUILD_TESTING:BOOL=ON
%cmake_build
cd %{_builddir}/%{upstream_name}-%{version}
%sysusers_generate_pre build/misc/user-lotus.conf lotus lotus.conf

%install
%cmake_install
%find_lang %{upstream_name}
%py3_compile %{buildroot}%{_datadir}/fcitx5-lotus

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
%{_datadir}/gnome-shell/extensions/forward-keys@ngosen.github.io/

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

%pre -f lotus.pre

%post
%service_add_post fcitx5-lotus-server@.service

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
%service_del_preun fcitx5-lotus-server@.service

%postun
%service_del_postun fcitx5-lotus-server@.service

%changelog
* Sun Oct 04 2026 Nguyen Phi <nguyenphidt@gmail.com> - 3.5.10-4
- First build published on the GitHub Releases page.

* Sun Oct 04 2026 Nguyen Phi <nguyenphidt@gmail.com> - 3.5.10-3
- Rename the package to fcitx5-ngosen; it replaces fcitx5-lotus.
- Take the service user out of group input on upgrade and apply the device ACLs to plugged-in pointer devices.
- The server ignores key counts out of range, opens pointer devices only, and caps its backspace queue.
- The server and the addon identify each other by uid; CAP_SYS_PTRACE is dropped.
- udev no longer gives group input access to /dev/uinput and input devices.

* Sat Sep 19 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com> - 3.5.10-1
- Added desktop notifications when switching typing modes via the mode menu.
- Fixed typing and key event handling for GTK4 applications on Wayland.
- Fixed focus loss issues in Chromium on X11 when using uinput modes.
- Fixed tray icon coloring to match KDE Plasma panel themes dynamically.
- Improved icon rendering by prioritizing scalable vector assets over raster images.
- Preserved per-app typing mode rules across configuration reloads and input context switches.
- Fixed input engine crashes when initialized without an external dictionary loaded.
- Fixed input lag caused by unhandled mouse/touchpad input events.

%check
%ctest
