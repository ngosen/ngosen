/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// The typing logic run against a fake app field, with no fcitx5 at all.
#include "fake-app.h"
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-recorder.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"
#include "ngosen-xtest.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using ngosen::Field;
using ngosen::test::FakeHost;
using ngosen::test::FakeKey;
using ngosen::test::FakeLoop;
using ngosen::test::FakeResources;

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what, const std::string& actual) {
        if (!ok) {
            std::cerr << "FAIL: " << what << "\n  got: " << actual << '\n';
            ++failures;
        }
    }

    std::string join(const std::vector<std::string>& parts) {
        std::string out;
        for (const auto& part : parts)
            out += "['" + part + "']";
        return out.empty() ? "(none)" : out;
    }

    Field makeField(const std::string& frontend, bool surroundingText, const std::string& program = {}) {
        Field field;
        field.frontend        = frontend;
        field.program         = program;
        field.surroundingText = surroundingText;
        field.preedit         = true;
        return field;
    }

    bool press(ngosen::TypingState& state, uint32_t sym) {
        FakeKey down(sym);
        state.keyEvent(down);
        FakeKey up(sym, true);
        state.keyEvent(up);
        return down.accepted();
    }

    // Types "tie" as plain keys, with the app reporting each letter when it reports surrounding text.
    void typeTie(ngosen::TypingState& state, FakeHost& host, FakeLoop& loop, bool appReports) {
        host.setSurrounding("", 0);
        const std::string word = "tie";
        for (size_t i = 0; i < word.size(); ++i) {
            press(state, static_cast<uint32_t>(word[i]));
            host.setSurrounding(word.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            if (appReports)
                state.surroundingUpdated();
            loop.pump(10);
        }
    }

    void testPreeditTelex() {
        realMode = ngosen::Mode::Preedit;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (char c : std::string("vieetj"))
            press(state, static_cast<uint32_t>(c));
        check(host->preedit() == "việt", "preedit: Telex vieetj shows việt", host->preedit());
        check(host->commits().empty(), "preedit: nothing committed while typing", join(host->commits()));

        press(state, ngosen::key::space);
        check(join(host->commits()) == "['việt ']", "preedit: space commits the word with the space", join(host->commits()));
    }

    // GTK apps reset the input context when Enter inserts a new line; resetAfter says after which key.
    std::string typeWithAutoCapitalize(const std::vector<uint32_t>& keys, size_t resetAfter = SIZE_MAX, bool focusOut = false) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().autoCapitalizeAfterPunctuation = true;
        auto                owned                                 = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host                                  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        for (size_t i = 0; i < keys.size(); ++i) {
            press(state, keys[i]);
            if (i == resetAfter)
                state.reset(focusOut);
        }
        std::string text;
        for (const auto& commit : host->commits())
            text += commit;
        return text + host->preedit();
    }

    void testAutoCapitalize() {
        using namespace ngosen::key;
        const std::string afterPeriod = typeWithAutoCapitalize({'b', 'a', 'n', period, space, 't', 'o', 'o', 'i'});
        check(afterPeriod == "ban. Tôi", "auto-capitalize: a letter after '. ' is upper case", afterPeriod);
        const std::vector<uint32_t> enterKeys{'b', 'a', 'n', Return, 't', 'o', 'o', 'i'};
        const std::string           afterEnter = typeWithAutoCapitalize(enterKeys, 3);
        check(afterEnter == "banTôi", "auto-capitalize: a letter after Enter is upper case though the app resets", afterEnter);
        const std::string afterFocusOut = typeWithAutoCapitalize(enterKeys, 3, true);
        check(afterFocusOut == "bantôi", "auto-capitalize: leaving the field forgets the sentence start", afterFocusOut);
    }

    void testSenForwards(const std::string& frontend, bool surroundingText) {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        Field               field = makeField(frontend, surroundingText);

        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, surroundingText);
        const std::string what = "sen " + frontend + ": ";
        check(join(host->commits()) == "(none)", what + "plain letters go to the app as keys", join(host->commits()));

        check(press(state, static_cast<uint32_t>('e')), what + "e -> ê takes the key", "not accepted");
        // XIM forwards on the next event loop turn, after its sync reply.
        loop.pump(3);
        const auto& forwarded = host->forwarded();
        check(forwarded.size() == 2 && forwarded[0].key == ngosen::EditKey::BackSpace && !forwarded[0].release && forwarded[1].release,
              what + "forwards one backspace press and release", std::to_string(forwarded.size()) + " forwarded keys");
        check(host->deletes().empty(), what + "no surrounding text deletion", std::to_string(host->deletes().size()));

        if (surroundingText) {
            check(host->commits().empty(), what + "no commit before the app reports the deletion", join(host->commits()));
            host->setSurrounding("ti", 2);
            state.surroundingUpdated();
        }
        loop.pump(150);
        check(join(host->commits()) == "['ê']", what + "commits ê", join(host->commits()));
    }

    // Sway (wlroots) reports the text and then a done event; a commit sent before the done carries a
    // stale serial and is dropped.
    void testCommitAfterTheReport() {
        Field                   field = makeField("wayland_v2", true);
        ngosen::test::AppQuirks quirks;
        quirks.dropsCommitsInReport = true;
        const std::vector<uint32_t> keys{'d', 'd', 'w', 'o', 'w', 'n', 'g', 'f'};
        const std::string           text = ngosen::test::typeInApp(field, quirks, keys, std::vector<int>(keys.size(), 100));
        check(text == "đường", "sway: commits go out after the app's report", text);
    }

    // Zed leaves focus for a moment when a deletion empties its field; a commit sent then is lost (#93).
    void testCommitAfterFieldRefocus() {
        Field                   field = makeField("wayland", true);
        ngosen::test::AppQuirks quirks;
        quirks.refocusWhenEmptiedMs = 13;
        const std::vector<uint32_t> keys{'d', 'd', 'o', 'w', 'n'};
        const std::string           text = ngosen::test::typeInApp(field, quirks, keys, std::vector<int>(keys.size(), 100));
        check(text == "đơn", "emptied field: commit after focus comes back", text);
    }

    void testMacroKeepsSpace() {
        FakeResources resources;
        resources.setMacros({{"e", "em"}, {"ko", "không"}});
        const std::vector<uint32_t> keys{'e', ' ', 'k', 'o', ' ', 'a'};
        const std::string           text = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
        check(text == "em không a", "macro keeps its space", text);
    }

    // TikTok's comment box in Chrome under GNOME applies our backspaces late and loses a commit that
    // arrives before the redraw they cause (#175).
    void testCommitAfterEditorRedraw() {
        Field                   field = makeField("ibus", true);
        ngosen::test::AppQuirks quirks;
        quirks.keyStepMs         = 6;
        quirks.redrawAfterEditMs = 7;
        const std::vector<uint32_t> keys{'t', 'i', 'e', 'e', 'n', 'g', 's'};
        const std::string           text = ngosen::test::typeInApp(field, quirks, keys, std::vector<int>(keys.size(), 100));
        check(text == "tiếng", "redrawing editor: commit after the redraw", text);

        // A box that redraws after each of our backspaces only, with over a second of plain words in between.
        quirks.keyStepMs                   = 10;
        quirks.redrawsOnlyOnDeletion       = true;
        const std::string           spaced = "tieengs xin xin xin chaof";
        const std::vector<uint32_t> spacedKeys(spaced.begin(), spaced.end());
        int                         slowest = 0;
        const std::string           after   = ngosen::test::typeInApp(field, quirks, spacedKeys, std::vector<int>(spacedKeys.size(), 100), &slowest);
        // The first word may be lost: nothing has shown yet that the box redraws.
        const std::string tail = " xin xin xin chào";
        check(after.size() >= tail.size() && after.compare(after.size() - tail.size(), tail.size(), tail) == 0, "redrawing editor: commit after the redraw following a pause",
              after);
        // Three backspaces take 30 ms and their last redraw 7 ms more; the wait gives up at 100 ms.
        check(slowest < 60, "redrawing editor: commit once the redraws are done", std::to_string(slowest) + " ms");
    }

    // Switching fields quickly is not a redraw, so a field that never redraws commits as fast as before.
    void testQuickSwitchesNoRedrawWait() {
        for (const auto& [frontend, surrounding] : std::vector<std::pair<std::string, bool>>{{"xim", false}, {"wayland", true}}) {
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, makeField(frontend, surrounding), ngosen::test::AppQuirks{});
            auto*               app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            for (int i = 0; i < 2; ++i) {
                loop.pump(30);
                state.deactivate(true);
                loop.pump(3);
                state.activate(ngosen::Mode::Sen, true);
            }
            for (const char c : std::string("tie")) {
                app->type(static_cast<uint32_t>(c));
                loop.pump(100);
            }
            const uint64_t start = ngosen::monotonicUs();
            app->type('e');
            while (app->text() != "tiê" && ngosen::monotonicUs() - start < 300000)
                loop.pump(1);
            const auto ms = static_cast<int>((ngosen::monotonicUs() - start) / 1000);
            check(app->text() == "tiê" && ms < 90, "quick field switches: no wait for a redraw (" + frontend + ")", app->text() + ", " + std::to_string(ms) + " ms");
        }
    }

    // Backspacing from after a word into it and typing on continues that word; kitty reports no
    // surrounding text, so the word comes from what we committed.
    void testBackspaceIntoLastWord(const std::string& frontend, bool surroundingText, ngosen::test::AppQuirks quirks = {}) {
        const Field                                                      field = makeField(frontend, surroundingText);
        const std::string                                                what  = "backspace into last word (" + frontend + (surroundingText ? ", surrounding): " : "): ");
        const uint32_t                                                   bs    = ngosen::key::BackSpace;
        const std::vector<std::pair<std::vector<uint32_t>, std::string>> cases{
            {{'d', 'u', 'y', 'e', 'e', 't', ' ', bs, bs, bs, bs, bs, 'y', 'e', 'e', 'j', 't', ' '}, "duyệt "},
            {{'t', 'o', 'a', 'n', ' ', bs, bs, 'n', 'f', ' '}, "toàn "},
            {{'b', 'a', 'n', ' ', bs, 'f', ' '}, "bàn "},
        };
        for (const auto& [keys, want] : cases) {
            const std::string text = ngosen::test::typeInApp(field, quirks, keys, std::vector<int>(keys.size(), 100));
            check(text == want, what + want, text);
        }
    }

    // After a click elsewhere the text before the cursor is not our word, so it is not reopened.
    void testBackspaceAfterMovingAway() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        const std::string   typed = "ban ";
        for (size_t i = 0; i < typed.size(); ++i) {
            press(state, static_cast<uint32_t>(typed[i]));
            host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            state.surroundingUpdated();
        }
        host->setSurrounding("xyz", 3);
        state.surroundingUpdated();
        press(state, ngosen::key::BackSpace);
        host->setSurrounding("xy", 2);
        state.surroundingUpdated();
        check(!press(state, static_cast<uint32_t>('f')), "backspace after moving away: f goes to the app as a key", "f was taken");
    }

    // gnome-text-editor sends a key that types no text back once more, marked as repeated; counting the
    // backspace twice left the word one letter short of the app's text.
    void testResentBackspace() {
        ngosen::test::AppQuirks quirks;
        quirks.resendsKeysWithoutText                                       = true;
        const uint32_t                                                   bs = ngosen::key::BackSpace;
        const std::vector<std::pair<std::vector<uint32_t>, std::string>> cases{
            {{'t', 'o', 'a', 'n', bs, 'n', 'f', ' '}, "toàn "},
            {{'b', 'a', 'n', ' ', bs, 'f', ' '}, "bàn "},
        };
        for (const auto& [keys, want] : cases) {
            const std::string text = ngosen::test::typeInApp(makeField("dbus", true, "gnome-text-editor"), quirks, keys, std::vector<int>(keys.size(), 100));
            check(text == want, "resent backspace: " + want, text);
        }
    }

    // Ctrl+Backspace deletes text we cannot follow without surrounding text.
    void testBackspaceAfterCtrlBackspace() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        ngosen::TypingState state(&resources, std::make_unique<FakeHost>(loop, makeField("wayland", false)));

        for (const char c : std::string("ban "))
            press(state, static_cast<uint32_t>(c));
        FakeKey wordDelete(ngosen::key::BackSpace, false, ngosen::modifier::Ctrl);
        state.keyEvent(wordDelete);
        press(state, ngosen::key::BackSpace);
        check(!press(state, static_cast<uint32_t>('f')), "backspace after ctrl+backspace: f goes to the app as a key", "f was taken");
    }

    // A key typed while the app has not yet applied the backspaces waits and goes out with the
    // replacement in one commit.
    void testKeyDuringReplacement() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        press(state, static_cast<uint32_t>('e'));
        check(press(state, static_cast<uint32_t>('n')), "key during replacement: n is held back", "not accepted");
        host->setSurrounding("ti", 2);
        state.surroundingUpdated();
        loop.pump(150);
        check(join(host->commits()) == "['ên']", "key during replacement: one commit with the held key", join(host->commits()));
    }

    // GNOME keeps only the last commit of a key event, so a key that ends a timed wait and commits text
    // itself must send the pending text in the same commit.
    void testOneCommitAfterTimedWait() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("ibus", false));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        press(state, static_cast<uint32_t>('d'));
        press(state, static_cast<uint32_t>('d'));
        check(host->commits().empty(), "one commit after timed wait: đ waits for the backspace", join(host->commits()));
        check(press(state, static_cast<uint32_t>('w')), "one commit after timed wait: w is taken", "went to the app");
        check(join(host->commits()) == "['đư']", "one commit after timed wait: đ and ư go out together", join(host->commits()));
    }

    // GTK4 drops forwarded keys, so the deletion goes through the surrounding text.
    void testGtk4DeletesSurrounding() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("dbus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        press(state, static_cast<uint32_t>('e'));
        loop.pump(150);
        check(host->forwarded().empty(), "gtk4: no forwarded keys", std::to_string(host->forwarded().size()));
        check(host->deletes().size() == 1 && host->deletes()[0] == std::pair<int, unsigned int>(-1, 1), "gtk4: deletes one character before the cursor",
              std::to_string(host->deletes().size()) + " deletions");
        check(join(host->commits()) == "['ê']", "gtk4: commits ê", join(host->commits()));
    }

    // SDL takes only commits and preedit; forwarded backspaces never reach it.
    void testSdlGetsNoForwardedBackspace() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("ibus", false, "SDL_App"));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (char c : std::string("tiee"))
            press(state, static_cast<uint32_t>(c));
        loop.pump(100);
        check(host->forwarded().empty(), "sdl: no forwarded keys", std::to_string(host->forwarded().size()));
        check(host->deletes().empty(), "sdl: no surrounding text deletion", std::to_string(host->deletes().size()));
    }

    bool has(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    // Types "tiee" in Sen mode and returns what was committed, with DEL shown as <DEL>.
    std::string commitsForTiee(const std::string& program, bool& eTaken, const std::string& frontend = "dbus") {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField(frontend, false, program));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (char c : std::string("tie"))
            press(state, static_cast<uint32_t>(c));
        eTaken = press(state, static_cast<uint32_t>('e'));
        loop.pump(100);
        std::string out = join(host->commits());
        for (size_t at = out.find('\x7f'); at != std::string::npos; at = out.find('\x7f', at))
            out.replace(at, 1, "<DEL>");
        return out;
    }

    void testTerminalErasesWithCommittedDel() {
        bool              taken = false;
        const std::string vte   = commitsForTiee("ptyxis", taken);
        check(taken, "ptyxis: e -> ê takes the key", "not accepted");
        check(vte == "['<DEL>ê']", "ptyxis: one commit erases e and types ê", vte);

        const std::string console = commitsForTiee("kgx", taken);
        check(console == "['<DEL>ê']", "kgx: one commit erases e and types ê", console);

        const std::string viaIbus = commitsForTiee("gtk4-im:ptyxis", taken, "ibus");
        check(viaIbus == "['<DEL>ê']", "ibus ptyxis: one commit erases e and types ê", viaIbus);

        const std::string sdl = commitsForTiee("Medieval2", taken);
        check(!has(sdl, "<DEL>"), "dbus sdl: never gets DEL", sdl);
    }

    // The log a user saves after a wrong word must show the keys, what was sent and what the app said.
    void testRecorderKeepsTheReplacement() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true, "firefox"));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        press(state, static_cast<uint32_t>('e'));
        host->setSurrounding("ti", 2);
        state.surroundingUpdated();
        loop.pump(150);

        const std::string log = resources.recorder().dump();
        check(has(log, "\tfield\tfrontend=wayland program=firefox surrounding=1 preedit=1 mode=Sen\n"), "recorder: notes the field and mode", log);
        check(has(log, "\tkey\tdown 0x0065 e\n"), "recorder: notes the key", log);
        check(has(log, "\tforward\tBackSpace down\n"), "recorder: notes the forwarded backspace", log);
        check(has(log, "\tapp\t\"ti|\" cursor=2 anchor=2 length=2\n"), "recorder: notes the app's report", log);
        check(has(log, "\tcommit\t\"ê\"\n"), "recorder: notes the commit", log);
    }

    // Every field shares one recorder, so coming back to a field must note it again.
    void testRecorderNotesFieldSwitch() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        ngosen::TypingState terminal(&resources, std::make_unique<FakeHost>(loop, makeField("wayland", false, "kitty")));
        ngosen::TypingState chat(&resources, std::make_unique<FakeHost>(loop, makeField("ibus", false, "zalo")));

        press(terminal, static_cast<uint32_t>('a'));
        press(chat, static_cast<uint32_t>('b'));
        press(terminal, static_cast<uint32_t>('c'));
        const std::string log  = resources.recorder().dump();
        const auto        last = log.rfind("\tfield\t");
        check(last != std::string::npos && has(log.substr(last), "program=kitty") && has(log.substr(last), "0x0063 c"), "recorder: notes the field again on coming back", log);
    }

    void testRecorderHidesPasswords() {
        realMode = ngosen::Mode::Sen;
        FakeLoop      loop;
        FakeResources resources;
        Field         field       = makeField("wayland", true, "firefox");
        field.password            = true;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        press(state, static_cast<uint32_t>('e'));
        host->setSurrounding("ti", 2);
        state.surroundingUpdated();
        loop.pump(150);

        const std::string log = resources.recorder().dump();
        check(has(log, " password=1 ") && has(log, "\tkey\thidden\n"), "recorder: marks the password field", log);
        check(!has(log, "0x0065") && !has(log, "\"ti") && !has(log, "\"ê\"") && !has(log, " e\n"), "recorder: keeps no password text", log);
    }

    // Focus that leaves a field and comes straight back keeps the word; a real switch starts a new
    // one. Chromium on Wayland reports the text while it leaves, as it does when a page redraws its
    // editor after every key (TikTok comments, #175).
    void testFocusBounce() {
        for (const bool reports : {false, true}) {
            for (const bool bounce : {true, false}) {
                realMode = ngosen::Mode::Sen;
                FakeLoop            loop;
                FakeResources       resources;
                auto                owned = std::make_unique<FakeHost>(loop, makeField(reports ? "wayland" : "xim", reports));
                FakeHost*           host  = owned.get();
                ngosen::TypingState state(&resources, std::move(owned));

                const std::string   typed = "tie";
                for (size_t i = 0; i < typed.size(); ++i) {
                    press(state, static_cast<uint32_t>(typed[i]));
                    if (reports)
                        host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
                }
                state.deactivate(true);
                if (!bounce)
                    loop.pump(150);
                state.activate(ngosen::Mode::Sen, true);
                const std::string field = reports ? " (field reports its text)" : "";
                const bool        taken = press(state, static_cast<uint32_t>('e'));
                if (bounce)
                    check(taken, "focus bounce: keeps the word" + field, "e went to the app as a plain key");
                else
                    check(!taken, "focus switch: starts a new word" + field, "e was taken to change the old word");
            }
        }

        // A quick switch to another field on the same context looks like a bounce until that field
        // reports text without the word before the cursor.
        for (const bool otherField : {true, false}) {
            realMode = ngosen::Mode::Sen;
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            typeTie(state, *host, loop, true);
            loop.pump(40);
            state.deactivate(true);
            loop.pump(3);
            state.activate(ngosen::Mode::Sen, true);
            host->setSurrounding(otherField ? "xin chao" : "tie", otherField ? 8 : 3);
            state.surroundingUpdated();
            const bool taken = press(state, static_cast<uint32_t>('e'));
            if (otherField)
                check(!taken, "quick switch to another field: starts a new word", "e was taken to change the old word");
            else
                check(taken, "focus bounce with the word still there: keeps it", "e went to the app as a plain key");
        }
    }

    void testHomeDictionaryPath() {
        check(isInHomeDirectory("/home/an/.local/share/fcitx5/ngosen/vietnamese.cm.dict"), "dictionary path: the custom one is skipped when it is off",
              "a dictionary under /home was not recognised");
        check(!isInHomeDirectory("/usr/share/fcitx5/ngosen/vietnamese.cm.dict"), "dictionary path: the built-in one is kept", "the built-in dictionary was taken for a custom one");
    }

    void testWithoutToneMark() {
        check(withoutToneMark("tiếng") == "tiêng", "tone mark: tiếng loses only the sắc", withoutToneMark("tiếng"));
        check(withoutToneMark("Đường") == "Đương", "tone mark: Đường keeps đ and the horns", withoutToneMark("Đường"));
        check(withoutToneMark("VIỆT") == "VIÊT", "tone mark: upper case", withoutToneMark("VIỆT"));
        check(withoutToneMark("chào") == "chao", "tone mark: huyền", withoutToneMark("chào"));
        check(withoutToneMark("ăn") == "ăn", "tone mark: a word without one is unchanged", withoutToneMark("ăn"));
        // Every row keeps its base when the tone is taken off, so a table rewrite cannot drift.
        const char* rows[] = {
            "aáàảãạ", "ăắằẳẵặ", "âấầẩẫậ", "eéèẻẽẹ", "êếềểễệ", "iíìỉĩị", "oóòỏõọ", "ôốồổỗộ", "ơớờởỡợ", "uúùủũụ", "ưứừửữự", "yýỳỷỹỵ",
            "AÁÀẢÃẠ", "ĂẮẰẲẴẶ", "ÂẤẦẨẪẬ", "EÉÈẺẼẸ", "ÊẾỀỂỄỆ", "IÍÌỈĨỊ", "OÓÒỎÕỌ", "ÔỐỒỔỖỘ", "ƠỚỜỞỠỢ", "UÚÙỦŨỤ", "ƯỨỪỬỮỰ", "YÝỲỶỸỴ",
        };
        for (const char* row : rows) {
            const std::string_view view      = row;
            const size_t           baseBytes = ngosen::utf8::charBytes(view);
            const std::string      base(view.substr(0, baseBytes));
            for (size_t i = baseBytes; i < view.size();) {
                const size_t      bytes = ngosen::utf8::charBytes(view.substr(i));
                const std::string toned(view.substr(i, bytes));
                check(withoutToneMark(toned) == base, "tone mark: table row keeps its base", withoutToneMark(toned));
                i += bytes;
            }
        }
        check(withoutToneMark("tiếng Việt 123!") == "tiêng Viêt 123!", "tone mark: mixed sentence", withoutToneMark("tiếng Việt 123!"));
    }

    // Backspace with BackspaceRemovesTone takes the mark off the word being typed (#34).
    void testBackspaceRemovesTone(bool enabled) {
        realMode = ngosen::Mode::Sen;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().backspaceRemovesTone = enabled;
        // GTK4 deletes through the surrounding text, so each step lands at once.
        auto                owned = std::make_unique<FakeHost>(loop, makeField("dbus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        host->setSurrounding("", 0);
        state.surroundingUpdated();
        const std::string shown[] = {"a", "â", "ấ"};
        for (int i = 0; i < 3; ++i) {
            press(state, static_cast<uint32_t>("aas"[i]));
            loop.pump(150);
            host->setSurrounding(shown[i], 1);
            state.surroundingUpdated();
            loop.pump(50);
        }

        const std::string what  = enabled ? "backspace removes tone: " : "backspace removes tone off: ";
        const bool        taken = press(state, ngosen::key::BackSpace);
        loop.pump(150);
        if (enabled) {
            check(taken, what + "backspace on ấ is taken", "went to the app");
            check(!host->commits().empty() && host->commits().back() == "â", what + "leaves â", join(host->commits()));
            host->setSurrounding("â", 1);
            state.surroundingUpdated();
            loop.pump(50);
            check(!press(state, ngosen::key::BackSpace), what + "backspace on â deletes as usual", "was taken");
        } else {
            check(!taken, what + "backspace goes to the app", "was taken");
        }
    }

    void testPreeditBackspaceRemovesTone() {
        realMode = ngosen::Mode::Preedit;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().backspaceRemovesTone = true;
        auto                owned                       = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host                        = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (char c : std::string("vieetj"))
            press(state, static_cast<uint32_t>(c));
        press(state, ngosen::key::BackSpace);
        check(host->preedit() == "viêt", "preedit: backspace on việt shows viêt", host->preedit());
        press(state, static_cast<uint32_t>('s'));
        check(host->preedit() == "viết", "preedit: a new tone mark goes on the rebuilt word", host->preedit());
    }

    // An address bar fills in a suggestion after the cursor and selects it. Typing its next letter
    // leaves the text as it was and moves the cursor one step, which is not a click.
    void testTypingIntoSuggestion() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true, "microsoft-edge"));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        const std::string   suggestion = "ngosen.github.io";
        host->setSurrounding("", 0);
        state.surroundingUpdated();
        for (unsigned int typed = 1; typed <= 3; ++typed) {
            press(state, static_cast<uint32_t>(suggestion[typed - 1]));
            // Later than the caret settling time, as Edge reports it.
            loop.pump(45);
            host->setSurrounding(suggestion, typed, static_cast<unsigned int>(suggestion.size()));
            state.surroundingUpdated();
        }
        check(press(state, static_cast<uint32_t>('s')), "suggestion: ngo then s makes ngó", "s went to the app as a plain key");
    }

    void testRecorderLimits() {
        ngosen::Recorder recorder(3);
        for (int i = 0; i < 5; ++i)
            recorder.add("key", std::to_string(i));
        const std::string log = recorder.dump();
        check(!has(log, "\t1\n") && has(log, "\t2\n") && has(log, "\t4\n"), "recorder: keeps only the last events", log);

        ngosen::Recorder fields(3);
        fields.add("field", "frontend=xim");
        for (int i = 0; i < 5; ++i)
            fields.add("key", std::to_string(i));
        const std::string kept = fields.dump();
        check(has(kept, "\tfield\tfrontend=xim\n") && has(kept, "\t4\n"), "recorder: keeps the field of the kept keys", kept);

        const std::string text = std::string(100, 'a') + "ê" + std::string(30, 'b');
        const std::string seen = ngosen::describeSurrounding(ngosen::Surrounding(text, 101, 101));
        const std::string want = "\"…" + std::string(39, 'a') + "ê|" + std::string(10, 'b') + "…\" cursor=101 anchor=101 length=131";
        check(seen == want, "recorder: cuts the surrounding text around the cursor", seen);
    }

    void testQuirks() {
        check(ngosen::forwardsBackspaces(makeField("wayland_v2", false)), "quirks: wayland_v2 forwards backspaces", "false");
        check(ngosen::forwardsBackspaces(makeField("xim", false)), "quirks: xim forwards backspaces", "false");
        check(!ngosen::forwardsBackspaces(makeField("ibus", false, "SDL_App")), "quirks: SDL gets no forwarded backspaces", "true");
        check(ngosen::ignoresForwardedKeys(makeField("dbus", true)), "quirks: dbus with surrounding text and no key order fix is GTK4", "false");
        Field gtk3            = makeField("dbus", true);
        gtk3.keyEventOrderFix = true;
        check(!ngosen::ignoresForwardedKeys(gtk3), "quirks: key order fix means GTK3", "true");
    }

} // namespace

