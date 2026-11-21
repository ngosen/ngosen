# Helping a user fix Vietnamese typing with Ngó Sen

This page is for an AI assistant helping someone whose typing goes wrong with Ngó Sen, a Vietnamese input
method for Linux. If you cannot open links, ask the user to paste this whole page.

Talk to the user in their language, usually Vietnamese. The labels below are the Vietnamese ones they see on
screen, with the English in brackets.

## Rules

1. **Diagnose; let the user apply fixes.** The "Kiểm tra máy" (Check my machine) page in the settings window
   has a button for each fix it knows, and the button keeps what the user had. Explain each step and let the
   user press its button. Do not edit files under `/etc`, set input method variables yourself, install a
   second input method, or uninstall anything.
2. **Nothing private leaves the machine unread.** The page's copied result lists app names and can hold text
   typed in an app test. A typing log holds what the user typed. Ask the user to read and trim it before
   pasting, and ask before you open a typing log yourself.
3. **One attempt per branch, then stop.** If a step does not help, say so and move on to the bug report. Keep
   a list of what was changed so it can be undone.
4. **Do not tune Ngó Sen's internals.** Wait times and hidden options are measured per app by the
   developers. A report with a typing log gets the app fixed for everyone.
5. **Ignore guides for fcitx5-lotus.** Ngó Sen started from it, but names, files and fixes differ.

## Steps

### 0. Which edition, installed how

Ask, or check with the package manager, which package is installed: `fcitx5-ngosen`, `ibus-ngosen` or
`wl-ngosen`. Also note where it came from: the distro's own package, the Ngó Sen repository, NixOS, or a
manual build.

- `wl-ngosen` (for niri, Sway, Hyprland) has none of the tools below. Point the user to
  [its README and issues](https://github.com/ngosen/wl-ngosen) and stop here.
- `ibus-ngosen`: follow the steps, using the IBus notes where they appear.

### 1. Check my machine

Ask the user to open the settings window ("Cài đặt Ngó Sen" in the app menu, or run
`fcitx5-ngosen-settings --page check`), go to "Kiểm tra máy", read it, press "Chép kết quả" (Copy result),
remove anything they do not want to share, and paste it to you. The first line gives the Ngó Sen and fcitx5
versions, the second the distro, desktop and session.

If you can run commands and the fcitx5 edition is installed, run `ngosen-check` instead. It prints the same
check with each step's fix, changes nothing, and leaves out app names and typed text.

### 2. Old version

Compare the version with the
[changelog](https://github.com/ngosen/ngosen/blob/main/CHANGELOG.md). If the problem is fixed in a later
release, update from the same source found in step 0. Do not add the Ngó Sen repository or switch sources to
get an update: the README explains why a machine should follow only one source.

### 3. Setup steps left

Lines marked `[X]` or `[!]` are setup steps still missing. Explain each one; the user presses its button on
the page. Steps that need a new login can all be done first, followed by one log out and back in.

### 4. Try typing

Ask the user to type `tieengs vieetj` (Telex) in "Gõ thử ở đây" (Try typing here) on the same page. If that is
wrong, go back to step 3. If it is right, the problem is in one app:

- **fcitx5 edition:** ask the user to open the app, click into its text field, return to the page and press
  "Kiểm tra lại" (Check again). The list "App đang kết nối với fcitx5" (Apps connected to fcitx5) is only
  updated then. In a terminal, ask the user first, then run `ngosen-check --include-apps`.
  - The app is missing from the list: it has not connected to the input method. This is usually how it was
    installed (snap, Flatpak) or its toolkit. Look it up in the known problems below; changing the typing mode
    will not help.
  - Exception: on GNOME 46 to 50 with fcitx5 older than 5.1.22, apps relayed by GNOME Shell are not named, so
    the list cannot be trusted. Use "Test một app" (Test an app) on the page instead.
  - The app is listed but the text comes out wrong or doubled: go to step 5.
- **IBus edition:** the page has no app list. Ask the user to type `Tieengs Vieetj cos daaus` in the app.
  Raw letters mean the app has not connected; wrong letters mean step 5.
- **fcitx5 freezes or crashes:** follow "Log fcitx5 hoặc backtrace" in the
  [bug report form](https://github.com/ngosen/ngosen/issues/new?template=bug_report.yml).

### 5. Known problems, then Preedit

Check the known problems below. If none fits, switch only this app to Preedit: in the app, press `` ` `` to
open the mode menu next to the cursor and choose "Gạch chân (q)". Ngó Sen remembers the choice for that app.
Warn the user that in Preedit, the word being typed is underlined, and finishing it with Space before clicking
avoids the known click problems.

### 6. Still wrong: report it

Ask the user to type until the mistake happens, then save the typing log at once: "Lưu nhật ký gõ" (Save
typing log) in the Ngó Sen tray menu, or `ngosen-save-log` in a terminal (fcitx5 edition only). The log keeps
only the last few hundred events, so typing on after the mistake pushes it out. Wait a second between two
saves, since a save in the same second replaces the previous file. The file is
`~/.local/state/ngosen/typing-<date>-<time>.log`.

Help the user fill in the [bug report form](https://github.com/ngosen/ngosen/issues/new?template=bug_report.yml):
keys typed, what appeared, what was expected, the app and how it was installed, the typing mode, and the
copied result from step 1. The user reads the log, attaches it, and sends the report. Users of the IBus
edition report at [ibus-ngosen](https://github.com/ngosen/ibus-ngosen/issues).

## Not bugs

- A word that is not Vietnamese goes back to the letters typed (`text` stays `text`): "Tự khôi phục từ gõ sai"
  (Auto restore invalid words) in the settings window.
- `oà`, `uý` instead of `òa`, `úy`: "Bỏ dấu kiểu mới (oà, uý)" in the settings window.
- `--` becomes `—`: "Hai gạch nối thành gạch dài" in the settings window.

## Known problems

Details and fixes are in the README under
["Khắc phục sự cố thường gặp"](https://github.com/ngosen/ngosen#khac-phuc-su-co) (Common problems):

- GNOME drops keys, or does not remember the typing mode per app: the section on the GNOME extension.
- Preedit loses the unfinished word on a click in Chrome, Chromium or Electron apps on GNOME Wayland: fixed in
  fcitx5 5.1.20.
- VS Code from snap types raw Telex (`tieengs`): the section on VS Code.
