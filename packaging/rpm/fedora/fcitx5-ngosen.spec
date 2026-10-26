Name:           fcitx5-ngosen
# Ngó Sen numbers its own releases from 0.5.0; the epoch keeps them above the 3.5.10 builds.
Epoch:          1
Version:        1.6.1
Release:        1%{?dist}
Summary:        Ngó Sen, a Vietnamese input method for fcitx5
License:        GPL-3.0-or-later AND OFL-1.1
URL:            https://github.com/ngosen/ngosen
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

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
Ngó Sen is a Vietnamese input method for fcitx5.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake -DNGOSEN_BYTECOMPILE_PYTHON:BOOL=OFF -DBUILD_TESTING:BOOL=ON
%cmake_build

%install
%cmake_install
%find_lang %{name}
%py_byte_compile %{__python3} %{buildroot}%{_datadir}/fcitx5-ngosen

%check
%ctest

%files -f %{name}.lang
%{_datadir}/licenses/%{name}/GPL-3.0-or-later.txt
%{_datadir}/licenses/%{name}/LGPL-2.1-or-later.txt
%{_datadir}/licenses/%{name}/OFL-1.1.txt

%dir %{_datadir}/licenses/%{name}
%{_bindir}/fcitx5-ngosen-settings
%{_bindir}/ngosen-save-log

%{_libdir}/fcitx5/libngosen.so
%{_libdir}/fcitx5/libngosenmigrate.so

%{_datadir}/fcitx5/addon/ngosen.conf
%{_datadir}/fcitx5/addon/ngosenmigrate.conf
%{_datadir}/fcitx5/default/vi_VN
%{_datadir}/fcitx5/inputmethod/ngosen.conf

%{_datadir}/fcitx5/ngosen/
%{_datadir}/fcitx5-ngosen/
%{_datadir}/applications/io.github.ngosen.NgoSen.Settings.desktop
%{_datadir}/metainfo/io.github.ngosen.NgoSen.metainfo.xml
%{_datadir}/gnome-shell/extensions/forward-keys@ngosen.github.io/

