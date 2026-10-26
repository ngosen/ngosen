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
#include <utility>
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

    // Types `before` and Enter in Preedit, then nextKey if set, or loses focus; counts the keys forwarded
    // right after Enter, right after nextKey and after the timers ran, and lists them as text.
    struct EnterResult {
        bool        enterAccepted = false, releaseAccepted = false;
        size_t      atEnter = 0, atNextKey = 0, atEnd = 0;
        std::string commits, forwarded;
    };

    EnterResult typeEnterInPreedit(const Field& field, const char* before, uint32_t nextKey = 0, uint32_t enterSym = ngosen::key::Return, bool focusOut = false) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        for (const char* c = before; *c != 0; ++c)
            press(state, static_cast<uint32_t>(*c));
        EnterResult r;
        FakeKey     down(enterSym);
        state.keyEvent(down);
        FakeKey up(enterSym, true);
        state.keyEvent(up);
        r.enterAccepted   = down.accepted();
        r.releaseAccepted = up.accepted();
        r.atEnter         = host->forwarded().size();
        if (nextKey != 0)
            press(state, nextKey);
        if (focusOut)
            state.reset(true);
        r.atNextKey = host->forwarded().size();
        loop.pump(100);
        r.atEnd   = host->forwarded().size();
        r.commits = join(host->commits());
        for (const auto& key : host->forwarded())
            r.forwarded += std::string(key.key == ngosen::EditKey::Return ? "Return" : "other") + (key.release ? " up;" : " down;");
        return r;
    }

    void testPreeditEnterAfterCommit() {
        const EnterResult held = typeEnterInPreedit(makeField("wayland", true), "tieen");
        check(held.commits == "['tiên']", "preedit Enter: commits the word", held.commits);
        check(held.enterAccepted && held.atEnter == 0, "preedit Enter: holds the Enter that commits the word", std::to_string(held.atEnter));
        check(held.forwarded == "Return down;Return up;", "preedit Enter: sends the held Enter after a moment", held.forwarded);
        check(held.releaseAccepted, "preedit Enter: keeps the release of the held Enter from the app", held.forwarded);

        const EnterResult keypad = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::KP_Enter);
        check(keypad.enterAccepted && keypad.forwarded == "Return down;Return up;", "preedit Enter: holds keypad Enter too", keypad.forwarded);

        const EnterResult away = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::Return, true);
        check(away.atEnd == 0, "preedit Enter: drops the held Enter on focus out", away.forwarded);

        const EnterResult fast = typeEnterInPreedit(makeField("wayland", true), "tieen", 'a');
        check(fast.atNextKey == 2 && fast.atEnd == 2, "preedit Enter: a key typed at once sends the held Enter first, once", fast.forwarded);

        const EnterResult empty = typeEnterInPreedit(makeField("wayland", true), "");
        check(!empty.enterAccepted && empty.atEnd == 0, "preedit Enter: with nothing to commit Enter goes on to the app", empty.forwarded);

        Field             gtk4           = makeField("dbus", true);
        const EnterResult dropsForwarded = typeEnterInPreedit(gtk4, "tieen");
        check(!dropsForwarded.enterAccepted && dropsForwarded.atEnd == 0, "preedit Enter: goes on at once where forwarded keys are dropped", dropsForwarded.forwarded);
    }

    // GTK apps reset the input context when Enter inserts a new line; resetAfter says after which key.
    std::string typeWithAutoCapitalize(const std::vector<uint32_t>& keys, size_t resetAfter = SIZE_MAX, bool focusOut = false, bool appCommits = false) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().autoCapitalizeAfterPunctuation = true;
        auto                owned                                 = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host                                  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        for (size_t i = 0; i < keys.size(); ++i) {
            press(state, keys[i]);
            if (i == resetAfter && appCommits)
                state.resetAfterAppCommit(focusOut);
            else if (i == resetAfter)
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
        const std::string appResets = typeWithAutoCapitalize(enterKeys, 3, false, true);
        check(appResets == "banTôi", "auto-capitalize: an app that commits by itself keeps the sentence start on reset", appResets);
        const std::string appLeaves = typeWithAutoCapitalize(enterKeys, 3, true, true);
        check(appLeaves == "bantôi", "auto-capitalize: an app that commits by itself forgets it on focus out", appLeaves);
    }

    // Types "viee" in Preedit, ends it the way named, then types "a"; returns what was committed and the preedit left.
    std::pair<std::string, std::string> endPreeditWord(bool appCommits, bool focusOut) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("ibus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        for (char c : std::string("viee"))
            press(state, static_cast<uint32_t>(c));
        if (appCommits)
            state.resetAfterAppCommit(focusOut);
        else
            state.reset(focusOut);
        const std::string commits = join(host->commits());
        press(state, 'a');
        return {commits, host->preedit()};
    }

    void testResetAfterAppCommit() {
        const auto ours = endPreeditWord(false, true);
        check(ours.first == "['viê']", "preedit focus out: commits the word", ours.first);
        const auto left = endPreeditWord(true, true);
        check(left.first == "(none)", "preedit focus out, app commits: nothing committed a second time", left.first);
        check(left.second == "a", "preedit focus out, app commits: the dropped word does not come back", left.second);
        const auto reset = endPreeditWord(true, false);
        check(reset.first == "(none)" && reset.second == "a", "preedit reset, app commits: the word is dropped", reset.first + " " + reset.second);
    }

    // Types in a field that already holds text before the cursor; capitalizeStartOfText is on unless off is set.
    std::string typeAtStart(Field field, const std::string& before, const std::string& keys, bool off = false, ngosen::Mode mode = ngosen::Mode::Preedit) {
        realMode = mode;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().capitalizeStartOfText = !off;
        auto                owned                        = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host                         = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        host->setSurrounding(before, static_cast<unsigned int>(ngosen::utf8::length(before)));
        for (const char c : keys)
            press(state, static_cast<uint32_t>(c));
        std::string text;
        for (const auto& commit : host->commits())
            text += commit;
        return text + host->preedit();
    }

    // Presses sym in a field holding before, after which the app reports report, or nothing when it is null.
    std::string typeAfterKey(const Field& field, const std::string& before, uint32_t sym, uint32_t states, const char* report) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().capitalizeStartOfText = true;
        auto                owned                        = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host                         = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        host->setSurrounding(before, static_cast<unsigned int>(ngosen::utf8::length(before)));
        FakeKey down(sym, false, states);
        state.keyEvent(down);
        FakeKey up(sym, true, states);
        state.keyEvent(up);
        if (report != nullptr) {
            host->setSurrounding(report, static_cast<unsigned int>(ngosen::utf8::length(report)));
            state.surroundingUpdated();
        }
        for (const char c : std::string("xin"))
            press(state, static_cast<uint32_t>(c));
        std::string text;
        for (const auto& commit : host->commits())
            text += commit;
        return text + host->preedit();
    }

    void testCapitalizeStartOfText() {
        const Field field = makeField("wayland", true);
        std::string text  = typeAtStart(field, "", "xin");
        check(text == "Xin", "start of text: the first letter in an empty field is upper case", text);
        text = typeAtStart(field, "abc\n", "xin");
        check(text == "Xin", "start of text: the first letter of a line is upper case", text);
        text = typeAtStart(field, "abc ", "xin");
        check(text == "xin", "start of text: a word after other text stays lower case", text);
        text = typeAtStart(field, "", "ddi");
        check(text == "Đi", "start of text: dd at the start gives Đ", text);
        text = typeAtStart(field, "", "awn");
        check(text == "Ăn", "start of text: aw at the start gives Ă", text);
        text = typeAtStart(field, "", "xin", true);
        check(text == "xin", "start of text: nothing changes with the option off", text);
        FakeResources resources;
        resources.mutableOptions().capitalizeStartOfText = true;
        const std::vector<uint32_t> keys{'x', 'i', 'n', ' ', 'c', 'h', 'a', 'o', 'f'};
        text = ngosen::test::typeInApp(resources, field, {}, keys, std::vector<int>(keys.size(), 20));
        check(text == "Xin chào", "start of text: only the first word is upper case in Sen mode", text);

        Field url = field;
        url.url   = true;
        text      = typeAtStart(url, "", "xin");
        check(text == "xin", "start of text: an address bar stays lower case", text);
        text = typeAtStart(makeField("wayland", false), "", "xin");
        check(text == "xin", "start of text: a field that does not report its text stays lower case", text);
        const Field ibus = makeField("ibus", true);
        text             = typeAtStart(ibus, "", "xin");
        check(text == "Xin", "start of text: an empty field over IBus (GNOME) is upper case", text);
        text = typeAfterKey(ibus, "abc", ngosen::key::Return, 0, "abc\n");
        check(text == "Xin", "start of text: a new line over IBus is upper case once GNOME reports it", text);
        text = typeAfterKey(ibus, "", 'v', ngosen::modifier::Ctrl, nullptr);
        check(text == "xin", "start of text: a paste GNOME has not reported yet stays lower case", text);

        const Field wayland = makeField("wayland", true);
        text                = typeAfterKey(wayland, "", 'v', ngosen::modifier::Ctrl, nullptr);
        check(text == "xin", "start of text: a paste the app has not reported yet stays lower case", text);
        text = typeAfterKey(wayland, "", 'v', ngosen::modifier::Ctrl, "");
        check(text == "xin", "start of text: a report still showing the field before the paste is not trusted", text);
        text = typeAfterKey(wayland, "", 'v', ngosen::modifier::Ctrl, "abc\n");
        check(text == "Xin", "start of text: once the app reports the paste, a new line is upper case", text);
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

    // Firefox over D-Bus applies a key it sent before our commit reached it ahead of that commit.
    void testKeyTypedBeforeCommitArrived() {
        ngosen::test::AppQuirks quirks;
        quirks.commitLagMs               = 5;
        const std::vector<uint32_t> keys = {'t', 'i', 'e', 'e', 'n', 'g', 's', ' ', 'v', 'i'};
        std::vector<int>            gaps(keys.size(), 30);
        gaps[6]                = 2;
        Field gtk3             = makeField("dbus", true);
        gtk3.keyEventOrderFix  = true;
        const std::string text = ngosen::test::typeInApp(gtk3, quirks, keys, gaps);
        check(text == "tiếng vi", "dbus: a key typed before the commit arrived lands after it", text);
        const std::vector<uint32_t> back = {'t', 'i', 'e', 'e', 'n', 'g', 's', ngosen::key::BackSpace};
        const std::string           cut  = ngosen::test::typeInApp(gtk3, quirks, back, std::vector<int>{30, 30, 30, 30, 30, 30, 2, 30});
        check(cut.find('\b') == std::string::npos, "dbus: a backspace typed before the commit arrived is not committed", cut);
    }

    void testMacroKeepsSpace() {
        FakeResources resources;
        resources.setMacros({{"e", "em"}, {"ko", "không"}});
        const std::vector<uint32_t> keys{'e', ' ', 'k', 'o', ' ', 'a'};
        const std::string           text = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
        check(text == "em không a", "macro keeps its space", text);
    }

    // With Capitalize Macro on, an abbreviation typed with a capital first letter capitalizes the
    // text, unless the text has capitals of its own.
    void testMacroCapitalized() {
        const std::vector<std::pair<std::string, std::string>> cases{
            {"Ko", "Không "}, {"kO", "không "}, {"Ip", "iPhone "}, {"Io", "iOS "}, {"Bt", "ßa "},
        };
        for (const auto& [typed, want] : cases) {
            FakeResources resources;
            resources.setMacros({{"ko", "không"}, {"ip", "iPhone"}, {"io", "iOS"}, {"bt", "ßa"}});
            std::vector<uint32_t> keys(typed.begin(), typed.end());
            keys.push_back(' ');
            const std::string text = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
            check(text == want, "capitalized macro: " + typed, text);
        }
    }

    // An abbreviation matches in any case it was written or typed in.
    void testMacroKeyCase() {
        const std::vector<std::pair<std::string, std::string>> cases{
            {"ko", "không "}, {"KO", "KHÔNG "}, {"vn", "việt nam "}, {"Vn", "Việt Nam "}, {"VN", "VIỆT NAM "},
        };
        for (const auto& [typed, want] : cases) {
            FakeResources resources;
            resources.setMacros({{"ko", "không"}, {"VN", "Việt Nam"}});
            std::vector<uint32_t> keys(typed.begin(), typed.end());
            keys.push_back(' ');
            const std::string text = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
            check(text == want, "macro key case: " + typed, text);
        }
        // Of several capitalized spellings, both cores take the first one saved.
        const std::vector<std::pair<std::string, std::string>> spellings{{"KO", "kansas"}, {"Ko", "korea"}, {"kO", "kosovo"}};
        for (size_t first = 0; first < spellings.size(); ++first) {
            std::vector<std::pair<std::string, std::string>> macros;
            for (size_t i = 0; i < spellings.size(); ++i)
                macros.push_back(spellings[(first + i) % spellings.size()]);
            FakeResources resources;
            resources.setMacros(macros);
            const std::vector<uint32_t> keys{'k', 'o', ' '};
            const std::string           text  = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
            const std::string&          value = spellings[first].second;
            check(text == value + " ", "first macro spelling wins: " + spellings[first].first, text);
        }
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

    // What we committed may differ from what the engine can take back: a macro's text ending in a
    // symbol, or a word in a legacy charset.
    void testBackspaceIntoCommittedText() {
        const Field    field = makeField("wayland", true);
        const uint32_t bs    = ngosen::key::BackSpace;
        const auto     type  = [&](FakeResources& resources, const std::string& keys) {
            std::vector<uint32_t> syms;
            for (const char c : keys)
                syms.push_back(c == '<' ? bs : static_cast<uint32_t>(c));
            return ngosen::test::typeInApp(resources, field, {}, syms, std::vector<int>(syms.size(), 100));
        };
        FakeResources macros;
        macros.setMacros({{"q", "“quá”"}, {"vv", "vân vân…"}});
        const std::string quote = type(macros, "q <s ");
        check(quote == "“quá”s ", "backspace into a macro ending in a quote", quote);
        const std::string ellipsis = type(macros, "vv <s ");
        check(ellipsis == "vân vân…s ", "backspace into a macro ending in an ellipsis", ellipsis);

        // VNI Windows writes "đươ" as "ñöô"; the word is not reopened, so j starts a new one.
        FakeResources vni;
        vni.mutableOptions().outputCharset = "VNI Windows";
        const std::string legacy           = type(vni, "dduowc <<j ");
        check(legacy == "ñöôj ", "backspace into a VNI Windows word", legacy);
        // A word of plain letters is the same in any charset, so it is still reopened.
        const std::string plain = type(vni, "ban <f ");
        check(plain == "baøn ", "backspace into a plain word in VNI Windows", plain);

        // VIQR writes tone marks as ASCII symbols, so "ba`n" is not reopened either.
        FakeResources viqr;
        viqr.mutableOptions().outputCharset = "VIQR";
        const std::string marked            = type(viqr, "banf <s ");
        check(marked == "ba`ns ", "backspace into a VIQR word", marked);
    }

    // Firefox contenteditable keeps a trailing space as U+00A0 in the text it reports.
    void testBackspaceIntoWordBeforeNbsp() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("dbus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (const char c : std::string("ban"))
            press(state, static_cast<uint32_t>(c));
        press(state, static_cast<uint32_t>(' '));
        host->setSurrounding("ban\u00a0", 4);
        state.surroundingUpdated();
        press(state, ngosen::key::BackSpace);
        host->setSurrounding("ban", 3);
        state.surroundingUpdated();
        check(press(state, static_cast<uint32_t>('f')), "backspace into word before nbsp: f marks the word", "f went to the app");
    }

    // xterm.js keeps only the last key typed in its hidden textarea, so the field never shows the word.
    void testBackspaceIntoWordFieldDropped() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("dbus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        for (const char c : std::string("ban ")) {
            press(state, static_cast<uint32_t>(c));
            host->setSurrounding(std::string(1, c), 1);
            state.surroundingUpdated();
        }
        press(state, ngosen::key::BackSpace);
        host->setSurrounding("", 0);
        state.surroundingUpdated();
        check(press(state, static_cast<uint32_t>('f')), "backspace into word the field dropped: f marks the word", "f went to the app");
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
        // Flatpak Firefox: the log shows the app ID it reported too.
        Field firefox           = makeField("wayland", true, "firefox");
        firefox.reportedProgram = "org.mozilla.firefox";

        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, firefox);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        press(state, static_cast<uint32_t>('e'));
        host->setSurrounding("ti", 2);
        state.surroundingUpdated();
        loop.pump(150);

        const std::string log = resources.recorder().dump();
        check(has(log, "\tfield\tfrontend=wayland program=firefox (org.mozilla.firefox) surrounding=1 preedit=1 mode=Sen\n"), "recorder: notes the field and mode", log);
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

    // Types `keys`, a word and a macro, keeping every log line; returns them.
    std::string logWhileTyping(ngosen::Mode mode, bool password, const std::string& keys) {
        realMode = mode;
        FakeLoop      loop;
        FakeResources resources;
        resources.setMacros({{"ko", "không"}});
        Field field               = makeField("wayland", true, "firefox");
        field.password            = password;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        host->setSurrounding("", 0);
        ngosen::test::captureLog(true);
        for (char c : keys) {
            press(state, static_cast<uint32_t>(c));
            loop.pump(20);
        }
        state.reset(true);
        loop.pump(150);
        std::string log;
        for (const auto& line : ngosen::test::capturedLog())
            log += line + "\n";
        ngosen::test::captureLog(false);
        return log;
    }

    void testLogHidesPasswords() {
        for (const auto mode : {ngosen::Mode::Preedit, ngosen::Mode::Sen}) {
            const std::string name  = mode == ngosen::Mode::Sen ? "Sen" : "Preedit";
            const std::string plain = logWhileTyping(mode, false, "vieetj ko ");
            // The fake field reports no text, so only Preedit gets as far as the macro.
            check(has(plain, "ệ") && (mode == ngosen::Mode::Sen || has(plain, "không")), "log: " + name + " shows typed text outside a password field", plain);
            const std::string hidden = logWhileTyping(mode, true, "vieetj ko ");
            check(!has(hidden, "ê") && !has(hidden, "ệ") && !has(hidden, "vi") && !has(hidden, "không"), "log: " + name + " keeps no password text", hidden);
        }
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

    // An address bar fills in a suggestion after the cursor. Typing its next letter leaves the text
    // as it was and moves the cursor one step, which is not a click. Edge reports the suggestion
    // selected; Firefox reports no selection.
    void testTypingIntoSuggestion() {
        for (const bool selected : {true, false}) {
            realMode = ngosen::Mode::Sen;
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true, selected ? "microsoft-edge" : "firefox"));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));

            const std::string   suggestion = "ngosen.github.io";
            const std::string   what       = selected ? "selected suggestion: " : "unselected suggestion: ";
            host->setSurrounding("", 0);
            state.surroundingUpdated();
            for (unsigned int typed = 1; typed <= 3; ++typed) {
                press(state, static_cast<uint32_t>(suggestion[typed - 1]));
                // Later than the caret settling time, as both browsers report it.
                loop.pump(45);
                host->setSurrounding(suggestion, typed, selected ? static_cast<unsigned int>(suggestion.size()) : typed);
                state.surroundingUpdated();
            }
            check(press(state, static_cast<uint32_t>('s')), what + "s is taken as a tone key", "s went to the app as a plain key");
            loop.pump(200);
            const std::string last = host->commits().empty() ? std::string() : host->commits().back();
            check(last == "ó", what + "ngo then s makes ngó", last);
        }
    }

    // Without a selection, a click one letter to the right looks like a step into a suggestion,
    // except that it comes long after the key.
    void testClickAfterTypingIsNotSuggestion() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true, "firefox"));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        host->setSurrounding("on", 0);
        state.surroundingUpdated();
        press(state, static_cast<uint32_t>('c'));
        loop.pump(45);
        host->setSurrounding("con", 1);
        state.surroundingUpdated();
        press(state, static_cast<uint32_t>('o'));
        loop.pump(45);
        host->setSurrounding("coon", 2);
        state.surroundingUpdated();
        loop.pump(300);
        host->setSurrounding("coon", 3);
        state.surroundingUpdated();
        check(!press(state, static_cast<uint32_t>('s')), "click one letter on: s starts a new word", "s was taken as a tone key for co");
    }

    // Browsers repeat the same report several times per key; repeats would push the keys out of the log.
    void testRecorderSkipsRepeatedReports() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true, "firefox"));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        state.surroundingUpdated();
        state.surroundingUpdated();
        const auto count = [&](const std::string& s) {
            const std::string log = resources.recorder().dump();
            size_t            n   = 0;
            for (size_t at = log.find(s); at != std::string::npos; at = log.find(s, at + 1))
                ++n;
            return n;
        };
        const std::string tie = "\tapp\t\"tie|\" cursor=3 anchor=3 length=3\n";
        check(count(tie) == 1, "recorder: drops a report that repeats the one just before", resources.recorder().dump());

        const size_t before = count(tie);
        press(state, static_cast<uint32_t>('e'));
        state.surroundingUpdated();
        state.surroundingUpdated();
        check(count(tie) == before + 2, "recorder: keeps a repeat that a wait acts on", resources.recorder().dump());
        host->setSurrounding("ti", 2);
        state.surroundingUpdated();
        loop.pump(150);
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

    // Tapping a modifier other than the lock key mid-word, as when reaching for a shortcut and letting
    // go, keeps the word open so a later tone key still applies to it.
    void testModifierTapKeepsWord() {
        for (const auto& [frontend, surroundingText] : {std::pair<const char*, bool>{"wayland", true}, {"ibus", false}}) {
            for (const uint32_t modifier : {ngosen::key::Shift_L, ngosen::key::Control_L}) {
                FakeResources resources;
                if (modifier == ngosen::key::Control_L)
                    resources.mutableOptions().lockWordKey = ngosen::LockWordKey::None;
                const std::vector<uint32_t> keys{'v', 'i', 'e', 'e', 't', modifier, 'j'};
                const std::string           text = ngosen::test::typeInApp(resources, makeField(frontend, surroundingText), {}, keys, std::vector<int>(keys.size(), 100));
                check(text == "việt", std::string("modifier tap: ") + frontend + (modifier == ngosen::key::Shift_L ? " Shift" : " Ctrl") + " keeps vieet open", text);
            }
        }
    }

    // ^ is a Ctrl tap, + a Shift tap and < a backspace.
    void expectTyped(FakeResources& resources, const Field& field, const std::string& typed, const std::string& want, const std::string& what, ngosen::test::AppQuirks quirks = {},
                     int gapMs = 30) {
        std::vector<uint32_t> keys;
        for (char c : typed)
            keys.push_back(c == '^' ? ngosen::key::Control_L : c == '+' ? ngosen::key::Shift_L : c == '<' ? ngosen::key::BackSpace : static_cast<uint32_t>(c));
        const std::string text = ngosen::test::typeInApp(resources, field, quirks, keys, std::vector<int>(keys.size(), gapMs));
        check(text == want, what, text);
    }

    // Tapping the lock key keeps the word as shown, like Ctrl in UniKey: no more tone marks, no
    // restore of an invalid word, no macro (#218).
    void testLockWord() {
        for (const auto& [frontend, surroundingText] : {std::pair<const char*, bool>{"wayland", true}, {"ibus", false}}) {
            const Field   field = makeField(frontend, surroundingText);
            const auto    where = std::string(" (") + frontend + ")";
            FakeResources resources;
            resources.setMacros({{"ko", "không"}});
            expectTyped(resources, field, "tunx ko ", "tunx không ", "lock: tunx is restored and ko expands without the key" + where);
            // tũn is kept, s after the tap is a letter, ko stays, and the word after a locked one is restored again.
            expectTyped(resources, field, "tunx^ a^sk ko^ tunx ", "tũn ask ko tunx ", "lock: Ctrl keeps the word as shown" + where);
        }
        FakeResources shift;
        shift.mutableOptions().lockWordKey = ngosen::LockWordKey::Shift;
        expectTyped(shift, makeField("wayland", true), "tunx+ ", "tũn ", "lock: Shift when chosen");
        FakeResources off;
        off.mutableOptions().lockWordKey = ngosen::LockWordKey::None;
        expectTyped(off, makeField("wayland", true), "tunx^ ", "tunx ", "lock: Ctrl does nothing when disabled");
        // A tap right after a tone key lands while that replacement is still in flight.
        ngosen::test::AppQuirks slow;
        slow.lagMs = 40;
        FakeResources lagging;
        // s finishes the replacement on arrival and must not join the word locked before it.
        expectTyped(lagging, makeField("wayland", true), "tunx^s ", "tũns ", "lock: Ctrl during a replacement", slow, 10);
        // A tap between words does not close the last one to Backspace.
        FakeResources plain;
        expectTyped(plain, makeField("wayland", true), "ban ^<f ", "bàn ", "lock: Ctrl after a word");
    }

    // Letting go of Ctrl before Shift in a Shift+Ctrl chord is not a tap.
    void testLockWordNotOnChord() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, makeField("wayland", true), ngosen::test::AppQuirks{});
        auto*               app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (char c : std::string("vieet"))
            app->type(static_cast<uint32_t>(c));
        loop.pump(30);
        const auto modifierKey = [&](uint32_t sym, bool release, uint32_t states) {
            FakeKey key(sym, release, states);
            state.keyEvent(key);
        };
        modifierKey(ngosen::key::Shift_L, false, 0);
        modifierKey(ngosen::key::Control_L, false, ngosen::modifier::Shift);
        modifierKey(ngosen::key::Control_L, true, ngosen::modifier::Shift | ngosen::modifier::Ctrl);
        modifierKey(ngosen::key::Shift_L, true, ngosen::modifier::Shift);
        app->type('j');
        loop.pump(300);
        check(app->text() == "việt", "lock: Shift+Ctrl chord keeps the word open", app->text());
    }

    // The engine already commits the preedit on Ctrl or Alt; a bare Shift never reaches it.
    void testLockWordPreedit() {
        for (const auto [lockKey, modifier] : {std::pair{ngosen::LockWordKey::Ctrl, ngosen::key::Control_L}, {ngosen::LockWordKey::Shift, ngosen::key::Shift_L}}) {
            realMode = ngosen::Mode::Preedit;
            FakeLoop      loop;
            FakeResources resources;
            resources.mutableOptions().lockWordKey = lockKey;
            auto                owned              = std::make_unique<FakeHost>(loop, makeField("wayland", true));
            FakeHost*           host               = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));

            for (char c : std::string("tunx"))
                press(state, static_cast<uint32_t>(c));
            press(state, modifier);
            press(state, ngosen::key::space);
            const std::string name = modifier == ngosen::key::Shift_L ? "Shift" : "Ctrl";
            check(join(host->commits()) == "['tũn'][' ']", "lock preedit: " + name + " commits tũn as shown", join(host->commits()));
        }
    }

    // GLFW (Minecraft) under XIM keeps the time of the last Backspace press and drops older ones.
    void testForwardedKeysNewerThanTyped() {
        ngosen::test::AppQuirks glfw;
        glfw.dropsOldKeyPresses          = true;
        const std::vector<uint32_t> keys = {ngosen::key::BackSpace, 't', 'i', 'e', 'e', 'n', 'g', 's'};
        const std::string           text = ngosen::test::typeInApp(makeField("xim", false, "glfw"), glfw, keys, std::vector<int>(keys.size(), 20));
        check(text == "tiếng", "glfw: forwarded backspaces are newer than a typed one", text);
    }

} // namespace

