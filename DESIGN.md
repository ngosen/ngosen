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
  wordmark:
    fontFamily: "'Xanh Mono', 'DejaVu Sans Mono', monospace"
    fontSize: "22px"
    wordSpacing: "-0.28em"
  typed:
    fontFamily: "'Xanh Mono', 'DejaVu Sans Mono', monospace"
    fontSize: "16-17px"
  everything-else: "system font (QApplication default)"
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
   - The wordmark "Ngó Sen" in Xanh Mono next to it, with diacritics, never in bold.
   - The current typing mode (Sen, Preedit, Emoji, Tắt) as a segmented row of buttons; the checked
     one is filled enamel with `on-enamel` text.
   - Typed text in Xanh Mono: the keys-to-result example under the input method
     (`vieetj → việt`) and the test field.
3. **No lotus flower, no "Lotus" wording.** fcitx5-lotus appears only in the About credit.
4. **Changes save at once.** No OK, Apply or Cancel. After a change, an inline message at the top
   of the page says "Đã lưu “<setting>”." with a Hoàn tác button (`KMessageWidget`-style: enamel
   border, light enamel fill; `saved-*` colours). One message, replaced by the next change.
5. **Ship one font.** Xanh Mono (OFL) is bundled with the settings GUI. If it is missing, fall back
   to the system monospace font; nothing else changes.

## Layout

- Window: sidebar list on the left, page on the right. Below about 620px wide the form labels go
  above their fields.
- Sidebar: icon and wordmark, a search field, then six pages with a small line icon each:
  Gõ chữ, Gõ tắt & Từ điển, Phím, Giao diện, Có gì mới & mẹo, Giới thiệu. The current page uses
  the platform's selection look.
- Có gì mới & mẹo: one card per recent feature (name, how to use it, a typed example in Xanh
  Mono, an "Mở cài đặt" button that jumps to the setting), then the tips as plain lines. Nothing
  on it is a control, so the search field skips it. Gõ chữ shows one tip, picked at random, above
  its form.
- Page: title in the system font (semibold, about 1.3× body), then a form layout (`QFormLayout`):
  labels right-aligned with a colon, controls left-aligned. Groups are separated by a thin line,
  the group name in the label column ("Cách bỏ dấu:", "Gõ nhanh:").
- The most common changes come first on Gõ chữ: Chế độ gõ, Kiểu gõ, Bảng mã.
- Bottom bar: Mặc định on the left; Sao lưu… and Khôi phục… on the right.

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
settings only through `settings-gui/core/`, so both windows save the same values. The IBus edition's
window (its `DESIGN.md`) is the model for widgets and spacing.

- **libadwaita 1.5 or newer** (Ubuntu 24.04, Mint 22, Debian 13, Fedora 43). Where the system has an
  older one (Debian 12, Mint 21) the installer picks the Qt window.
- **Stock widgets.** `AdwNavigationSplitView` with the six pages in the sidebar, as in GNOME
  Settings; below about 600sp only one side shows. Each page is an `AdwPreferencesPage` of
  `AdwPreferencesGroup`s with `AdwSwitchRow`, `AdwComboRow`, `AdwEntryRow` and `AdwSpinRow`. A
  hint is the row's subtitle. No CSS beyond the brand marks and the accent colour.
- **The same four brand marks.** The icon and wordmark at the top of the sidebar; typed text in
  Xanh Mono through a `.typed` class; the typing mode as linked toggle buttons, the checked one
  filled enamel with `on-enamel` text (`enamel-dark` in dark mode). libadwaita 1.5 has no
  `AdwToggleGroup`.
- **Changes save at once.** Each change shows an `AdwToast` "Đã lưu “<setting>”." with Hoàn tác,
  one at a time, instead of the Qt window's inline message.
- **Bottom bar as in the Qt window**, an `AdwToolbarView` bottom bar: Mặc định on the left, Sao
  lưu… and Khôi phục… on the right, so both windows put them in the same place. Mặc định asks
  first in an `AdwAlertDialog`.
- **Enamel is the window's accent.** The app sets libadwaita's accent colours to `enamel` (light)
  and `enamel-dark` (dark) with `on-enamel` text, so switches, checkboxes, focus rings
  and suggested buttons are enamel. This one window ignores the accent the user picked in GNOME.
- **Light and dark** come from `AdwStyleManager`.
- **Search** is a `GtkSearchEntry` at the top of the sidebar that lists matching rows across pages,
  like the Qt window's search field.
- **Check before merging** a GTK page: screenshots light and dark at 1.5 and the newest
  libadwaita, a real window on GNOME and on Cinnamon, and keyboard-only use.
