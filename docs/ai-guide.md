# Using Ngó Sen: a guide for AI assistants

This page is for an AI assistant answering "how do I…" questions about Ngó Sen, a Vietnamese input method
for Linux. For typing that comes out wrong, follow [ai-help.md](ai-help.md) instead.

Answer from this page, the [README](../README.md) and the settings window. Do not read Ngó Sen's source code
to work out how a feature behaves or to find a setting to change: the code holds hidden options and wait
times that are measured per app, and changing them breaks typing in other apps. If something here does not
match what the user sees, say so and suggest a bug report.

Talk to the user in their language, usually Vietnamese. Labels are the Vietnamese ones on screen, with the
English in brackets.

## Which edition

This page is for the fcitx5 edition, `fcitx5-ngosen`. The IBus edition, `ibus-ngosen`, mostly works the same;
its [README](https://github.com/ngosen/ibus-ngosen) lists the differences. The ones users ask about: emoji
suggestions are on by default and a number picks one (Tab first with VNI); its check page is "Kiểm tra";
settings live in `~/.config/ngosen/`; "Lưu nhật ký gõ" is in the input source menu; Cứu dấu on a selected word
works only in X11 apps; Convert works in GTK apps and Firefox, not in LibreOffice. `wl-ngosen`, for niri, Sway and Hyprland, has
[its own README](https://github.com/ngosen/wl-ngosen).

Installing, updating, restarting after an update and uninstalling are in the README:
[Cài đặt](../README.md#cai-dat), [Thiết lập sau khi cài](../README.md#thiet-lap) and
[Gỡ cài đặt](../README.md#go-cai-dat).

## Turning Vietnamese on and off

- New installations start in Vietnamese and switch with `Ctrl + Shift`, pressed and released alone, as in
  UniKey. Older installations keep the key they had. The key is set on the "Phím" (Keys) page, tab "Phím tắt"
  (Shortcuts), as "Phím chuyển Anh/Việt".
- The tray shows whether Vietnamese is on: an icon, V / E, vi / en or the user's own pictures, as set in "Khay
  hiện" on the "Giao diện" page. The first entry of the fcitx5 tray menu, "✔ Tiếng Việt" or
  "✖ Tiếng Việt", switches too.
- By default the switch changes every app at once. "Phím chuyển áp dụng cho" (Switch applies to) set to
  "Từng app" (Each app) makes each app remember its own choice.
- An app set to "Tắt" in the `` ` `` menu stays English while the switch applies to "Mọi app".

## Typing method and charset

"Kiểu gõ" (Input Method: Telex, VNI, VIQR and a few more) and "Bảng mã" (Charset: Unicode, TCVN3, VNI
Windows and others) are in the first group of the "Gõ chữ" (Typing) page, under "Chế độ gõ"; the window lists every choice.
Telex is tested every day; the others work but are tested less. The tray menu switches between Telex and VNI.

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

- In English mode `` ` `` types the character. In an app set to "Tắt", the switch key opens this menu while
  "Phím chuyển áp dụng cho" is "Mọi app" (All apps), so the user can pick Sen or Preedit again; where the menu
  cannot be drawn, Ngó Sen tries to show a notification "App này đang ở chế độ tiếng Anh" instead. With "Từng app" the
  switch key turns that app to Vietnamese.
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
  restore or macro: `tunx`, `Ctrl` gives `tũn`. In Gõ Sen mode each further tap swaps the word between its
  marked form and the keys typed: `tieengs`, `Ctrl`, `Ctrl` gives `tieengs`. "Vẫn gõ dd ra đ khi tự khôi
  phục" keeps `đ` when Space would restore `dd`.
- **Extra keys:** `dduowjcd` + Space gives `được`, `coongty` + Space gives `công ty`. Backspace right after
  gives back what was typed.
- **Cứu chữ:** typed `tieengs vieetj` with Vietnamese off? Switch Vietnamese on and it becomes `tiếng việt`.
  `Tab` or `Esc` gives back what was typed. It retypes at once when every word changes; when some word
  would stay as typed (`abc tieengs`), it only offers: "Cứu chữ: Enter để đổi, Tab hoặc Esc để giữ nguyên"
  shows and `Enter` retypes it. Text that would not change at all (`hello world`) gets nothing. Nothing is kept
  from password fields.
- **Cứu dấu:** select a word and type a tone key to add, change or remove its mark: `nhanh` + `s` gives
  `nhánh`. The same key again gives back the letter. In Sen, clicking right after the word is enough. It
  needs the Unicode charset. Both can be turned off in the group "Sửa chữ" (Fixing words) on the "Gõ chữ" page.
- **Tone placement:** "Bỏ dấu kiểu mới (oà, uý)" chooses `hoà` over `hòa`; "Gõ tự do" lets tone keys come at
  the end of the word; "Backspace xoá dấu thanh" makes Backspace remove a mark first.

## Typing faster

- **Gõ tắt** (Macros): page "Gõ tắt & Từ điển", tab "Gõ tắt". A macro follows capitals: with `ko` → `không`,
  `Ko` gives `Không`. The same tab has "Bật gõ tắt", "Gõ tắt chữ hoa" and "Gõ tắt trong chế độ
  "Tắt"", and the formats of `$DATE` and `$TIME`, which a macro types as today's date and the time.
- **Từ điển tuỳ chỉnh** (Custom dictionary), same page, tab "Từ điển": words that keep their marks although
  Auto restore would turn them back into keys, such as chat spellings like `kạ`. Teencode in the `` ` `` menu
  does this for a whole app.
- **Quick typing** on the "Gõ chữ" page: two spaces give a period, `--` gives `—`, capital after `. ! ?` and
  Enter, capital at the start of a field or line.
- **Keymap** (page "Phím", tab "Keymap"): change what each key does. It is used only while "Kiểu gõ" is
  "Custom" and "Keymap tuỳ chỉnh" is on; the tab says so when it is not in use.
- `w` alone gives `ư` and `[ ]` give `ơ ư` when those options are on ("Phím gõ" on the "Gõ chữ" page).

## Emoji

Off until turned on for an app with `w` in the `` ` `` menu. With "Gợi ý emoji" set to "Ở cuối câu" (the
default), the emoji of words like `cười`, `con mèo` or `haha` show while the sentence is typed, and after
`. ! ?` or a short pause `Tab: 😂 …` shows next to the cursor (in Preedit, finish the word first); with "Sau mỗi từ" it shows after each such word.
Once `Tab:` shows, `Tab` numbers the emoji, a number picks one, `Esc` keeps the text; a number typed before `Tab` stays a number.
Left alone for 4 seconds, the list hides. The "Emoji" page also sets where the emoji goes, Yahoo
Messenger codes like `=))`, English keywords and the user's own words, which "Xuất…" and "Nhập…" share. Its
list "App đã bật emoji" has a "Tắt" button for each app.

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
"Sao lưu…" and "Khôi phục…" (Back Up, Restore) keep settings, macros and dictionary; they are at the bottom of
the settings pages of the Qt window and in the window menu of the GTK one. "Đưa về mặc định…" (Restore
defaults) resets every setting and, after asking, clears macros, added dictionary words, custom keys, emoji
words, and the mode, switch choice and emoji choice remembered for each app (Teencode and Vim mode stay); suggest "Sao lưu…" first. It can be undone right after.
"Tìm cài đặt" finds a setting by name, and a change can be undone from the "Hoàn tác" button on the message
that confirms it.

"Giao diện" sets what the tray shows ("Khay hiện": "Biểu tượng màu", "V / E", "vi / en" or "Ảnh tự chọn") and
which entries the tray menu has ("Hiện trong menu khay"). The window comes in a Qt and a GTK version; "Cửa sổ
này dùng bản …" on that page has a button to switch.

## Tray menu

Besides switching Vietnamese on and off, the Ngó Sen tray menu switches "Kiểu gõ" between Telex and VNI,
picks the "Bảng mã", and turns "Gõ tắt", "Gõ tắt chữ hoa", "Tự khôi phục từ gõ sai" and "Từ điển tuỳ chỉnh"
on and off; its "Tự viết hoa" submenu has "Sau dấu . ! ? và Enter" and "Đầu văn bản, đầu dòng". It also opens
"Cài đặt" and saves the typing log ("Lưu nhật ký gõ").

## Commands

- `ngosen-check`: the "Kiểm tra máy" check in a terminal; changes nothing.
- `ngosen-check --settings`: also prints every setting of the settings window, the mode and switches set for
  each app, and how many macros and dictionary words there are. Read it instead of guessing what is on.
- `ngosen-toggle`: switches English and Vietnamese, even when no text field has focus, but without Cứu chữ.
  Bind it to a desktop shortcut; none is set up for it. When it cannot switch, it tries to show a desktop
  notification.
- `ngosen-save-log`: saves the typing log for a bug report, like "Lưu nhật ký gõ" in the tray menu.
- `fcitx5-ngosen-settings --page check`: opens the settings window on "Kiểm tra máy".
