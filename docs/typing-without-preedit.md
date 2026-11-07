# How Vietnamese people type

Vietnamese users expect each letter to appear in the app as they type it, with the word corrected
in place when a tone mark arrives, and no preedit underline. Ngó Sen's default mode, Gõ Sen, works
this way. This page explains what that asks of the app and the toolkit, how
Ngó Sen copes today, and where a fix upstream would remove a workaround. It is written for people
who maintain apps, toolkits and other input methods; the Vietnamese page on the website is a
translation of it.

## Why Vietnamese typing wants to change committed text

In Telex, the most used Vietnamese input scheme, marks are typed after the letters they change:
`tieengs` becomes `tiếng`, letter by letter. `tieeng` is already on screen when the `s` arrives and
turns `ê` into `ế`. There are two ways to show this:

- **Preedit:** the input method keeps the word in an underlined composition until it is finished.
  Every toolkit supports it; it is how Chinese, Japanese and Korean input methods work. Vietnamese
  users dislike it: the underline flickers on every word, and fields with inline suggestions
  (address bars, search boxes, chat mentions) see no text until the word is done.
- **Committed text, corrected in place:** the input method commits each letter at once and, when a
  mark changes the word, deletes the characters before the cursor and commits the corrected word.
  No underline; suggestions follow every key. This is what Unikey did on Windows for twenty years
  and what Vietnamese users expect.

The second way needs one thing the first does not: **a reliable way to delete text the app
already has.**

## What the protocols already offer

The standard path exists at every layer:

- Wayland `text-input-v3`: `delete_surrounding_text(before, after)`, and `set_surrounding_text` so
  the input method knows what is there.
- fcitx5: `InputContext::deleteSurroundingText`, with the `SurroundingText` capability flag
  telling the input method whether the client supports it, and forwarded key events as a fallback.
- GTK 3 and 4: `gtk_im_context_delete_surrounding` and `retrieve_surrounding`.
- Qt: `QInputMethodEvent` with a replacement range.
- IBus: `delete-surrounding-text`, with the matching capability.

XIM, the X11 protocol, has no deletion request at all; a client over XIM can only receive key
events.

So a Vietnamese input method does not need a new protocol. It needs clients to implement what the
protocols already define, and to report their capabilities truthfully.

## How Ngó Sen deletes today

Ngó Sen decides from what the field reports (frontend, capability flags, surrounding text), and
matches a program name only when nothing reported tells the app apart. The checks are in
`src/core/ngosen-app-quirks.cpp`; the tests in `test/host-field-mapping.cpp`,
`test/ibus-dbus-forward-backspaces.cpp` and `test/x11-xtest-replacement.cpp` pin each case.

The default is not the standard path. Ngó Sen forwards Backspace key events, because every
frontend except XIM can deliver them and clients apply them in order with the commits that follow;
`deleteSurroundingText` is used only where forwarded keys are known to be dropped or reordered.
Surrounding text, where a client reports it, is read to check what is before the cursor, not to
delete.

| Path | Used for | Where it breaks |
| --- | --- | --- |
| Forwarded Backspace key events | The default: GTK 3 and Qt apps through the fcitx5 modules, Chromium and Electron apps, snap apps with the fcitx4 GTK module, clients of the Wayland frontends (KDE Plasma text-input; Sway and Hyprland input-method-v2), XIM clients under Xwayland. | Fields with inline autocompletion: a Backspace removes the suggested text first, so `cowf` in an address bar comes out as `coờ`. GTK 4 IM modules drop forwarded keys. GTK apps on Wayland text-input often apply a commit before the Backspace forwarded just ahead of it. LibreOffice runs Backspace asynchronously, so the next commit overtakes it. |
| `deleteSurroundingText` | Clients where forwarded keys fail: GTK 4 apps through fcitx5-gtk4 or IBus, and LibreOffice. | Needs the client to report surrounding text. |
| Select and overtype (Shift+Left, then commit) | Chromium-based browsers over D-Bus, whose address bar selects an inline autocompletion after the typed text; optionally Facebook's composers. | Terminals that match the same flags print the Shift+Left instead of selecting. |
| Synthetic keys through XTEST | XIM clients on an X11 session: Wine, WordCraft, SDL games. | Needs X11. Under Xwayland, XTEST asks the user for permission first, so Wine gets key events sent to its window instead. |
| A committed DEL character | GTK 4 VTE terminals (Ptyxis, GNOME Console), which take neither forwarded keys nor surrounding text but pass committed text to the shell, where DEL erases like Backspace. | Only terminals. |

Every row is a workaround for the same gap: an input method cannot yet rely on
`delete_surrounding_text` being implemented, and implemented in order with commits, across Linux
clients. When it can, the fake key presses go away.

## Behaviours we work around, and the fix that would retire each

Measured on the apps named, with the version and desktop in the linked Ngó Sen issue or pull
request. "Fix" is what the client could change; none of these need a protocol change.

