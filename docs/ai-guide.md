# Using Ngó Sen: a guide for AI assistants

This page is for an AI assistant answering "how do I…" questions about Ngó Sen, a Vietnamese input method
for Linux. For typing that comes out wrong, follow [ai-help.md](ai-help.md) instead.

Answer from this page, the [README](../README.md) and the settings window. Do not read Ngó Sen's source code
to work out how a feature behaves or to find a setting to change: the code holds hidden options and wait
times that are measured per app, and changing them breaks typing in other apps. If something here does not
match what the user sees, say so and suggest a bug report.

Talk to the user in their language, usually Vietnamese. Labels are the Vietnamese ones on screen, with the
English in brackets.

## Turning Vietnamese on and off

- New installations start in Vietnamese and switch with `Ctrl + Shift`, pressed and released alone, as in
  UniKey. Older installations keep the key they had. The key is set on the "Phím" (Keys) page, tab "Phím tắt"
  (Shortcuts), as "Phím chuyển Anh/Việt".
- The tray icon shows V while typing Vietnamese. The first entry of the fcitx5 tray menu, "✔ Tiếng Việt" or
  "✖ Tiếng Việt", switches too.
- By default the switch changes every app at once. "Phím chuyển áp dụng cho" (Switch applies to) set to
  "Từng app" (Each app) makes each app remember its own choice.
- An app set to "Tắt" in the `` ` `` menu stays English whatever the switch says.

## Typing modes

- **Gõ Sen (Sen):** letters go straight into the app, without an underline. The recommended mode; Ngó Sen
  adapts to how browsers, terminals, Zalo and LibreOffice take text.
- **Gạch chân (Preedit):** the word being typed is underlined and sent to the app when it ends. Use it for an
  app where Sen types wrong.
- **Tắt (OFF):** English only, for one app.

The default mode for all apps is "Chế độ gõ" (Mode) on the "Gõ chữ" (Typing) page. A mode for one app is
picked in the `` ` `` menu.

## Mode menu (`` ` ``)

Many users have not used this menu, and it answers most "only this app types wrong" questions. With
Vietnamese on, pressing `` ` `` in an app's text field opens a menu next to the cursor. Its last line says which
app the choice is kept for: "App: <name>", or "cửa sổ này" (this window) when the app gave no name.

| Key | Item | What it does for this app |
| --- | --- | --- |
| `1` | Gõ Sen (Sen) | Types straight into the app, no underline. Usually the default. |
| `q` | Gạch chân (Preedit) | Underlines the word until it is finished. Try it when Sen types wrong in one app. |
| `w` | Gợi ý emoji: tắt/bật | Offers emoji after words like `cười`. Off until turned on here. |
| `t` | Teencode: tắt/bật | Keeps marks on chat spellings like `zậy`, `đag`. |
| `e` | Tắt (OFF) | Types English in this app only, for example a game or a code editor. |
| `v` | Vim mode: tắt/bật | Vim commands after Esc go through as typed, for nvim or Claude Code. |
| `` ` `` | Gõ `` ` `` | Types the `` ` `` character itself. |

Press the key, or move with the arrow keys or Tab and press Enter. Esc, a click, or 5 seconds without a key
closes it; any other key closes it and is typed. The `t` and `w` items only show while "Tự khôi phục từ gõ
sai" (Auto restore invalid words) and "Gợi ý emoji khi gõ" (Suggest emoji while typing) are on. The keys,
the menu key and the order of the items are set on the "Phím" (Keys) page, tab "Phím tắt" (Shortcuts).

- In English mode `` ` `` types the character. In an app set to "Tắt", pressing the English/Vietnamese switch
  key opens the menu, so the user can pick Sen or Preedit again.
- On GNOME Wayland without the "Input Method Panel" extension (`kimpanel@kde.org`), apps that GNOME Shell
  relays show no menu, and `` ` `` types the character. The install command adds that extension; "Kiểm tra
  máy" shows whether it is on.
- On GNOME, apps without a name share one rule: the last line says so, and a choice made there applies to
  all of them until the Ngó Sen GNOME extension is on.

## Fixing words while typing

