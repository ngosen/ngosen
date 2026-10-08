# Ngó Sen is a fork of fcitx5-lotus. Installed paths, the gettext domain and
# the source directory keep the upstream name so upstream patches still apply.
%global upstream_name fcitx5-lotus

Name:           fcitx5-ngosen
Version:        3.5.10
Release:        4%{?dist}
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

BuildRequires:  cargo
BuildRequires:  rust >= 1.88
BuildRequires:  python3-devel
BuildRequires:  librsvg2-tools

Requires:       fcitx5
Requires:       python3-QtPy
Requires:       (python3-pyqt6 or python3-pyside6)
Requires:       python3-dbus
# Loaded with dlopen at runtime, so the automatic soname requires miss it.
Requires:       libxcb
Requires(posttrans): shadow-utils

%description
Ngó Sen is a Vietnamese input method for fcitx5, forked from fcitx5-lotus.

%prep
%setup -q -n %{upstream_name}-%{version}

%build
%cmake -DLOTUS_BYTECOMPILE_PYTHON:BOOL=OFF -DBUILD_TESTING:BOOL=ON -DNGOSEN_RUST_CORE:BOOL=ON
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
%{_bindir}/fcitx5-lotus-settings

%{_libdir}/fcitx5/liblotus.so

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
* Sun Oct 04 2026 Nguyen Phi <nguyenphidt@gmail.com> - 3.5.10-4
- First build published on the GitHub Releases page.

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