int main() {
    testQuirks();
    testPreeditTelex();
    testAutoCapitalize();
    testSenForwards("wayland", true);
    testSenForwards("wayland_v2", true);
    testSenForwards("xim", false);
    testKeyDuringReplacement();
    testOneCommitAfterTimedWait();
    testCommitAfterTheReport();
    testCommitAfterFieldRefocus();
    testCommitAfterEditorRedraw();
    testQuickSwitchesNoRedrawWait();
    testMacroKeepsSpace();
    testBackspaceIntoLastWord("wayland", true);
    testBackspaceIntoLastWord("wayland", false);
    {
        // On an X11 session XIM clients get our backspaces as XTEST presses.
        setXTestSenderForTest([](int) { return true; });
        ngosen::test::AppQuirks xtest;
        xtest.takesXTest = true;
        testBackspaceIntoLastWord("xim", false, xtest);
        setXTestSenderForTest({});
    }
    testBackspaceAfterMovingAway();
    testBackspaceAfterCtrlBackspace();
    testResentBackspace();
    testGtk4DeletesSurrounding();
    testSdlGetsNoForwardedBackspace();
    testTerminalErasesWithCommittedDel();
    testRecorderKeepsTheReplacement();
    testRecorderNotesFieldSwitch();
    testRecorderHidesPasswords();
    testRecorderLimits();
    testFocusBounce();
    testTypingIntoSuggestion();
    testHomeDictionaryPath();
    testWithoutToneMark();
    testBackspaceRemovesTone(true);
    testBackspaceRemovesTone(false);
    testPreeditBackspaceRemovesTone();
    if (failures == 0)
        std::cout << "all core typing checks passed\n";
    return failures == 0 ? 0 : 1;
}
