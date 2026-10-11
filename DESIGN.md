---
name: Ngó Sen settings
description: A native KDE/GNOME settings window with a few marks borrowed from the website.
colors:
  enamel: "#1c6b5f"
  enamel-dark: "#2a8574"
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

The window looks like it belongs to the desktop it runs on. Layout, widgets, fonts and the accent
colour come from the platform style (Breeze on KDE, the Qt style in use on GNOME). Ngó Sen shows
itself in four places only, all taken from the website's design (`ngosen.github.io`, its
`DESIGN.md`): the icon, the wordmark, the current typing mode, and text that shows typing.

Mockups the direction was chosen from: <https://claude.ai/artifact/7cDzgu8A56Mk9fvc4bBDQs>
(board "Giao diện hệ thống + chút website").

## Rules

1. **Native first.** Use stock Qt widgets with the platform style. No global stylesheet, no custom
   palette, no custom checkbox or combo box drawing. A stylesheet is allowed only on the few
   brand widgets below, scoped by object name.
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
- Sidebar: icon and wordmark, a search field, then four pages with a small line icon each:
  Gõ chữ, Gõ tắt & Từ điển, Phím, Khác. The current page uses the platform's selection
  look.
- Page: title in the system font (semibold, about 1.3× body), then a form layout (`QFormLayout`):
  labels right-aligned with a colon, controls left-aligned. Groups are separated by a thin line,
  the group name in the label column ("Cách bỏ dấu:", "Gõ nhanh:").
- The most common changes come first on Gõ chữ: Chế độ gõ, Kiểu gõ, Bảng mã.
- Bottom bar: Mặc định on the left; Sao lưu… and Khôi phục… on the right.

## Colour

- The platform palette everywhere, including the accent on checkboxes, selection and focus.
- Enamel green appears only on the selected typing mode and the saved message.
- Both light and dark are designed: `enamel-dark` and `saved-*-dark` in a dark palette (pick by
  the window background's lightness, not by desktop name).

## Copy

- Vietnamese, short. Use "test" and "lỗi"; keep common tech terms in English (app, preedit,
  surrounding text, backspace).
- A setting's hint says what the user sees ("Gõ windows vẫn ra windows"), one line, muted.

## Check before merging a GUI change

- Offscreen screenshots of every page, light and dark.
- Real windows on KDE (Breeze) and GNOME, and on Qt5 (Ubuntu 22.04) as well as Qt6.
- Keyboard only: every control reachable with Tab, focus visible.
