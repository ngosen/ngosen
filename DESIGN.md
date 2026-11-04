---
name: Ngó Sen settings
description: A native KDE/GNOME settings window with a few marks borrowed from the website.
colors:
  enamel: "#1c6b5f"
  enamel-dark: "#247b6c"
  on-enamel: "#f0f5ee"
  cap: "#f1ead9"
  saved-bg: "#e1efe9"
  saved-bg-dark: "#183a33"
  saved-border: "#1c6b5f"
  saved-border-dark: "#3f9c8b"
typography:
  family: "system font (QApplication default)"
  wordmark:
    fontSize: "22px in the Qt window, 26px in the GTK window"
  typed:
    fontSize: "16-17px"
---

# Design: Ngó Sen settings window

The window looks like it belongs to the desktop it runs on. Layout, widgets and fonts come from the
platform style (Breeze on KDE, the Qt style in use on GNOME); the accent colour is Ngó Sen's enamel
green. Ngó Sen also shows itself in the icon, the wordmark, the current typing mode and text that
shows typing, all taken from the website's design (`ngosen.github.io`, its `DESIGN.md`).

Mockups the direction was chosen from: <https://claude.ai/artifact/7cDzgu8A56Mk9fvc4bBDQs>
(board "Giao diện hệ thống + chút website").

## Rules

1. **Native first.** Use stock Qt widgets with the platform style. No global stylesheet, no custom
   checkbox or combo box drawing. The palette is the platform's except its accent (`Highlight`,
   `Accent`, `HighlightedText`), set to enamel in `brand.install_accent` and set again whenever
   the desktop sends a new palette. A stylesheet is allowed only on the few
   brand widgets below, scoped by object name. One exception: on GNOME, Qt has no GNOME style and
   falls back to Fusion, whose scroll bars are wide with arrows. There, and only while the style is
   Fusion, each scroll bar gets a stylesheet for GNOME's thin rounded bar, drawn in the text colour
   seen through (`settings-gui/ui/scroll_bars.py`).
2. **Four brand marks, nothing more.**
   - The tray icon (`fcitx-ngosen`, a lotus-root slice on enamel green) at the top of the sidebar.
   - The wordmark "Ngó Sen" next to it, larger than the text around it, with diacritics, never in bold.
   - The current typing mode (Gõ Sen, Gạch chân, Chọn Emoji) as a segmented row of buttons; the checked
     one is filled enamel with `on-enamel` text.
   - Typed text, slightly larger than the text around it: the keys-to-result example under the input method
     (`vieetj → việt`) and the test field.
3. **Ngó Sen's own name and marks.** No lotus flower logo; the README credits the projects it builds on.
4. **Changes save at once.** No OK, Apply or Cancel. After a change, a message floats over the
   bottom of the page, which scrolls only if the focused control would be under it, and says
   "Đã lưu “<setting>”." with a Hoàn tác button
   (`KMessageWidget`-style: enamel border, light enamel fill; `saved-*` colours). One message,
   replaced by the next change. It leaves after 5 seconds unless the pointer or focus is on it;
   errors and messages whose Undo takes back a whole fix or list stay until closed.
5. **Ship no font.** All text uses the system font: distributions keep fonts in their own packages,
   and a monospace fallback can misplace stacked Vietnamese tones.

## Layout

- Window: sidebar list on the left, page on the right. Below about 620px wide the form labels go
  above their fields.
- Sidebar: icon and wordmark, a search field, then seven pages with a small line icon each:
  Gõ chữ, Gõ tắt & Từ điển, Phím, Giao diện, Có gì mới & mẹo, Kiểm tra máy, Giới thiệu. The current page uses
  the platform's selection look.
- Có gì mới & mẹo: one card per recent feature (name, how to use it, a typed example, an "Mở cài đặt" button that jumps to the setting), then the tips as plain lines. Nothing
  on it is a control, so the search field skips it. Gõ chữ shows one tip, picked at random, above
  its form.
