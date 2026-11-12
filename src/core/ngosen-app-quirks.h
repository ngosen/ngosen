/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"

// How apps differ in taking text and deletions from the input method. Decided from what the field
// reports where possible; a program name is matched only when nothing reported tells the app apart.
namespace ngosen {

    // True when the frontend can deliver backspaces itself, so XTEST is not needed: the Wayland
    // input-method frontend, XIM clients under Xwayland, and IBus, D-Bus and fcitx4 clients other than SDL
    // and those takesSentKeyEvents picks.
    bool forwardsBackspaces(const Field& field);

    // True for Wine over XIM and WPS over IBus under Xwayland, where XTEST asks the user first:
    // backspaces go as key events sent to the focused X window instead.
    bool takesSentKeyEvents(const Field& field);

    // True for GTK4 clients, whose IM modules drop forwarded keys; delete through surrounding text there.
    bool ignoresForwardedKeys(const Field& field);

    // True when a key the app sent before our commit reached it can land ahead of that commit.
    bool appliesEarlyKeysFirst(const Field& field);

    // True for fields that report the terminal purpose (except Alacritty), xterm, and GNOME's VTE terminals
    // (Ptyxis, GNOME Console, GNOME Terminal) where GTK4 or GNOME Shell drops their purpose: they pass committed
    // text to the shell as is, so a DEL in it erases like Backspace.
    bool erasesWithCommittedDel(const Field& field);

    // True for fields that report the terminal or URL purpose, xterm and GTK4 VTE terminals, where -- is part
    // of a command or a URL, as in git push --force. Firefox's address bar reports nothing that sets it apart.
    bool takesDoubleHyphenLiterally(const Field& field);

    // A terminal window whose shell runs on one of the user's ptys, where a password prompt can hide input.
    bool runsShellOnPty(const Field& field);

    // True for the address bar of Chromium-based browsers on the D-Bus and fcitx4 frontends: it reports no
    // surrounding text and selects an inline autocompletion after the typed text.
    bool selectsOverAutocompletion(const Field& field);

    // True when a selection tells apart the fields selectsOverAutocompletion matches: the browser's own text
    // fields drop and regain focus, while web pages with a thin caret (Google Docs, Slides) keep focus and do
    // not replace the selection on a commit. Vivaldi's address bar is itself a web page, and nothing shows
    // that the fcitx4 frontend passes the focus change on, so both keep typing over the selection.
    bool focusChangeShowsBrowserField(const Field& field);

    // True for Chromium-based browsers and WPS Office: where they send no surrounding text, as Chromium on
    // X11 and WPS's own fcitx4 module do, a key typed there replaces the selection, which is also the X11
    // primary selection.
    bool typesOverPrimarySelection(const Field& field);

    // True for web pages in Chromium-based browsers over IBus or Wayland text-input. A page can keep its own text and take only key
    // presses (Lark Sheets): a deletion through the surrounding text then changes only the copy reported.
    bool pageMayKeepOwnText(const Field& field);

    // Under GNOME, apps reach fcitx5 through GNOME Shell's IBus bridge, which can relay the client's
    // surrounding text tens of milliseconds late, one edit at a time.
    bool surroundingTextLags(const Field& field);

    // A key passed to the app right after a commit can reach it first: a dbus client gets the commit
    // after the key's reply, and GNOME Shell's IBus bridge relays the two separately.
    bool keyOvertakesCommit(const Field& field);

    // mutter sends GNOME Shell's text-input edits once per main-loop turn, and the client keeps only the
    // last deletion and commit of a turn.
    bool keepsLastEditPerTurn(const Field& field);

    // Within GNOME Shell's turn limit, whether a deletion and its commit can go out together. Firefox and
    // its forks then leave the cursor one past the real one, and nothing they report shows it.
    bool takesJoinedEdit(const Field& field);

    bool namesNoApp(const Field& field);

    // GNOME acts on Tab and Esc in its search box before the input method sees them, so nothing typed
    // there could be taken back; retyping and retoning stay out of it.
    bool isGnomeSearch(const Field& field);

    // Wayland text input delivers a commit only to the field that has focus.
    bool dropsCommitsWithoutFocus(const Field& field);

    // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it.
    bool appliesBackspacesLate(const Field& field);

    // A field on these frontends that also takes keys deletes through its surrounding text when it can.
    // Through GNOME's IBus bridge forwarded backspaces can land after the commit that follows them (#362);
    // the other frontends keep backspaces until they are measured.
    bool deletesThroughSurroundingFirst(const Field& field);

    // These frontends turn a surrounding deletion's character count into bytes on their own copy of
    // the text, which can lag behind the app.
    bool countsDeletionOnCopy(const Field& field);

    // LibreOffice Calc over Wayland text-input tells the input method nothing when another cell is
    // selected; it reports the field only when a key reaches it, a lone Shift included.
    bool reportsFieldOnlyOnKey(const Field& field);

    // The address bar of Firefox and its forks sets no Url flag; callers recognise it by the text after the cursor.
    bool hidesAddressBarFlag(const Field& field);
    bool keepsOldCopyAfterEdit(const Field& field);

    // Firefox and its forks outside IBus can report a field holding only the start of the word just typed.
    bool reportsTrailKeys(const Field& field);

    // An app behind IBus may show a replacement in its report only when a later key arrives, so a
    // report that does not come at once says nothing about the deletion.
    bool reportsReplacementsWithLaterKey(const Field& field);

} // namespace ngosen