- **Tự khôi phục từ gõ sai** (Auto restore invalid words): a word that cannot be Vietnamese goes back to the
  keys typed, so `windows` stays `windows`. To keep an English word that is also valid Telex, type its tone
  key twice: `thiss` gives `this`. Words starting with `/` stay as typed (`/cost`, `cd /usr/share`).
- **Phím chốt chữ** (Key to keep the word as typed): tap `Ctrl` alone to keep the word as shown, with no
  restore or macro: `tunx`, `Ctrl` gives `tũn`.
- **Extra keys:** `dduowjcd` + Space gives `được`, `coongty` + Space gives `công ty`. Backspace right after
  gives back what was typed.
- **Cứu chữ:** typed `tieengs vieetj` with Vietnamese off? Switch Vietnamese on and it becomes `tiếng việt`.
  `Tab` or `Esc` gives back what was typed. Nothing is kept from password fields.
- **Cứu dấu:** select a word and type a tone key to add, change or remove its mark: `nhanh` + `s` gives
  `nhánh`. The same key again gives back the letter. In Sen, clicking right after the word is enough. It
  needs the Unicode charset.
- **Tone placement:** "Bỏ dấu kiểu mới (oà, uý)" chooses `hoà` over `hòa`; "Gõ tự do" lets tone keys come at
  the end of the word; "Backspace xoá dấu thanh" makes Backspace remove a mark first.

## Typing faster

- **Gõ tắt** (Macros): page "Gõ tắt & Từ điển", tab "Gõ tắt". A macro follows capitals: with `ko` → `không`,
  `Ko` gives `Không`.
- **Từ điển tuỳ chỉnh** (Custom dictionary), same page, tab "Từ điển": words that keep their marks although
  Auto restore would turn them back into keys, such as chat spellings like `kạ`. Teencode in the `` ` `` menu
  does this for a whole app.
- **Quick typing** on the "Gõ chữ" page: two spaces give a period, `--` gives `—`, capital after `. ! ?` and
  Enter, capital at the start of a field or line.
- **Keymap** (page "Phím", tab "Keymap"): change what each key does in the typing method.
- `w` alone gives `ư` and `[ ]` give `ơ ư` when those options are on ("Phím gõ" on the "Gõ chữ" page).

## Emoji

Off until turned on for an app with `w` in the `` ` `` menu. Then after a word like `cười`, `con mèo` or
`haha`, `Tab: 😂 …` shows next to the cursor: `Tab` numbers the emoji, a number picks one, `Esc` keeps the
text. Left alone for 4 seconds, the list hides. The "Emoji" page sets where the emoji goes, Yahoo Messenger
codes like `=))`, English keywords, and the user's own words.

## Converting old text

Select text typed in an old charset (TCVN3, VNI…) and press the convert key set on the "Chuyển mã" (Convert)
page. The text becomes Unicode in place; `Ctrl+Z` brings it back. The same page converts pasted text and can
change case or remove marks. It does not work in terminals.

## The settings window

Open "Cài đặt Ngó Sen" from the app menu or the tray menu, or run `fcitx5-ngosen-settings`. Pages: "Gõ chữ"
(Typing), "Gõ tắt & Từ điển" (Macros & Dictionary), "Emoji", "Phím" (Keys), "Chuyển mã" (Convert), "Giao
diện" (Appearance: language, tray icon, tray menu entries), "Có gì mới & mẹo" (What's new & tips), "Kiểm tra
máy" (Check my machine), "Giới thiệu" (About).

Change settings there, not in files. The settings live in `~/.config/fcitx5/conf/ngosen.conf`, and the
per-app choices of the `` ` `` menu in `ngosen-app-rules.conf` and `ngosen-*-apps.conf` next to it. Reading
them can answer a question; the settings window writes them and tells Ngó Sen to reload.
"Sao lưu…" and "Khôi phục…" (Back Up, Restore) keep settings, macros and dictionary.

## Commands

- `ngosen-check`: the "Kiểm tra máy" check in a terminal; changes nothing.
- `ngosen-save-log`: saves the typing log for a bug report, like "Lưu nhật ký gõ" in the tray menu.
- `fcitx5-ngosen-settings --page check`: opens the settings window on "Kiểm tra máy".