%{_datadir}/icons/hicolor/scalable/apps/*fcitx-ngosen*.svg
%{_datadir}/icons/hicolor/scalable/apps/io.github.ngosen.NgoSen.Settings.svg
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

%posttrans
echo "--- Cấu hình Ngó Sen ---"
# posttrans cannot tell an install from an upgrade. KWin starts fcitx5 itself on KDE Wayland, and
# one started with fcitx5 -rd cannot serve Qt apps.
if pgrep -x kwin_wayland >/dev/null 2>&1; then
    echo "Mở 'Fcitx5 Configuration' và thêm bộ gõ Ngó Sen, hoặc đăng xuất rồi đăng nhập lại nếu vừa cập nhật."
    echo "KDE Wayland: chọn 'Fcitx 5' trong System Settings → Virtual Keyboard."
else
    echo "Mở 'Fcitx5 Configuration' và thêm bộ gõ Ngó Sen, hoặc khởi động lại fcitx5 nếu vừa cập nhật: fcitx5 -rd"
fi

%changelog
* Mon Oct 26 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.6.1-1
- Cứu chữ and Cứu dấu hints follow the interface language, Vietnamese by default (#322).
- On a Vietnamese desktop, fcitx5's first profile lists Ngó Sen (#300, #316).
- Debug logging no longer records text typed in password fields (#321).

* Mon Oct 26 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.6.0-1
- Cứu chữ: words typed before switching to Vietnamese are retyped with tones (#71, #80, #82, #83).
- Cứu dấu: select a word and type a tone key to change its tone (#272, #290).
- A toggle key of its own for English and Vietnamese; new installs use Ctrl+Shift (#234, #242, #244).
- Lock-word key, tray menu items for input method and capitalization, menu item settings (#219, #240, #274, #275).
- Settings window: Vietnamese by default, separate Appearance and About pages, hints under options (#276, #277, #281).
- install.sh reloads fcitx5 after an update; the Go core is gone and bamboo-core is 0.5.2 (#223, #230, #298).

* Mon Oct 26 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.8-1
- Settings window: changes are saved again; 1.5.7 could not save any setting (#303, #305).
- README states how to finish setup after installing the package from a repository (#301).

* Sat Oct 24 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.7-1
- Settings window on KDE Plasma Wayland: it opens again when PyQt5 is also installed, by preferring Qt 6 (#278).
- Settings changed while the window was open across an fcitx5 restart are saved again (#279).
- The .deb packages depend on PyQt6 itself instead of the whole PyQt6 set, about 22 MB instead of 456 MB on a clean Ubuntu 26.04 (#280).
- Sen mode in Firefox rich text editors: after deleting a space, a tone key reopens the last word, so toan, space, Backspace, s gives toán (#285).
- Preedit mode on gemini.google.com: one Enter sends the message (#286).
- Sen mode on Firefox X11: keys typed quickly after a toned word stay in order (#289).
- Sen mode in xterm.js terminals on Firefox X11: the last word is reopened after Backspace (#292).

* Fri Oct 23 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.6-1
- Sen mode on X11: Google Sheets in Chromium-based browsers no longer jumps or loses letters (#258, #261).
- Sen mode in GLFW apps under XWayland, such as Minecraft Java: after one Backspace, adding a tone no longer deletes the earlier text (#247).
- KDE Plasma Wayland: the installer now sets XMODIFIERS, so X11 apps under XWayland such as xterm type Vietnamese (#251).
- The About page warns when fcitx5 still runs the Ngó Sen from before an update (#212).
- The typing log saved from the tray keeps about three times as much typing by skipping repeated reports (#213).
- Settings window: About text matches the README (#249), the Typing page fits large system fonts (#250), thin scrollbars on GNOME without a Qt theme (#252), and light/dark follows GNOME with Qt 6.8+ (#253).

* Tue Oct 20 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.5-1
- Recognise Flatpak apps by their usual program name, so Firefox from Flatpak no longer mixes up words while autofilling an address (#200, #204).
- Macros with an uppercase abbreviation such as VN now expand (#194).
- Sen mode: backspacing into the last word no longer rebuilds macro text that ends in a quote or ellipsis, or a word in a legacy charset; found by a test (#199).
- Typing the next letter of an address Firefox is autofilling is no longer taken for a click, so gox gives gõ and daaus gives dấu (#209).

* Tue Oct 20 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.4-3
- Rework the issue forms and add discussion forms (#190).

* Tue Oct 20 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.4-2
- Rewrite the bug report form (#188).

* Tue Oct 20 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.4-1
- Sen mode: wait for pages that redraw the box after each edit, so Chrome on GNOME no longer drops letters in Draft.js boxes such as TikTok comments (#176, #180).
- Macros whose text starts with the abbreviation keep the space after them (#179).

* Tue Oct 20 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.3-2
- Fedora COPR: build from the release tag again (#169).

* Mon Oct 19 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.3-1
- Sen mode: backspacing into the word before a space and typing on fixes that word (#142).
- Sen mode: fixing a word mid-way in gnome-text-editor no longer loses a letter (#149).
- Typing log: record keys under the right app after switching windows (#143).
- Settings: the Defaults button lights up as soon as there is something to reset (#132).
- Signed package repositories for Fedora (COPR), Debian/Ubuntu, Arch and openSUSE (#133-#140).

* Sun Oct 18 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.2-1
- Keep the mode key buttons on the Shortcuts page inside the list when the window is resized (#128).

* Sun Oct 18 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.1-1
- Same as 1.5.0, which was not released, plus the capitalize-after-Enter fix (#117).

* Sun Oct 18 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.5.0-1
- Add a check of the machine's fcitx5 setup to the settings window; the Rust core in every package but Debian 12.

* Sat Oct 17 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.4.3-1
- Turning Custom Dictionary off brings back the built-in one on fcitx5 older than 5.1.13.

* Fri Oct 16 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.4.2-1
- Fix lost letters with Sen mode on Sway and in empty Wayland fields; add arm64 packages and Kali install.

* Wed Oct 14 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.4.1-1
- The installer makes fcitx5 the KDE input method on Plasma Wayland; CI runs the installer.

* Wed Oct 14 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.4.0-1
- One-line installer sets up fcitx5 on GNOME and X11 desktops and can uninstall; hidden experiment options.

* Tue Oct 13 2026 Nguyen Phi <nguyenphidt@gmail.com> - 1:1.3.0-1
- Backspace can remove only the tone mark; new V icon; tray icon fixes on dark panels.

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