int main() {
    testQuirks();
    testPreeditTelex();
    testPreeditEnterAfterCommit();
    testAutoCapitalize();
    testResetAfterAppCommit();
    testLogHidesPasswords();
    testCapitalizeStartOfText();
    testSenForwards("wayland", true);
    testSenForwards("wayland_v2", true);
    testSenForwards("xim", false);
    testForwardedKeysNewerThanTyped();
    testKeyDuringReplacement();
    testCommitAfterTheReport();
    testCommitAfterFieldRefocus();
    testKeyTypedBeforeCommitArrived();
    testCommitAfterEditorRedraw();
    testQuickSwitchesNoRedrawWait();
    testMacroKeepsSpace();
    testLockWord();
    testLockWordPreedit();
    testLockWordNotOnChord();
    testMacroCapitalized();
    testMacroKeyCase();
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
    testBackspaceIntoCommittedText();
    testBackspaceIntoWordBeforeNbsp();
    testBackspaceIntoWordFieldDropped();
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
    testRecorderSkipsRepeatedReports();
    testFocusBounce();
    testTypingIntoSuggestion();
    testClickAfterTypingIsNotSuggestion();
    testHomeDictionaryPath();
    testWithoutToneMark();
    testBackspaceRemovesTone(true);
    testBackspaceRemovesTone(false);
    testPreeditBackspaceRemovesTone();
    testModifierTapKeepsWord();
    if (failures == 0)
        std::cout << "all core typing checks passed\n";
    return failures == 0 ? 0 : 1;
}
