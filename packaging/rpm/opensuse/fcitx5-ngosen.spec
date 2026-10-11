Name:           fcitx5-ngosen
# Ngó Sen numbers its own releases from 0.5.0; the epoch keeps them above the 3.5.10 builds.
Epoch:          1
Version:        1.2.2
Release:        1
Summary:        Ngó Sen, a Vietnamese input method for fcitx5
License:        GPL-3.0-or-later AND OFL-1.1
URL:            https://github.com/ngosen/ngosen
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  kf6-extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  glibc-devel
BuildRequires:  fcitx5-devel

BuildRequires:  cargo
BuildRequires:  rust >= 1.88
BuildRequires:  rsvg-convert

Requires:       fcitx5
Requires:       python3-QtPy
Requires:       (python3-PyQt6 or python3-pyside6)
Requires:       python3-dbus-python
# Loaded with dlopen at runtime, so the automatic soname requires miss them.
Requires:       libxcb1
Requires:       libxcb-xtest0
Requires:       libxcb-xinput0
Requires(posttrans): shadow

%description
Ngó Sen is a Vietnamese input method for fcitx5.

%prep
%setup -q -n %{name}-%{version}
find . -type f -name '*.py' -exec sed -i '1s|^#!.*env python3|#!/usr/bin/python3|' {} +

%build
%cmake -DNGOSEN_BYTECOMPILE_PYTHON:BOOL=OFF -DBUILD_TESTING:BOOL=ON -DNGOSEN_RUST_CORE:BOOL=ON
%cmake_build

%install
%cmake_install
%find_lang %{name}
%py3_compile %{buildroot}%{_datadir}/fcitx5-ngosen

%files -f %{name}.lang
%{_datadir}/licenses/%{name}/GPL-3.0-or-later.txt
%{_datadir}/licenses/%{name}/LGPL-2.1-or-later.txt
%{_datadir}/licenses/%{name}/OFL-1.1.txt

%dir %{_datadir}/licenses/%{name}
%{_bindir}/fcitx5-ngosen-settings

%{_libdir}/fcitx5/libngosen.so
%{_libdir}/fcitx5/libngosenmigrate.so

%{_datadir}/fcitx5/addon/ngosen.conf
%{_datadir}/fcitx5/addon/ngosenmigrate.conf
%{_datadir}/fcitx5/inputmethod/ngosen.conf

%{_datadir}/fcitx5/ngosen/

%{_datadir}/fcitx5-ngosen/
%{_datadir}/applications/io.github.ngosen.NgoSen.Settings.desktop
%{_datadir}/metainfo/io.github.ngosen.NgoSen.metainfo.xml
%{_datadir}/gnome-shell/extensions/forward-keys@ngosen.github.io/

%{_datadir}/icons/hicolor/scalable/apps/*fcitx-ngosen*.svg
%{_datadir}/icons/hicolor/scalable/status/fcitx-ngosen*.svg
%{_datadir}/icons/hicolor/*/status/fcitx-ngosen*.png

%dir %{_datadir}/icons/breeze
%dir %{_datadir}/icons/breeze/status
%dir %{_datadir}/icons/breeze/status/22
%dir %{_datadir}/icons/breeze/status/24
%{_datadir}/icons/breeze/status/*/fcitx-ngosen*.svg

%dir %{_datadir}/icons/breeze-dark
%dir %{_datadir}/icons/breeze-dark/status
%dir %{_datadir}/icons/breeze-dark/status/22
%dir %{_datadir}/icons/breeze-dark/status/24
%{_datadir}/icons/breeze-dark/status/*/fcitx-ngosen*.svg

# Earlier versions ran fcitx5-lotus-server@<user>.service as the uinput_proxy user, with udev rules
# granting it /dev/uinput and the pointer devices. This version needs none of it. posttrans runs after
# the old package is gone, so no old scriptlet can restart the server.
%posttrans
if [ -d /run/systemd/system ]; then
    for unit in $(systemctl list-units --all --plain --no-legend 'fcitx5-lotus-server@*' 2>/dev/null | awk '{print $1}'); do
        systemctl stop "$unit" >/dev/null 2>&1 || :
    done
fi
# The unit file is gone, so systemctl disable no longer finds the instances.
find /etc/systemd/system -name 'fcitx5-lotus-server@*.service' -type l -delete 2>/dev/null || :
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || :
    systemctl reset-failed 'fcitx5-lotus-server@*' >/dev/null 2>&1 || :
fi
if getent passwd uinput_proxy >/dev/null; then
    for dev in /dev/uinput /dev/input/event*; do
        [ -e "$dev" ] && setfacl -x u:uinput_proxy "$dev" >/dev/null 2>&1 || :
    done
    userdel uinput_proxy >/dev/null 2>&1 || :
fi
if getent group uinput_proxy >/dev/null; then
    groupdel uinput_proxy >/dev/null 2>&1 || :
fi
udevadm control --reload-rules >/dev/null 2>&1 || :

echo "--- Cấu hình Ngó Sen ---"
echo "Mở 'Fcitx5 Configuration' và thêm bộ gõ Ngó Sen, hoặc khởi động lại fcitx5 nếu vừa cập nhật: fcitx5 -rd"
echo "KDE Wayland: chọn 'Fcitx 5' trong System Settings → Virtual Keyboard."

%changelog
* Sun Oct 11 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.2.2-1
- Shows vi instead of a lotus next to the cursor; typing into Edge's address bar suggestion keeps tone marks.

* Sun Oct 11 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.2.1-1
- The settings window drops the Applications page; modes picked from the mode menu are still remembered per app.

* Sun Oct 11 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.2.0-1
- New settings window that follows the desktop look and saves at once; letters no longer swap when typing fast in GTK3 apps.

* Sat Oct 10 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.1.0-1
- Installs under ngosen names and carries settings over; new tray icon; a typing log for bug reports.

* Fri Oct 09 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.0.0-1
- Fixes for Calc, WPS Office and Google Sheets when clicking another cell; fcitx5 only from now on.

* Fri Oct 09 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:0.5.1-1
- Sen mode types on Sway and Hyprland; a fresh install starts in Sen mode.

* Thu Oct 08 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:0.5.0-1
- Ngó Sen's own version numbers; the typing core is now the Rust port.

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