- **Chromium-based browsers and Electron apps** (Chrome, Chromium, Edge, Brave, Vivaldi, Opera;
  VS Code, Discord, Zalo and other Electron apps) report no surrounding text to fcitx5 through the
  GTK IM module, which they use on X11 and by default on Wayland, so Ngó Sen cannot check what is
  before the cursor and forwards Backspace keys. Their address bar selects an inline autocompletion
  after the typed text, which a forwarded Backspace deletes. With Wayland text-input instead
  (`--enable-wayland-ime`), Chromium does send surrounding text; KWin passes it on, while Sway
  drops it after the first update (swaywm/sway#9191).
  Fix: report surrounding text through the GTK IM module as the Wayland text-input path does.
- **Firefox and its forks** (Firefox ESR on Debian, Zen, LibreWolf, Waterfox, Floorp, Mullvad
  Browser, GNU IceCat) set no URL input purpose on the address bar on Linux, so an input method
  cannot tell it from a text field and has to guess from the text after the cursor (#161, #162,
  #164). The address bar uses the `mozAwesomebar` input mode, which Firefox maps to URL on Windows
  and Android but not on GTK. Fix: map it to `GTK_INPUT_PURPOSE_URL`.
- **GTK 3 and GTK 4 on Wayland text-input**, which GTK apps use under GNOME by default, apply a
  commit before a key event that arrived just before it, so a forwarded Backspace followed by a
  commit often deletes the new text instead of the old. Fix: apply key events and text-input
  events in the order they arrive.
- **GTK 4 IM modules** (fcitx5-gtk4 and IBus's GTK 4 module) drop forwarded key events and do not
  pass on the terminal input purpose that VTE sets, so GTK 4 terminals get neither deletion path.
  Fix: deliver forwarded keys to the widget as the GTK 3 modules do, and pass the input purpose
  through.
- **SDL** takes only commits and preedit from the input method, so there is no way to delete text
  in an SDL game except real key presses, which only XTEST on X11 can send. Fix: implement
  `delete_surrounding_text` in SDL's Wayland and IBus text-input code.
- **LibreOffice** runs Backspace as an asynchronous shortcut, so a commit sent right after it lands
  before the deletion. With its Qt and KDE interface, the default on KDE Plasma, Calc reports the
  field only once a key reaches it, not when a cell is selected. Fix: apply forwarded Backspace in order with commits; report focus
  on cell selection.
- **Wine over XIM under Xwayland** applies forwarded key events after a later commit (#158).
  Fix: process X key events in order with XIM commits.
- **XIM itself** has no deletion request. Clients that still speak only XIM (Wine, WordCraft,
  older Java apps) get real key presses through XTEST on X11, which Xwayland gates behind a user
  prompt. Fix at the client: use the toolkit's native IM path instead of XIM.

## Upstream reports

Filed by Ngó Sen:

- Ubuntu 26.04, mutter: key events lost under GNOME 50 (the "forward-keys" extension in the
  README works around it): https://bugs.launchpad.net/ubuntu/+source/mutter/+bug/2169784
- Firefox's address bar reports a free-form input purpose instead of URL on GTK; measured on KDE
  Plasma with Firefox 157 (#215): https://bugzilla.mozilla.org/show_bug.cgi?id=2084459
- GTK applies a text-input commit before a key event received earlier, so a forwarded Backspace
  deletes the committed text; measured with an IBus test engine on GNOME 51, GTK 3.24 and 4.24:
  https://gitlab.gnome.org/GNOME/gtk/-/issues/8431

Filed by others, confirmed here:

- Sway drops the surrounding text Chromium sends over Wayland text-input; confirmed on Sway 1.12
  with Chromium 154 and fcitx5: https://github.com/swaywm/sway/issues/9191#issuecomment-6287333263
- Firefox on Wayland text-input sends surrounding text one edit late, so an input method that
  deletes surrounding text corrupts the word, even with 1.5 s between keys; confirmed on KDE Plasma
  with Firefox 157, with a protocol log: https://bugzilla.mozilla.org/show_bug.cgi?id=2080478#c2
- Chromium's GTK IM path reports no surrounding text; confirmed with fcitx5's capability flags on
  KDE Plasma with Chromium 154 (#220): https://issues.chromium.org/issues/40146869#comment20
- GTK 4 IM modules cannot deliver a forwarded key to the widget; confirmed on GNOME 51 with GTK
  4.24.1, where IBus's GTK 4 module dropped 19 of 19 forwarded Backspaces and its GTK 3 module
  applied them (#225): https://gitlab.gnome.org/GNOME/gtk/-/issues/3465
- LibreOffice runs Backspace as an asynchronous shortcut, so text committed after it lands first;
  confirmed in Writer 26.8.1 with an IBus test engine, 17 of 19 edits (#227):
  https://bugs.documentfoundation.org/show_bug.cgi?id=139807
- LibreOffice's Qt and KDE interface tells the input method nothing when the cursor moves by mouse
  click; confirmed in Calc 26.8.1 on KDE Plasma with a Wayland protocol log (#227):
  https://bugs.documentfoundation.org/show_bug.cgi?id=173064

Filed by others:

- SDL does not handle `delete_surrounding_text`: https://github.com/libsdl-org/SDL/issues/15987

The other behaviours above are not yet reported upstream. Reports should carry a minimal
reproduction without Ngó Sen (a small fcitx5 or `text-input-v3` client that commits and deletes),
the app version and the desktop, and link to this page. Each report gets an issue in the Ngó Sen
tracker that links to it.

## How to help

- **Linux users:** open the "Kiểm tra máy" (Check my machine) page in Ngó Sen's settings and report
  apps that still type wrong at https://github.com/ngosen/ngosen/issues, with the app, its version
  and your desktop. Each report either adds a test case or finds a bug above.
- **App and toolkit developers:** on Linux, report surrounding text to the input method and honour
  `delete_surrounding_text`; set the URL input purpose on address bars; deliver forwarded keys to
  the focused widget. The tests named above show what an input method expects; an issue here with
  the app's name is enough to get a fix tried against Ngó Sen.
- **Other input method projects:** Vietnamese input methods on Linux share these problems, and
  so does any input method that corrects committed text. Confirming an upstream report with your
  own reproduction weighs more than one project alone.