- Page: title in the system font (semibold, about 1.3× body), then a form layout (`QFormLayout`):
  labels right-aligned with a colon, controls left-aligned. Groups are separated by a thin line,
  the group name in the label column ("Cách bỏ dấu:", "Gõ nhanh:"). A group with no checkbox to
  carry the name ("Phím gõ:") gets a line of its own.
- The most common changes come first on Gõ chữ: Chế độ gõ, Kiểu gõ, Bảng mã.
- A setting that does nothing because of a switch next to it is disabled, keeping its value
  (`DEPENDS` in `core/catalog.py`). Search and "Mở cài đặt" land on that switch. Lists the user
  writes (macros, dictionary, keymap) stay editable, so they can be filled before being turned on.
- Bottom bar: Đưa về mặc định… on the left; Sao lưu… and Khôi phục… on the right. Only on pages with
  settings: Có gì mới, Kiểm tra máy and Giới thiệu have nothing there to reset or back up.

## Colour

- The platform palette everywhere except the accent: checkboxes, selection, the current page and
  focus are enamel, with `on-enamel` text. This window ignores the accent the user picked.
- Both light and dark are designed: `enamel-dark` and `saved-*-dark` in a dark palette (pick by
  the window background's lightness, not by desktop name). `enamel-dark` is a shade darker than
  the website's so `on-enamel` text on it stays at 4.5:1. Against Breeze Dark's window it is 3.1:1;
  on a lighter dark window (`#353535`) it drops to 2.4:1, so a selection there also
  relies on its `on-enamel` text.

## Copy

- Vietnamese, short. Use "test" and "lỗi"; keep common tech terms in English (app, preedit,
  surrounding text, backspace).
- A setting's hint says what the user sees ("Gõ windows vẫn ra windows"), one line, muted.

## Check before merging a GUI change

- Offscreen screenshots of every page, light and dark.
- Real windows on KDE (Breeze) and GNOME, and on Qt5 as well as Qt6.
- Keyboard only: every control reachable with Tab, focus visible.

## GTK edition

The same window built with GTK 4 and libadwaita, for GNOME and Cinnamon. It follows the GNOME
Human Interface Guidelines the way the Qt window follows the platform style, and reads and writes
settings only through `settings-gui/core/`, so both windows save the same values.

- **libadwaita 1.5 or newer** (Ubuntu 24.04, Mint 22, Debian 13, Fedora 43). Where the system has an
  older one (Debian 12, Mint 21) the installer picks the Qt window.
- **Stock widgets.** `AdwNavigationSplitView` with the seven pages in the sidebar, as in GNOME
  Settings; below about 600sp only one side shows. Each page is an `AdwPreferencesPage` of
  `AdwPreferencesGroup`s with `AdwSwitchRow`, `AdwComboRow`, `AdwEntryRow` and `AdwSpinRow`. A
  hint is the row's subtitle. No CSS beyond the brand marks and the accent colour.
- **The same four brand marks.** The icon and wordmark at the top of the sidebar; typed text in
  a larger size through a `.typed` class; the typing mode as linked toggle buttons, the checked one
  filled enamel with `on-enamel` text (`enamel-dark` in dark mode). libadwaita 1.5 has no
  `AdwToggleGroup`.
- **Changes save at once.** Each change shows an `AdwToast` "Đã lưu “<setting>”." with Hoàn tác,
  one at a time, instead of the Qt window's inline message.
- **A header menu instead of the Qt window's bottom bar**, holding Sao lưu…, Khôi phục… and Đưa về
  mặc định…, as GNOME apps do. Đưa về mặc định… asks first in an `AdwAlertDialog`.
- **Enamel is the window's accent.** The app sets libadwaita's accent colours to `enamel` (light)
  and `enamel-dark` (dark) with `on-enamel` text, so switches, checkboxes, focus rings
  and suggested buttons are enamel. This one window ignores the accent the user picked in GNOME.
- **Light and dark** come from `AdwStyleManager`.
- **Search** is a `GtkSearchEntry` at the top of the sidebar that lists matching rows across pages,
  like the Qt window's search field.
- **Check before merging** a GTK page: screenshots light and dark at 1.5 and the newest
  libadwaita, a real window on GNOME and on Cinnamon, and keyboard-only use.
