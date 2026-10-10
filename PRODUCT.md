# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop

The settings window is a Linux desktop app (Python, Qt through qtpy) that must look at home on both
KDE Plasma and GNOME, and on Qt5 (Ubuntu 22.04) as well as Qt6.

## Users

Vietnamese Linux users of the Ngó Sen input method. The maintainer rarely opens the settings window,
and there is no data on who opens it most; assume a typical visit is rare and short: open, change one
thing (input method, typing mode, one app's mode, a macro), close. A smaller group edits macros, the
custom dictionary, the keymap or per-app rules.

## Product Purpose

Let users change how Ngó Sen types without editing config files, and make the rare visit fast: find
the setting, change it, see it take effect, leave.

## Positioning

Ngó Sen is a fork of fcitx5-lotus that now has its own name and direction. The settings window must
not look or read like the Lotus original.

## Operating Context

- Opened from the fcitx5 tray menu or the app launcher, usually right after something typed wrong.
- Settings reach the running fcitx5 over D-Bus (`org.fcitx.Fcitx.Controller1`); fcitx5 may not be
  running.
- Changes save immediately (confirmed 10/10/2026); no OK/Apply/Cancel.

## Capabilities and Constraints

- Settings: input method, typing mode, output charset, w→ư, bracket keys, spelling and auto-restore,
  typing extras, mode hotkeys and mode order, status icons, per-app modes, macros with time/date
  formats, custom dictionary, custom keymap, backup and restore.
- No new runtime dependencies beyond qtpy with PyQt6, PySide6 or PyQt5.
- Runtime names users already have stay unless a migration ships with the change: addon `ngosen`,
  config path `fcitx://config/addon/ngosen`, app id `io.github.ngosen.NgoSen.Settings`, gettext
  domain `fcitx5-ngosen`, icon `fcitx-ngosen`.
- UI strings are translatable (`_()`), Vietnamese in `po/vi.po`.

## Brand Commitments

- Name: Ngó Sen. No Lotus visuals (the lotus flower logo, "Lotus" wording) outside the credit to
  fcitx5-lotus.
- User-facing Vietnamese uses "test" and "lỗi" and keeps common English tech terms.

## Evidence on Hand

- Current screens: captured offscreen 10/10/2026 (not committed).
- Design direction chosen 11/10/2026 from four HTML mockups: native desktop look with a few touches
  from the website (ngosen.github.io). See DESIGN.md.
- No usage data, no user research.

## Product Principles

1. A visit is rare: the most common change is reachable in one step from opening the window.
2. Changes take effect at once and can be undone right there.
3. Native on both KDE and GNOME before being distinctive.
4. Fewer, clearer settings beat more pages.
