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

#include <algorithm>
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

    // Types `before` and Enter in Preedit, holding Enter down for holdMs, then nextKey if set, or loses focus;
    // counts the keys forwarded before the release, right after it, right after nextKey and after the timers ran.
    struct EnterResult {
        bool        enterAccepted = false, releaseAccepted = false;
        size_t      atHold = 0, atEnter = 0, atNextKey = 0, atEnd = 0;
        std::string commits, forwarded;
    };

    EnterResult typeEnterInPreedit(const Field& field, const char* before, uint32_t nextKey = 0, uint32_t enterSym = ngosen::key::Return, bool focusOut = false,
                                   bool reports = false, int holdMs = 0) {
        realMode = ngosen::Mode::Preedit;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        if (reports) {
            host->setSurrounding("", 0);
            state.surroundingUpdated();
        }
        for (const char* c = before; *c != 0; ++c)
            press(state, static_cast<uint32_t>(*c));
        EnterResult r;
        FakeKey     down(enterSym);
        state.keyEvent(down);
        loop.pump(holdMs);
        r.atHold = host->forwarded().size();
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

        const EnterResult slow = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::Return, false, false, 120);
        check(slow.atHold == 0 && slow.atEnter == 2 && slow.releaseAccepted, "preedit Enter: waits for a held-down Enter to be released",
              std::to_string(slow.atHold) + " " + slow.forwarded);
        const EnterResult noRelease = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::Return, false, false, 600);
        check(noRelease.atHold == 2 && noRelease.atEnd == 2, "preedit Enter: sends the held Enter by the time key repeat starts", noRelease.forwarded);

        const EnterResult keypad = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::KP_Enter);
        check(keypad.enterAccepted && keypad.forwarded == "Return down;Return up;", "preedit Enter: holds keypad Enter too", keypad.forwarded);

        const EnterResult away = typeEnterInPreedit(makeField("wayland", true), "tieen", 0, ngosen::key::Return, true);
        check(away.atEnd == 0, "preedit Enter: drops the held Enter on focus out", away.forwarded);

        const EnterResult fast = typeEnterInPreedit(makeField("wayland", true), "tieen", 'a');
        check(fast.atNextKey == 2 && fast.atEnd == 2, "preedit Enter: a key typed at once sends the held Enter first, once", fast.forwarded);

        const EnterResult empty = typeEnterInPreedit(makeField("wayland", true), "");
        check(!empty.enterAccepted && empty.atEnd == 0, "preedit Enter: with nothing to commit Enter goes on to the app", empty.forwarded);

        Field             gtk4           = makeField("dbus", true);
        const EnterResult dropsForwarded = typeEnterInPreedit(gtk4, "tieen", 0, ngosen::key::Return, false, true);
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

        // A field that has not reported its word yet keeps the backspace path.
        typeTie(state, *host, loop, false);
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

    // Once the field has reported the word, KWin, Sway and D-Bus fields delete through the surrounding text.
    void testSenDeletesReportedWord(const std::string& frontend) {
        realMode = ngosen::Mode::Sen;
        FakeLoop      loop;
        FakeResources resources;
        Field         field = makeField(frontend, true);
        // fcitx5-gtk3 and fcitx5-qt; without the flag a D-Bus field is GTK4, which only takes deletions.
        field.keyEventOrderFix    = frontend == "dbus";
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        typeTie(state, *host, loop, true);
        const std::string what = "sen " + frontend + ", reported word: ";
        check(press(state, static_cast<uint32_t>('e')), what + "e -> ê takes the key", "not accepted");
        loop.pump(3);
        check(host->forwarded().empty() && host->deletes().size() == 1 && join(host->commits()) == "['ê']", what + "one surrounding deletion, then ê, no backspace",
              std::to_string(host->forwarded().size()) + " forwarded, " + std::to_string(host->deletes().size()) + " deletes, " + join(host->commits()));
    }

    // Firefox may report a surrounding edit only after the next key, so its fields keep backspaces.
    void testFirefoxDeletesThroughSurrounding() {
        for (const char* frontend : {"wayland", "wayland_v2", "dbus"}) {
            realMode = ngosen::Mode::Sen;
            FakeLoop      loop;
            FakeResources resources;
            Field         field       = makeField(frontend, true, "firefox");
            field.keyEventOrderFix    = std::string(frontend) == "dbus";
            auto                owned = std::make_unique<FakeHost>(loop, field);
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            typeTie(state, *host, loop, true);
            press(state, static_cast<uint32_t>('e'));
            loop.pump(3);
            check(host->deletes().size() == 1 && host->forwarded().empty(), std::string("firefox ") + frontend + ": surrounding deletion, no backspaces",
                  std::to_string(host->deletes().size()) + " deletes, " + std::to_string(host->forwarded().size()) + " forwarded");
        }
    }

    // Sway (wlroots) reports the text and then a done event; a commit sent before the done carries a
    // stale serial and is dropped.
    void testCommitAfterTheReport() {
        Field                   field = makeField("wayland_v2", true);
        ngosen::test::AppQuirks quirks;
        quirks.dropsCommitsInReport = true;
        const std::vector<uint32_t> keys{'d', 'd', 'w', 'o', 'w', 'n', 'g', 'f'};
        FakeResources               resources;
        const auto                  run = ngosen::test::typeInAppTraced(resources, field, quirks, keys, std::vector<int>(keys.size(), 100));
        check(run.text == "đường", "sway: commits go out after the app's report", run.text);
        // The first replacement empties the field, so it alone goes through backspaces.
        check(run.sent.surroundingDeletes > 0 && run.sent.forwardedBackSpaces == 1, "sway: the later replacements delete through the surrounding text",
              std::to_string(run.sent.surroundingDeletes) + " deletes, " + std::to_string(run.sent.forwardedBackSpaces) + " backspaces");
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
        gaps[6]               = 2;
        Field gtk3            = makeField("dbus", true);
        gtk3.keyEventOrderFix = true;
        FakeResources     resources;
        const auto        run  = ngosen::test::typeInAppTraced(resources, gtk3, quirks, keys, gaps);
        const std::string text = run.text;
        check(text == "tiếng vi", "dbus: a key typed before the commit arrived lands after it", text);
        check(run.sent.surroundingDeletes > 0 && run.sent.forwardedBackSpaces == 0, "dbus: those replacements delete through the surrounding text",
              std::to_string(run.sent.surroundingDeletes) + " deletes, " + std::to_string(run.sent.forwardedBackSpaces) + " backspaces");
        const std::vector<uint32_t> back = {'t', 'i', 'e', 'e', 'n', 'g', 's', ngosen::key::BackSpace};
        const std::string           cut  = ngosen::test::typeInApp(gtk3, quirks, back, std::vector<int>{30, 30, 30, 30, 30, 30, 2, 30});
        check(cut.find('\b') == std::string::npos, "dbus: a backspace typed before the commit arrived is not committed", cut);
    }

    // A field behind GNOME Shell's IBus bridge that reports no text waits on a timer after each
    // replacement, and a key let through after the commit can reach the app ahead of it (#569).
    void testGnomeKeyDuringTimedWaitJoinsCommit() {
        ngosen::test::AppQuirks quirks;
        quirks.reportsEmptyText = true;
        quirks.commitLagMs      = 5;
        std::vector<uint32_t> keys;
        std::vector<int>      gaps;
        // The last letter of each word comes 2 ms after the key that adds the mark.
        for (const std::string word : {"looxi ", "ddi ", "looxi ", "ddi ", "looxi "}) {
            for (size_t i = 0; i < word.size(); ++i) {
                keys.push_back(static_cast<uint32_t>(word[i]));
                gaps.push_back(i + 3 == word.size() ? 2 : 100);
            }
        }
        const std::string text = ngosen::test::typeInApp(makeField("ibus", true), quirks, keys, gaps);
        check(text == "lỗi đi lỗi đi lỗi ", "GNOME ibus: a key typed during a timed wait lands after the replacement", text);
    }

    // Types `text` with `fastKey` arriving 2 ms after the previous key, inside the wait for the commit.
    std::string typeWithHeldKey(FakeResources& resources, const std::string& text, size_t fastKey) {
        ngosen::test::AppQuirks quirks;
        quirks.reportsEmptyText = true;
        quirks.commitLagMs      = 5;
        std::vector<uint32_t> keys;
        std::vector<int>      gaps;
        for (size_t i = 0; i < text.size(); ++i) {
            keys.push_back(text[i] == '\b' ? 0xff08U : static_cast<uint32_t>(text[i]));
            gaps.push_back(i + 1 == fastKey ? 2 : 100);
        }
        return ngosen::test::typeInApp(resources, makeField("ibus", true), quirks, keys, gaps);
    }

    void testHeldSpaceCountsForDoubleSpace() {
        FakeResources resources;
        resources.mutableOptions().doubleSpaceToPeriod = true;
        const std::string text                         = typeWithHeldKey(resources, "chaof  ", 5);
        check(text == "chào. ", "ibus: a space held during a timed wait still counts for double space", text);
    }

    void testHeldSpaceReopensWord() {
        FakeResources     resources;
        const std::string text = typeWithHeldKey(resources, "vieetj \bs", 6);
        check(text == "viết", "ibus: backspace after a held space reopens the word", text);
    }

    // `keys` typed in a GNOME field that reports no text, `gaps[i]` ms after the key before.
    std::string typeInSilentIbusField(const std::vector<uint32_t>& keys, const std::vector<int>& gaps) {
        ngosen::test::AppQuirks quirks;
        quirks.reportsEmptyText = true;
        quirks.commitLagMs      = 5;
        quirks.showsEnter       = true;
        return ngosen::test::typeInApp(makeField("ibus", true), quirks, keys, gaps);
    }

    void testEnterDuringTimedWaitFollowsCommit() {
        const std::string text = typeInSilentIbusField({'l', 'o', 'o', 'x', 0xff0d, 'b'}, {100, 100, 100, 2, 100, 100});
        check(text == "lỗ\nb", "ibus: an Enter typed during a timed wait goes after the commit", text);
    }

    void testBackspaceDuringTimedWaitFollowsCommit() {
        const std::string once = typeInSilentIbusField({'l', 'o', 'o', 'x', 0xff08, 'a'}, {100, 100, 100, 2, 100, 100});
        check(once == "la", "ibus: a backspace typed during a timed wait deletes the new letter", once);
        const std::string twice = typeInSilentIbusField({'l', 'o', 'o', 'x', 0xff08, 0xff08, 'a'}, {100, 100, 100, 2, 30, 100, 100});
        check(twice == "a", "ibus: two quick backspaces after a timed wait delete the word", twice);
    }

    void testMacroKeepsSpace() {
        FakeResources resources;
        resources.setMacros({{"e", "em"}, {"ko", "không"}});
        const std::vector<uint32_t> keys{'e', ' ', 'k', 'o', ' ', 'a'};
        const std::string           text = ngosen::test::typeInApp(resources, makeField("wayland", true), {}, keys, std::vector<int>(keys.size(), 100));
        check(text == "em không a", "macro keeps its space", text);
    }

    // A macro typed right before Enter expands, and the Enter reaches the app after the expansion.
    void testMacroBeforeEnter() {
        struct Case {
            const char* name;
            Field       field;
            int         lagMs;
        };
        const Case cases[] = {
            {"sen, field reports text", makeField("wayland", true), 0},
            {"sen, field reports nothing", makeField("wayland", false), 0},
            {"sen, slow field", makeField("wayland", true), 5},
        };
        for (const auto& c : cases) {
            FakeResources resources;
            resources.setMacros({{"dc", "được"}});
            ngosen::test::AppQuirks quirks;
            quirks.showsEnter = true;
            quirks.lagMs      = c.lagMs;
            const std::vector<uint32_t> keys{'d', 'c', ngosen::key::Return, 'a'};
            const std::string           text = ngosen::test::typeInApp(resources, c.field, quirks, keys, std::vector<int>(keys.size(), 100));
            check(text == "được\na", std::string("macro before Enter: ") + c.name, text);
        }
        FakeResources resources;
        resources.setMacros({{"dc", "được"}});
        ngosen::test::AppQuirks quirks;
        quirks.showsEnter = true;
        const std::vector<uint32_t> word{'t', 'i', 'e', 'e', 'n', 'g', 's', ngosen::key::Return};
        const std::string           text = ngosen::test::typeInApp(resources, makeField("wayland", true), quirks, word, std::vector<int>(word.size(), 100));
        check(text == "tiếng\n", "macro before Enter: a plain word stays", text);

        Field shell       = makeField("ibus", true);
        shell.sendsNoKeys = true;
        FakeResources shellResources;
        shellResources.setMacros({{"dc", "được"}});
        ngosen::test::AppQuirks lossy;
        lossy.showsEnter         = true;
        lossy.dropsForwardedKeys = true;
        const std::vector<uint32_t> keys{'d', 'c', ngosen::key::Return, 'a'};
        const std::string           kept = ngosen::test::typeInApp(shellResources, shell, lossy, keys, std::vector<int>(keys.size(), 100));
        check(kept == "được\na", "macro before Enter: a field that loses sent keys still gets its Enter", kept);

        FakeResources stuckResources;
        stuckResources.setMacros({{"dc", "được"}});
        ngosen::test::AppQuirks shows;
        shows.showsEnter               = true;
        const std::string cannotDelete = ngosen::test::typeInApp(stuckResources, makeField("dbus", false), shows, keys, std::vector<int>(keys.size(), 100));
        check(cannotDelete == "dc\na", "macro before Enter: a field nothing can delete in keeps the word and its Enter", cannotDelete);

        realMode = ngosen::Mode::Preedit;
        FakeLoop      loop;
        FakeResources preeditResources;
        preeditResources.setMacros({{"dc", "được"}});
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&preeditResources, std::move(owned));
        press(state, 'd');
        press(state, 'c');
        press(state, ngosen::key::Return);
        loop.pump(100);
        std::string forwarded;
        for (const auto& key : host->forwarded())
            forwarded += std::string(key.key == ngosen::EditKey::Return ? "Return" : "other") + (key.release ? " up;" : " down;");
        check(join(host->commits()) == "['được']", "macro before Enter: preedit commits the expansion", join(host->commits()));
        check(forwarded == "Return down;Return up;", "macro before Enter: preedit sends the Enter after it", forwarded);
    }

    // An Enter typed while a replacement waits for the app's report reaches the app as a key, not as
    // a carriage return inside the commit, which Google Docs ignores.
    void testEnterDuringReplacement() {
        ngosen::test::AppQuirks quirks;
        quirks.showsEnter    = true;
        quirks.reportDelayMs = 10;
        const std::vector<uint32_t> keys{'n', 'h', 'e', 's', ngosen::key::Return, 'a'};
        std::vector<int>            gaps(keys.size(), 100);
        gaps[3]                = 2;
        const std::string text = ngosen::test::typeInApp(makeField("wayland", true, "libreoffice-calc"), quirks, keys, gaps);
        check(text == "nhé\na", "Enter during replacement", text);

        std::vector<int> keysAfterGaps(keys.size(), 1);
        keysAfterGaps[2]         = 100;
        const std::string queued = ngosen::test::typeInApp(makeField("wayland", true, "libreoffice-calc"), quirks, keys, keysAfterGaps);
        check(queued == "nhé\na", "Enter during replacement: keys after it wait for it", queued);

        FakeResources resources;
        resources.setMacros({{"dc", "được"}});
        const std::vector<uint32_t> macro{'n', 'h', 'e', 's', ' ', 'd', 'c', ngosen::key::Return, 'a'};
        std::vector<int>            macroGaps(macro.size(), 100);
        for (size_t i = 3; i < 7; ++i)
            macroGaps[i] = 1;
        const std::string expanded = ngosen::test::typeInApp(resources, makeField("wayland", true, "libreoffice-calc"), quirks, macro, macroGaps);
        check(expanded == "nhé được\na", "Enter during replacement: macro before it", expanded);

        // Google Docs ignores a line break inside a commit, so Shift+Enter goes out as a key too.
        const std::vector<uint32_t> shifted{'n', 'h', 'e', 's', ngosen::key::Return | ngosen::test::WithShift, 'a'};
        const std::string           lineBreak = ngosen::test::typeInApp(makeField("wayland", true, "libreoffice-calc"), quirks, shifted, gaps);
        check(lineBreak == "nhé\u2028a", "Enter during replacement: Shift+Enter", lineBreak);

        // A dbus client gets the commit after the reply to the key, so a passed Enter would land first.
        Field dbus                = makeField("dbus", false);
        dbus.keyEventOrderFix     = true;
        const std::string ordered = ngosen::test::typeInApp(dbus, quirks, keys, gaps);
        check(ordered == "nhé\na", "Enter during replacement: dbus", ordered);
        const std::string shiftedOnDbus = ngosen::test::typeInApp(dbus, quirks, shifted, gaps);
        check(shiftedOnDbus == "nhé\u2028a", "Enter during replacement: dbus Shift+Enter", shiftedOnDbus);

        std::vector<uint32_t> shiftedMacro = macro;
        shiftedMacro[7] |= ngosen::test::WithShift;
        const std::string macroLineBreak = ngosen::test::typeInApp(resources, makeField("wayland", true, "libreoffice-calc"), quirks, shiftedMacro, macroGaps);
        check(macroLineBreak == "nhé được\u2028a", "Enter during replacement: Shift+Enter after a macro", macroLineBreak);

        // fcitx5-gtk4 discards forwarded keys, so its Enter can only pass through, after the commit.
        Field gtk4                    = makeField("dbus", true);
        auto  gtk4Quirks              = quirks;
        gtk4Quirks.dropsForwardedKeys = true;
        for (const std::vector<int>& g : {gaps, keysAfterGaps}) {
            const std::string gtk4Text = ngosen::test::typeInApp(gtk4, gtk4Quirks, keys, g);
            check(gtk4Text == "nhé\na", "Enter during replacement: GTK4 over dbus", gtk4Text);
            const std::string gtk4Shifted = ngosen::test::typeInApp(gtk4, gtk4Quirks, shifted, g);
            check(gtk4Shifted == "nhé\u2028a", "Enter during replacement: GTK4 over dbus, Shift+Enter", gtk4Shifted);
        }

        // Tab moves to the next field, which a tab character inside the commit does not.
        quirks.showsTab = true;
        for (const uint32_t tab : {uint32_t{ngosen::key::Tab}, ngosen::key::ISO_Left_Tab | ngosen::test::WithShift}) {
            const std::vector<uint32_t> tabbed{'n', 'h', 'e', 's', tab, 'a'};
            const std::string           want  = tab == ngosen::key::Tab ? "nhé⇥a" : "nhé⇤a";
            const std::string           moved = ngosen::test::typeInApp(makeField("wayland", true, "libreoffice-calc"), quirks, tabbed, gaps);
            check(moved == want, std::string("Tab during replacement: ") + (tab == ngosen::key::Tab ? "Tab" : "Shift+Tab"), moved);
        }

        // A typed Tab leaves an abbreviation as it is, so a replayed one does too.
        std::vector<uint32_t> macroTab  = macro;
        macroTab[7]                     = ngosen::key::Tab;
        const std::string tabAfterMacro = ngosen::test::typeInApp(resources, makeField("wayland", true, "libreoffice-calc"), quirks, macroTab, macroGaps);
        check(tabAfterMacro == "nhé dc⇥a", "Tab during replacement: no macro", tabAfterMacro);
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

        typeTie(state, *host, loop, false);
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

    // WPS over D-Bus sets the same flags as GTK4 but never reports its text, and ignores deletions through it.
    void testSilentFieldGetsBackspaces() {
        ngosen::test::AppQuirks quirks;
        quirks.reports               = false;
        quirks.ignoresSurroundingDel = true;
        const std::vector<std::pair<std::string, std::string>> cases{{"thucws ", "thức "}, {"cos thuwcs ", "có thức "}};
        for (const auto& [typed, want] : cases) {
            const std::vector<uint32_t> keys(typed.begin(), typed.end());
            const std::string           text = ngosen::test::typeInApp(makeField("dbus", true, "wps"), quirks, keys, std::vector<int>(keys.size(), 60));
            check(text == want, "silent field gets backspaces: " + want, text);
        }
    }

    // A GTK4 field that comes back with its text unchanged does not report it again; a field that
    // comes back with no text known is silent again.
    void testRefocusKeepsReport(bool textKept) {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("dbus", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));

        host->setSurrounding("", 0);
        state.surroundingUpdated();
        state.deactivate(true);
        loop.pump(200);
        if (!textKept)
            host->forgetSurrounding();
        state.activate(ngosen::Mode::Sen, true);
        typeTie(state, *host, loop, false);
        press(state, static_cast<uint32_t>('e'));
        loop.pump(150);
        const std::string label = textKept ? "refocus with text kept: deletes through surrounding" : "refocus with no text: forwards backspaces";
        check(textKept == !host->deletes().empty() && textKept == host->forwarded().empty(), label,
              std::to_string(host->deletes().size()) + " deletions, " + std::to_string(host->forwarded().size()) + " forwarded");
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

    // GNOME Shell drops keys an IBus engine sends, so Sen deletes through the surrounding text, with a DEL in a
    // terminal, or types the word in the preedit where neither works.
    void testSenWithoutSentKeys() {
        const auto typed = [](bool surroundingText, bool terminal, ngosen::test::AppQuirks quirks, const std::string& text) {
            Field field               = makeField("ibus", surroundingText);
            field.terminal            = terminal;
            field.sendsNoKeys         = true;
            quirks.dropsForwardedKeys = true;
            const std::vector<uint32_t> keys(text.begin(), text.end());
            return ngosen::test::typeInApp(field, quirks, keys, std::vector<int>(keys.size(), 100));
        };
        std::string text = typed(true, false, {}, "vieetj nam ");
        check(text == "việt nam ", "ibus sen: deletes through the surrounding text", text);
        text = typed(false, true, {.erasesOnDel = true}, "vieetj nam");
        check(text == "việt nam", "ibus sen: a terminal erases with DEL", text);
        text = typed(false, false, {}, "vieetj nam ");
        check(text == "việt nam ", "ibus sen: falls back to the preedit where nothing deletes", text);
        text = typed(false, false, {}, "vieetj nam");
        check(text == "việt ", "ibus sen: the fallback word stays in the preedit until it ends", text);
        // Qt reports no surrounding text while the field is empty.
        text = typed(true, false, {.reportsTextOnceFilled = true}, "vieetj nams");
        check(text == "việt nám", "ibus sen: types in the field once it reports text", text);
        FakeResources spaces;
        spaces.mutableOptions().doubleSpaceToPeriod = true;
        Field bare                                  = makeField("ibus", false);
        bare.sendsNoKeys                            = true;
        const std::vector<uint32_t> keys{'n', 'a', 'm', ' ', ' ', 'a', 'b', ' '};
        text = ngosen::test::typeInApp(spaces, bare, {.dropsForwardedKeys = true}, keys, std::vector<int>(keys.size(), 100));
        check(text == "nam  ab ", "ibus sen: two spaces stay where nothing deletes", text);

        // A Facebook composer reports only its trailing newlines; it takes no selection keys here.
        Field field       = makeField("ibus", true);
        field.sendsNoKeys = true;
        {
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, field);
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            host->setSurrounding("\n\n", 2);
            for (char c : std::string("tiee"))
                press(state, static_cast<uint32_t>(c));
            loop.pump(300);
            check(host->deletes().size() == 1 && host->systemKeys().empty(), "ibus sen: a Facebook composer deletes through the surrounding text",
                  std::to_string(host->deletes().size()) + " " + std::to_string(host->systemKeys().size()));
        }
        {
            // A click ends the word, so the next one is typed the way the field now allows.
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, field);
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            host->setSurrounding("", 0);
            press(state, static_cast<uint32_t>('v'));
            press(state, static_cast<uint32_t>('i'));
            Field cleared           = field;
            cleared.surroundingText = false;
            host->setField(cleared);
            g_mouse_clicked.store(true);
            press(state, static_cast<uint32_t>('e'));
            press(state, static_cast<uint32_t>('e'));
            check(host->preedit() == "ê", "ibus sen: after a click the next word picks its mode again", host->preedit());
            g_mouse_clicked.store(false);
        }
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
        Field firefox            = makeField("fcitx4", true, "firefox");
        firefox.reportedProgram  = "org.mozilla.firefox";
        firefox.keyEventOrderFix = true;

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
        check(has(log, "\tfield\tfrontend=fcitx4 program=firefox (org.mozilla.firefox) surrounding=1 preedit=1 mode=Sen\n"), "recorder: notes the field and mode", log);
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

    // Enter's key text and multi-line app reports would otherwise split one event over several lines.
    void testRecorderKeepsOneEventPerLine() {
        ngosen::Recorder recorder;
        recorder.add("key", "down 0xff0d \r");
        recorder.add("app", "\"a\nb\tc\\|\" cursor=6");
        const std::string log = recorder.dump();
        check(std::count(log.begin(), log.end(), '\n') == 2, "recorder: one line per event", log);
        check(has(log, "\tkey\tdown 0xff0d \\r\n") && has(log, "\tapp\t\"a\\nb\\tc\\\\|\" cursor=6\n"), "recorder: escapes control characters", log);
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

        // Firefox bounces focus while a page loads and its reports trail the keys, so the first word can
        // look lost when it is only not reported yet.
        struct BounceReports {
            const char*              label;
            const char*              program;
            std::string              reportedBefore;
            std::vector<std::string> reports;
            bool                     keepsWord;
        };
        const std::vector<BounceReports> cases = {
            {"firefox, report one letter short", "firefox", "", {"ti"}, true},     {"firefox, report still empty", "firefox", "", {""}, true},
            {"firefox, report catching up", "firefox", "t", {"ti"}, true},         {"firefox, empty report then another field's text", "firefox", "", {"", "xin chao"}, false},
            {"firefox, another field's text", "firefox", "", {"xin chao"}, false}, {"firefox, whole word reported, then an empty field", "firefox", "tie", {""}, false},
            {"other app, report still empty", "gedit", "", {""}, false},
        };
        for (const auto& c : cases) {
            realMode = ngosen::Mode::Sen;
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland_v2", true, c.program));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            host->setSurrounding("", 0);
            state.surroundingUpdated();
            press(state, static_cast<uint32_t>('t'));
            press(state, static_cast<uint32_t>('i'));
            press(state, static_cast<uint32_t>('e'));
            loop.pump(40);
            if (!c.reportedBefore.empty()) {
                host->setSurrounding(c.reportedBefore, static_cast<unsigned int>(c.reportedBefore.size()));
                state.surroundingUpdated();
                loop.pump(40);
            }
            state.deactivate(true);
            loop.pump(3);
            state.activate(ngosen::Mode::Sen, true);
            for (const auto& report : c.reports) {
                host->setSurrounding(report, static_cast<unsigned int>(report.size()));
                state.surroundingUpdated();
            }
            const bool taken = press(state, static_cast<uint32_t>('e'));
            check(taken == c.keepsWord, std::string("focus bounce, ") + c.label + (c.keepsWord ? ": keeps the word" : ": starts a new word"),
                  taken ? "e changed the old word" : "e went to the app as a plain key");
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

    struct PreeditReopen {
        bool                                      bsAccepted = false;
        std::vector<std::pair<int, unsigned int>> deletes;
        std::string                               preedit;
    };

    // Types `keys` in Preedit, sets the text the app then reports and presses Backspace once.
    PreeditReopen backspaceAfterPreeditCommit(ngosen::TypingState& state, FakeHost& host, const std::string& keys, const std::string& reported, unsigned int anchor = ~0U) {
        for (const char c : keys) {
            if (c == '^') {
                FakeKey down('a', false, ngosen::modifier::Ctrl);
                state.keyEvent(down);
                FakeKey up('a', true, ngosen::modifier::Ctrl);
                state.keyEvent(up);
                continue;
            }
            press(state, c == '\n' ? ngosen::key::Return : static_cast<uint32_t>(c));
        }
        const auto cursor = static_cast<unsigned int>(ngosen::utf8::length(reported));
        host.setSurrounding(reported, cursor, anchor == ~0U ? cursor : anchor);
        state.surroundingUpdated();
        PreeditReopen r;
        r.bsAccepted = press(state, ngosen::key::BackSpace);
        r.deletes    = host.deletes();
        r.preedit    = host.preedit();
        return r;
    }

    void testPreeditBackspaceReopensWord() {
        realMode = ngosen::Mode::Preedit;
        {
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            const PreeditReopen r = backspaceAfterPreeditCommit(state, *host, "vieetj ", "việt ");
            check(r.bsAccepted && r.deletes.size() == 1 && r.deletes[0] == std::make_pair(-5, 5U), "preedit reopen: backspace takes the word and the space back from the app",
                  std::to_string(r.deletes.size()));
            check(r.preedit == "việt", "preedit reopen: the word shows as preedit again", r.preedit);
            press(state, static_cast<uint32_t>('s'));
            check(host->preedit() == "viết", "preedit reopen: s puts a new tone on the word", host->preedit());
            press(state, ngosen::key::space);
            check(join(host->commits()) == "['việt ']['viết ']", "preedit reopen: space commits the fixed word", join(host->commits()));
        }
        {
            // Held Backspace: the reopened word is deleted letter by letter, then the app deletes on its own.
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            backspaceAfterPreeditCommit(state, *host, "ban ", "ban ");
            for (int i = 0; i < 3; ++i)
                press(state, ngosen::key::BackSpace);
            check(host->preedit().empty(), "preedit reopen: held backspace empties the preedit", host->preedit());
            check(!press(state, ngosen::key::BackSpace), "preedit reopen: the next backspace goes on to the app", "taken");
            check(host->deletes().size() == 1 && join(host->commits()) == "['ban ']", "preedit reopen: held backspace deletes the word once",
                  std::to_string(host->deletes().size()) + " " + join(host->commits()));
        }
        {
            // GNOME's IBus relays surrounding text late, but a reset on every click forgets the word.
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("ibus", true));
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            const PreeditReopen r = backspaceAfterPreeditCommit(state, *host, "vieetj ", "việt ");
            check(r.bsAccepted && r.preedit == "việt", "preedit reopen: the IBus frontend too", r.preedit);
        }
        const auto notReopened = [](const Field& field, const std::string& keys, const std::string& reported, const std::string& what, const std::string& charset,
                                    unsigned int anchor = ~0U) {
            FakeLoop      loop;
            FakeResources resources;
            if (!charset.empty())
                resources.mutableOptions().outputCharset = charset;
            auto                owned = std::make_unique<FakeHost>(loop, field);
            FakeHost*           host  = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            const PreeditReopen r = backspaceAfterPreeditCommit(state, *host, keys, reported, anchor);
            check(!r.bsAccepted && r.deletes.empty() && r.preedit.empty(), "preedit reopen: " + what, r.preedit);
        };
        notReopened(makeField("wayland", true), "ban ", "xyz", "not after a click elsewhere", {});
        notReopened(makeField("wayland", false), "ban ", "", "not where the app reports no text", {});
        notReopened(makeField("wayland", true), "dduowc ", "ñöôc ", "not for a word in a legacy charset", "VNI Windows");
        notReopened(makeField("wayland", true), "tieen\n", "tiên", "not after Enter", {});
        // Ctrl+A commits the word and selects all; Backspace must delete the selection.
        notReopened(makeField("wayland", true), "vieetj^", "việt", "not after Ctrl+A commits the word", {});
        notReopened(makeField("wayland", true), "ban ", "ban ", "not over a selection", {}, 0);
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
        FakeLoop      loop;
        FakeResources resources;
        // An address bar keeps the backspace path, whose wait acts on the repeats.
        Field addressBar          = makeField("wayland", true, "firefox");
        addressBar.url            = true;
        auto                owned = std::make_unique<FakeHost>(loop, addressBar);
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
        FakeResources toned;
        expectTyped(toned, makeField("wayland", true), "nhanh^s ", "nhanhs ", "lock: a tone key after the lock key starts a word");
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

    // Over GNOME's IBus bridge a slow app can apply our backspace after the commit sent behind it, which then
    // erases the new letter (#362).
    void testSlowBackspaceOverIbus() {
        ngosen::test::AppQuirks slow;
        slow.backspaceLagMs = 70;
        FakeResources resources;
        expectTyped(resources, makeField("ibus", true), "daasu ", "dấu ", "a backspace applied 70 ms late over IBus", slow, 120);
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

    // A field that deletes only through its surrounding text, behind GNOME Shell's IBus bridge.
    Field surroundingOnlyField() {
        Field field       = makeField("ibus", true);
        field.sendsNoKeys = true;
        return field;
    }

    std::vector<uint32_t> keysOf(const std::string& typed) {
        return std::vector<uint32_t>(typed.begin(), typed.end());
    }

    // The fake app's quirks must break the text the way the real apps do, or a test on them proves
    // nothing about the path that avoids them.
    void testSurroundingQuirksBreakText() {
        const auto typed = [](ngosen::test::AppQuirks quirks, const std::string& text) {
            quirks.dropsForwardedKeys = true;
            const auto keys           = keysOf(text);
            return ngosen::test::typeInApp(surroundingOnlyField(), quirks, keys, std::vector<int>(keys.size(), 100));
        };
        std::string text = typed({.ignoresSurroundingDel = true}, "tiee");
        check(text == "tieê", "quirk: a dropped deletion leaves the old letter", text);
        text = typed({.reportsEmptyText = true}, "tiee");
        check(text == "tieê", "quirk: a field that reports no text drops the deletion", text);
        text = typed({.convertsDeleteOnReportedCopy = true, .reportsNothingAfterDelete = true}, "vieetj nam tieesng");
        // The copy stays "tie" while the app holds tiê, then tíê: the deletion asks for as many copy
        // characters as the real ones have bytes.
        check(text == "việt nam tiếng", "quirk: bytes counted on an old copy delete whole letters", text);
        {
            // The deletion, "à" and "y" in one turn show "ngy", as Chromium behind GNOME Shell did.
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, surroundingOnlyField(), ngosen::test::AppQuirks{.keepsLastEditPerTurn = true});
            auto*               app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            for (char c : std::string("nga"))
                app->type(static_cast<uint32_t>(c));
            loop.pump(20);
            app->deleteSurrounding(-1, 1);
            app->commitText("à");
            app->commitText("y");
            loop.pump(20);
            check(app->text() == "ngy", "quirk: one turn keeps only its last commit", app->text());
        }

        // The cursor moves with no report, so the deletion falls before the start of the text.
        realMode = ngosen::Mode::Sen;
        realtextLen.store(0);
        FakeLoop                loop;
        FakeResources           resources;
        ngosen::test::AppQuirks silent;
        silent.reports            = false;
        silent.dropsForwardedKeys = true;
        auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, surroundingOnlyField(), silent);
        auto*               app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (char c : std::string("vie")) {
            app->type(static_cast<uint32_t>(c));
            loop.pump(30);
        }
        app->moveCursor(0);
        app->type('e');
        loop.pump(300);
        check(app->text() == "êvie", "quirk: a deletion out of range is dropped", app->text());
    }

    void testSuggestion() {
        for (const bool selected : {true, false}) {
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, makeField("wayland", true), ngosen::test::AppQuirks{});
            auto*               app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            for (char c : std::string("vie")) {
                app->type(static_cast<uint32_t>(c));
                loop.pump(30);
            }
            app->suggest("tnam", selected);
            loop.pump(30);
            app->type('k');
            loop.pump(300);
            const std::string want = selected ? "viek" : "viektnam";
            check(app->text() == want, std::string("suggest: ") + (selected ? "a selected" : "an unselected") + " suggestion", app->text());
        }
    }

    // The path check must tell a field that fell back to backspaces from one that deleted through its
    // surrounding text, even when both end with the right text.
    void testPathCheck() {
        FakeResources resources;
        auto          keys = keysOf("vieetj nam ");
        auto          run  = ngosen::test::typeInAppTraced(resources, surroundingOnlyField(), {.dropsForwardedKeys = true}, keys, std::vector<int>(keys.size(), 100));
        auto          off  = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 2, .forwardedBackSpaces = 0, .systemKeyPresses = 0, .preedits = 0});
        check(run.text == "việt nam " && off.empty(), "path: two deletions through the surrounding text, nothing else", run.text + " " + join(off));

        keys = keysOf("daasu ");
        run  = ngosen::test::typeInAppTraced(resources, makeField("ibus", true), {.ignoresSurroundingDel = true}, keys, std::vector<int>(keys.size(), 100));
        off  = ngosen::test::pathMismatches(run.sent, {.forwardedBackSpaces = 0});
        check(run.text == "dấu " && !off.empty(), "path: right text through backspaces is not the surrounding path", run.text + " " + join(off));

        setXTestSenderForTest([](int) { return true; });
        keys = keysOf("daasu ");
        run  = ngosen::test::typeInAppTraced(resources, makeField("xim", false), {.takesXTest = true}, keys, std::vector<int>(keys.size(), 100));
        setXTestSenderForTest({});
        off = ngosen::test::pathMismatches(run.sent, {.systemKeyPresses = 0});
        check(run.text == "dấu " && !off.empty(), "path: backspaces pressed through XTEST are counted", run.text + " " + join(off));

        ngosen::test::FakeLoop loop;
        ngosen::test::FakeApp  app(loop, makeField("wayland", true), {});
        app.showPreedit("vi", true);
        off = ngosen::test::pathMismatches(app.sent(), {.preedits = 0});
        check(!off.empty(), "path: a shown preedit is counted", join(off));
    }

    // Types keys with every log line kept; returns the lines that contain `start`.
    std::vector<std::string> logLinesWhileTyping(const Field& field, ngosen::test::AppQuirks quirks, const std::string& typed, const std::string& start,
                                                 ngosen::test::Typed* run = nullptr, int gapMs = 100) {
        FakeResources resources;
        const auto    keys = keysOf(typed);
        ngosen::test::captureLog(true);
        const auto               typedRun = ngosen::test::typeInAppTraced(resources, field, quirks, keys, std::vector<int>(keys.size(), gapMs));
        std::vector<std::string> lines;
        for (const auto& line : ngosen::test::capturedLog()) {
            if (has(line, start))
                lines.push_back(line);
        }
        ngosen::test::captureLog(false);
        if (run != nullptr)
            *run = typedRun;
        return lines;
    }

    size_t countHaving(const std::vector<std::string>& lines, const std::string& part) {
        return static_cast<size_t>(std::count_if(lines.begin(), lines.end(), [&](const std::string& line) { return has(line, part); }));
    }

    // Leaves the field while the replacement for the second "e" waits for the app's copy of the text,
    // then types "n" after coming back.
    void checkLeavingDuringReportWait(int leaveAfterMs, int awayMs, const std::string& expected, int expectedDeletes, const std::string& name, int gapMs = 40,
                                      bool otherFieldReplacing = false, int lagMs = 150) {
        realMode = ngosen::Mode::Sen;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        auto          owned = std::make_unique<ngosen::test::FakeApp>(loop, surroundingOnlyField(),
                                                                      ngosen::test::AppQuirks{.lagMs = lagMs, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true});
        auto*         app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (char c : std::string("viee")) {
            loop.pump(gapMs);
            app->type(static_cast<uint32_t>(c));
        }
        const bool waiting = app->sent().surroundingDeletes == 0;
        loop.pump(leaveAfterMs);
        app->leaveFocus(awayMs);
        if (otherFieldReplacing)
            is_deleting_.store(false);
        loop.pump(awayMs + 10);
        app->type(static_cast<uint32_t>('n'));
        loop.pump(400);
        check(waiting && app->text() == expected && app->sent().surroundingDeletes == expectedDeletes, name,
              app->text() + " deletes " + std::to_string(app->sent().surroundingDeletes));
    }

    // After a deletion through the surrounding text, the next reports tell what the app did with it.
    void testSurroundingCheck() {
        ngosen::test::Typed run;
        auto                lines = logLinesWhileTyping(surroundingOnlyField(), {.dropsForwardedKeys = true, .ignoresSurroundingDel = true}, "tiee", "Surrounding check", &run);
        check(countHaving(lines, "app dropped the deletion") == 1 && run.sent.forwardedBackSpaces == 0 && run.sent.systemKeyPresses == 0, "check: a dropped deletion is logged",
              run.text + " " + join(lines));

        lines = logLinesWhileTyping(surroundingOnlyField(), {.staleReport = true, .lagMs = 40, .dropsForwardedKeys = true}, "vieetj nam ", "Surrounding check");
        check(countHaving(lines, ": correct") == 2 && countHaving(lines, "untrusted") == 0, "check: a lagging app is judged correct", join(lines));

        // The app lags behind the keys: the next replacement comes before it answers the first, and the
        // Space before it answers the second.
        lines =
            logLinesWhileTyping(surroundingOnlyField(), {.lagMs = 60, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true}, "vieetj ", "Surrounding check", &run, 20);
        check(run.text == "việt " && countHaving(lines, "not answered before the next replacement") == 1 && countHaving(lines, ": correct") == 1,
              "check: a lagging app answers only the last replacement", run.text + " " + join(lines));

        // Keys faster than the app: its copy does not hold the letter yet when the first deletion goes out.
        for (const auto [lagMs, gapMs] : std::vector<std::pair<int, int>>{{60, 15}, {60, 19}, {150, 40}, {150, 50}, {150, 60}}) {
            lines = logLinesWhileTyping(surroundingOnlyField(), {.lagMs = lagMs, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true}, "vieetj ", "Surrounding check",
                                        &run, gapMs);
            check(run.text == "việt ", "check: keys " + std::to_string(gapMs) + " ms apart in an app " + std::to_string(lagMs) + " ms behind", run.text + " " + join(lines));
        }

        checkLeavingDuringReportWait(0, 300, "vien", 0, "wait: the replacement does not edit a field the user left");
        checkLeavingDuringReportWait(0, 5, "viên", 1, "wait: a focus bounce keeps the replacement");
        checkLeavingDuringReportWait(120, 95, "vien", 0, "wait: keys still work after focus returns to a dropped replacement", 40, false, 250);
        checkLeavingDuringReportWait(0, 5, "vien", 0, "wait: another field replacing meanwhile ends the wait", 40, true);
        // The deadline falls inside the bounce. The copy is still short, so the timer deletes anyway, as without one.
        checkLeavingDuringReportWait(198, 5, "vieên", 1, "wait: a focus bounce across the deadline keeps the replacement", 5, false, 250);

        lines = logLinesWhileTyping(surroundingOnlyField(), {.reports = false, .dropsForwardedKeys = true}, "vieetj nam tieesng ", "Surrounding check");
        // Silence alone is no reason to give up on a field that has no other way to delete.
        check(countHaving(lines, "not answered") >= 3 && countHaving(lines, "untrusted") == 0, "check: an app that never reports goes unanswered", join(lines));

        // A field outside the surrounding rule keeps its keys, and logs what the surrounding path would have done.
        lines = logLinesWhileTyping(makeField("fcitx4", true), {}, "vieetj nam ", "Surrounding path would", &run);
        check(lines.size() == 2 && countHaving(lines, "fails: none") == 2 && run.sent.surroundingDeletes == 0, "check: one observation line per replacement on an fcitx4 field",
              run.text + " " + join(lines));

        // A message written as `cond ? "a" : "b"` must reach the log as text, not as the condition's value.
        lines = logLinesWhileTyping(makeField("fcitx4", true), {.reportsEmptyText = true}, "vieetj ", "Surr wait skip");
        check(countHaving(lines, "Surr wait skip: empty snapshot") == 2, "log: a ternary message keeps its text", join(lines));
    }

    // An address bar that sets no Url flag still shows itself by filling in text after the cursor.
    void testAutofillMark() {
        realMode = ngosen::Mode::Sen;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        Field         field       = surroundingOnlyField();
        field.preeditFallback     = true;
        auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, field, ngosen::test::AppQuirks{.dropsForwardedKeys = true});
        auto*               app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        const auto typeText = [&](const std::string& text) {
            for (char c : text) {
                app->type(static_cast<uint32_t>(c));
                loop.pump(100);
            }
        };
        ngosen::test::captureLog(true);
        typeText("too vie");
        app->suggest("tnam.com", false);
        loop.pump(30);
        typeText("etj too ");
        loop.pump(300);
        const auto lines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(app->text() == "tô vieetj tô tnam.com" && countHaving(lines, "Text appeared after the cursor without a key") == 1 &&
                  countHaving(lines, "Type the word in the preedit: marked untrusted") == 1,
              "fallback: text filled in after the cursor sends the next words to the preedit", app->text() + " " + join(lines));
    }

    // '<' stands for a Backspace press.
    std::string typedWithBackspaces(const Field& field, ngosen::test::AppQuirks quirks, const std::string& typed, const std::vector<int>& gapsMs, std::vector<std::string>& lines,
                                    ngosen::test::Path* sent = nullptr) {
        FakeResources resources;
        auto          keys = keysOf(typed);
        std::replace(keys.begin(), keys.end(), static_cast<uint32_t>('<'), static_cast<uint32_t>(ngosen::key::BackSpace));
        ngosen::test::captureLog(true);
        const auto run = ngosen::test::typeInAppTraced(resources, field, quirks, keys, gapsMs);
        lines          = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        if (sent != nullptr)
            *sent = run.sent;
        return run.text;
    }

    std::string typedWithBackspaces(const Field& field, ngosen::test::AppQuirks quirks, const std::string& typed, int gapMs, std::vector<std::string>& lines,
                                    ngosen::test::Path* sent = nullptr) {
        return typedWithBackspaces(field, quirks, typed, std::vector<int>(typed.size(), gapMs), lines, sent);
    }

    // An app over IBus may report a replacement only with a later key, so its check stays open that long.
    void testReportWithNextKey() {
        const Field              gnome = makeField("ibus", true);
        std::vector<std::string> lines;
        ngosen::test::Path       sent;
        for (const int gapMs : {290, 400, 670}) {
            const auto text = typedWithBackspaces(gnome, {.repeatsOldTextAfterDelete = true}, "vieetj nam tieesng tooi ", gapMs, lines, &sent);
            check(text == "việt nam tiếng tôi " && countHaving(lines, ": correct") > 0 && countHaving(lines, "1 keys later") > 0 && countHaving(lines, "untrusted") == 0 &&
                      sent.surroundingDeletes > 0,
                  "next key: its report answers the check, " + std::to_string(gapMs) + " ms apart", text + " " + join(lines));
        }
        for (const int gapMs : {290, 400}) {
            const auto text = typedWithBackspaces(gnome, {.repeatsOldTextAfterDelete = true, .keysBeforeNewReport = 2}, "vieetj nam tieesng tooi ", gapMs, lines, &sent);
            check(text == "việt nam tiếng tôi " && countHaving(lines, "2 keys later") > 0 && countHaving(lines, "untrusted") == 0,
                  "next key: the report may come with the second key, " + std::to_string(gapMs) + " ms apart", text + " " + join(lines));
        }

        // Other frontends keep the short check, and the mark where the field has another way to delete.
        auto text = typedWithBackspaces(makeField("wayland", true, "someapp"), {.repeatsOldTextAfterDelete = true}, "vieetj nam tieesng tooi ", 400, lines);
        check(text == "việt nam tiếng tôi " && countHaving(lines, "after 200 ms") > 0 && countHaving(lines, "went unanswered") == 1, "next key: wayland keeps the short check",
              text + " " + join(lines));
        text = typedWithBackspaces(makeField("dbus", true, "someapp"), {.repeatsOldTextAfterDelete = true}, "vieetj nam tieesng tooi ", 400, lines);
        check(text == "việt nam tiếng tôi " && countHaving(lines, "after 200 ms") > 0 && countHaving(lines, ": correct") == 0, "next key: dbus keeps the short check",
              text + " " + join(lines));
    }

    // An app silent from its first deletion is caught by the copy check, so no mark is needed.
    void testSilentFromFirstDeletion() {
        const Field              gnome = makeField("ibus", true);
        std::vector<std::string> lines;
        ngosen::test::Path       sent;
        auto                     text = typedWithBackspaces(gnome, {.silentFromFirstDeletion = true}, "vieetj nam ", 400, lines);
        check(text == "việt nam " && countHaving(lines, "went unanswered") == 0, "silent: words after the first delete with backspaces", text + " " + join(lines));

        text = typedWithBackspaces(gnome, {.ignoresSurroundingDel = true, .silentFromFirstDeletion = true}, "vieetj nam ", 400, lines, &sent);
        check(sent.surroundingDeletes == 1, "silent: a dropped deletion costs one word", text + " " + join(lines));

        Field preeditHost           = surroundingOnlyField();
        preeditHost.preeditFallback = true;
        text                        = typedWithBackspaces(preeditHost, {.silentFromFirstDeletion = true}, "too too too too ", 400, lines);
        check(countHaving(lines, "went unanswered") == 1, "silent: a host with a preedit still gives up on the field", text + " " + join(lines));
    }

    void testBothPathsInOneWord() {
        std::vector<std::string> lines;
        ngosen::test::Path       sent;
        for (const auto& quirks : {ngosen::test::AppQuirks{.reportDelayMs = 300}, ngosen::test::AppQuirks{.reportDelayMs = 300, .repeatsOldTextAfterDelete = true}}) {
            const auto text = typedWithBackspaces(makeField("ibus", true), quirks, "tieesng<<ng", 120, lines, &sent);
            check(text == "tiếng" && sent.surroundingDeletes > 0 && sent.forwardedBackSpaces > 0, "both paths: one word mixes them and stays right", text + " " + join(lines));
        }
    }

    // A replacement through backspaces while a check is open changes what stands before that check's commit.
    void testBackspacePathDuringLongCheck() {
        std::vector<std::string> lines;
        for (const int gapMs : {100, 240, 400}) {
            const auto text = typedWithBackspaces(makeField("ibus", true), {.repeatsOldTextAfterDelete = true}, "tieesng tieesng ", gapMs, lines);
            check(text == "tiếng tiếng " && countHaving(lines, "Surrounding text untrusted") == 0,
                  "long check: backspaces in the next replacement, keys " + std::to_string(gapMs) + " ms", text + " " + join(lines));
        }
    }

    bool lastReplacementDeletedThroughCopy(const std::vector<std::string>& lines) {
        bool deleted = false;
        for (const auto& line : lines) {
            if (line.find("Perform replacement") != std::string::npos)
                deleted = false;
            else if (line.find("Delete using surrounding") != std::string::npos)
                deleted = true;
        }
        return deleted;
    }

    // A report still on its way can show the word a Backspace left while the app holds other text.
    void testLateReportAfterBackspace() {
        const Field              gnome = makeField("ibus", true);
        std::vector<std::string> lines;
        const std::string        typed = "muoons tieesng<<ng ";
        std::vector<int>         gapsMs(typed.size(), 500);
        // From the s of the word, so the report of its commit lands just before the key after the Backspaces.
        gapsMs[11] = 20;
        std::fill(gapsMs.begin() + 12, gapsMs.begin() + 16, 15);
        auto text = typedWithBackspaces(gnome, {.reportDelayMs = 80, .reportsTextAtChange = true}, typed, gapsMs, lines);
        check(text == "muốn tiếng " && !lastReplacementDeletedThroughCopy(lines), "late report: the word goes on through backspaces after the user's Backspace",
              text + " " + join(lines));

        text = typedWithBackspaces(gnome, {}, "too<oo too ", 400, lines);
        check(text == "tô tô " && lastReplacementDeletedThroughCopy(lines), "late report: the next word uses the surrounding text again", text + " " + join(lines));

        // The space comes during the s's replacement, so it ends the word only when replayed.
        const std::string quick = "too<oos too ";
        gapsMs.assign(quick.size(), 400);
        gapsMs[6] = 5;
        text      = typedWithBackspaces(gnome, {.lagMs = 40}, quick, gapsMs, lines);
        check(text == "tố tô " && lastReplacementDeletedThroughCopy(lines), "late report: the next word uses the surrounding text after a replayed space",
              text + " " + join(lines));

        ngosen::test::Path sent;
        text = typedWithBackspaces(makeField("wayland", true, "someapp"), {}, "too<oo ", 400, lines, &sent);
        check(text == "tô " && sent.forwardedBackSpaces > 0, "late report: other frontends go on through backspaces too", text + " " + join(lines));
    }

    // A long check must still see a deletion the app dropped.
    void testDroppedDeletionInLongCheck() {
        const Field              gnome = makeField("ibus", true);
        std::vector<std::string> lines;
        auto                     text = typedWithBackspaces(gnome, {.reportDelayMs = 300, .ignoresSurroundingDel = true}, "too too ", 400, lines);
        check(text == "tô tô " && countHaving(lines, "Fix the dropped deletion") == 1 && countHaving(lines, "Surrounding text untrusted") == 1,
              "long check: a late report of a dropped deletion is fixed", text + " " + join(lines));

        text = typedWithBackspaces(gnome, {.repeatsOldTextAfterDelete = true, .deletesOneTooMany = true}, "coo too too ", 400, lines);
        check(countHaving(lines, "text before it wrong") >= 1 && countHaving(lines, "Surrounding text untrusted") == 1,
              "long check: a wrong deletion reported late marks the field", text + " " + join(lines));

        text = typedWithBackspaces(gnome, {.ignoresSurroundingDel = true, .repeatsOldTextAfterDelete = true}, "too too ", 400, lines);
        check(text == "toô tô " && countHaving(lines, "Leave the dropped deletion") == 1 && countHaving(lines, "Surrounding text untrusted") == 1,
              "long check: a dropped deletion seen after a key costs one word", text + " " + join(lines));
    }

    // The user's own edits while a long check is open say nothing about the app.
    void testEditsDuringLongCheck() {
        const Field              gnome = makeField("ibus", true);
        std::vector<std::string> lines;
        for (const int gapMs : {90, 150, 300}) {
            const auto text = typedWithBackspaces(gnome, {.repeatsOldTextAfterDelete = true}, "coo too<<", gapMs, lines);
            check(countHaving(lines, "Surrounding text untrusted") == 0 && countHaving(lines, "text before it wrong") == 0,
                  "long check: Backspace is not a wrong report, " + std::to_string(gapMs) + " ms apart", text + " " + join(lines));
        }
        const auto text = typedWithBackspaces(gnome, {.repeatsOldTextAfterDelete = true}, "coos<o ", 400, lines);
        check(countHaving(lines, "Surrounding text untrusted") == 0 && countHaving(lines, "Fix the dropped deletion") == 0,
              "long check: a word typed crooked is not a dropped deletion", text + " " + join(lines));
    }

    // Backspaces reach an app out of focus but a commit does not, so a fix then would lose the word.
    void testLongCheckLeavesFocus() {
        for (const int delayMs : {300, 500, 800}) {
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop      loop;
            FakeResources resources;
            auto  owned = std::make_unique<ngosen::test::FakeApp>(loop, makeField("ibus", true), ngosen::test::AppQuirks{.reportDelayMs = delayMs, .ignoresSurroundingDel = true});
            auto* app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            const std::string typed = "xin too";
            for (size_t i = 0; i < typed.size(); ++i) {
                app->type(static_cast<uint32_t>(typed[i]));
                loop.pump(i + 1 == typed.size() ? 100 : 600);
            }
            ngosen::test::captureLog(true);
            app->leaveFocus(2000);
            loop.pump(1000);
            const auto lines = ngosen::test::capturedLog();
            ngosen::test::captureLog(false);
            check(app->text() == "xin toô" && countHaving(lines, "Fix the dropped deletion") == 0,
                  "long check: no fix while out of focus, report after " + std::to_string(delayMs) + " ms", app->text() + " " + join(lines));
        }
    }

    // An app that answers nothing in time gets no mark over IBus: each word still picks its own way.
    void testUnansweredChecksLeaveNoMark() {
        std::vector<std::string> lines;
        const auto               text = typedWithBackspaces(makeField("ibus", true), {.repeatsOldTextAfterDelete = true}, "vieetj nam tieesng tooi ", 1600, lines);
        check(text == "việt nam tiếng tôi " && countHaving(lines, "not answered") > 0 && countHaving(lines, "untrusted") == 0, "no mark: unanswered checks over IBus",
              text + " " + join(lines));
    }

    // An app whose copy still holds the replaced characters until the next key, as Firefox on GNOME
    // does: a deletion counted on that copy takes too many.
    void testSlowReport() {
        ngosen::test::Typed run;
        const Field         gnome = makeField("ibus", true);
        for (const auto& quirks : {ngosen::test::AppQuirks{.reportsNothingAfterDelete = true}, ngosen::test::AppQuirks{.repeatsOldTextAfterDelete = true}}) {
            const auto lines = logLinesWhileTyping(gnome, quirks, "vieetj nam tieesng ", "", &run, 150);
            check(run.text == "việt nam tiếng " && countHaving(lines, "Surrounding text untrusted") == 0 && countHaving(lines, "copy older than the last commit") > 0 &&
                      run.sent.forwardedBackSpaces > 0,
                  "slow report: a copy from before the last replacement is not deleted on", run.text + " " + join(lines));
        }
    }

    // Qt over GNOME Shell's IBus bridge stops reporting after a deletion, so the next replacement in
    // the word sees the copy from before the last one.
    void testStaleCopyBeforeReplacement() {
        ngosen::test::Typed run;
        Field               field = surroundingOnlyField();
        field.preeditFallback     = true;
        const ngosen::test::AppQuirks qt{.dropsForwardedKeys = true, .reportsTextOnceFilled = true, .convertsDeleteOnReportedCopy = true, .reportsNothingAfterDelete = true};
        const auto                    lines = logLinesWhileTyping(field, qt, "nam tieesng ", "", &run, 150);
        check(run.text == "nam tiếng " && countHaving(lines, "Keep the word as typed") == 0 && countHaving(lines, "copy shows the earlier word") > 0,
              "stale copy: a replacement after a replacement deletes on the older copy", run.text + " " + join(lines));
    }

    // Firefox on GNOME turns the deletion's bytes back into characters on a copy it updates only when
    // a commit arrives after the edit, so a count made for that old copy takes letters from the real text.
    void testFirefoxReportsOnlyAfterCommit() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        for (const int commits : {1, 2}) {
            ngosen::test::AppQuirks firefox{.repeatsOldTextAfterDelete = true, .emptyCommitsBeforeReport = commits};
            FakeResources           resources;
            const auto              keys = keysOf("bef owr tieesng ");
            const std::vector<int>  gaps(keys.size(), 150);
            ngosen::test::captureLog(true);
            const auto run   = ngosen::test::typeInAppTraced(resources, field, firefox, keys, gaps);
            const auto lines = ngosen::test::capturedLog();
            ngosen::test::captureLog(false);
            check(run.text == "bè ở tiếng ", "Firefox on GNOME: a replacement right after a replacement keeps the letters before it", run.text + " " + join(lines));
            check(countHaving(lines, "copy shows the earlier word") == 0, "Firefox on GNOME: asking for a report leaves no old copy to count on", join(lines));
        }
        FakeResources resources;
        const auto    keys = keysOf("bef owr tieesng ");
        ngosen::test::captureLog(true);
        const auto run   = ngosen::test::typeInAppTraced(resources, field, ngosen::test::AppQuirks{}, keys, std::vector<int>(keys.size(), 150));
        const auto lines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(run.text == "bè ở tiếng " && countHaving(lines, "Ask the app for a report") == 0, "an app that reports in time gets no empty commit", run.text + " " + join(lines));
    }

    // The request left over from one edit must not land in the next edit's turn and replace its commit.
    void testReportRequestKeepsNextEdit() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks shell{
            .reportDelayMs = 30, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true, .keepsLastEditPerTurn = true, .turnMs = 3};
        const auto keys = keysOf("nguwx ");
        for (const int gap : {38, 39, 40, 41}) {
            std::vector<int> gaps(keys.size(), 100);
            gaps[3] = gap; // w and x
            FakeResources resources;
            const auto    run = ngosen::test::typeInAppTraced(resources, field, shell, keys, gaps);
            check(run.text == "ngữ ", "a report request stays out of the next edit's turn, gap " + std::to_string(gap), run.text);
        }
    }

    // A macro taken on Enter skips the hold, so keys typed during the split replay from the split's timer.
    void testSplitTimerSurvivesReplay() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks shell{.dropsForwardedKeys = true, .keepsLastEditPerTurn = true};
        std::vector<uint32_t>   keys = keysOf("ko");
        keys.push_back(ngosen::key::Return);
        for (const uint32_t key : keysOf("aa"))
            keys.push_back(key);
        FakeResources resources;
        resources.setMacros({{"ko", "không"}});
        const int destroyedBefore = ngosen::test::timersDestroyedInCallback;
        ngosen::test::captureLog(true);
        ngosen::test::typeInAppTraced(resources, field, shell, keys, std::vector<int>(keys.size(), 3));
        const auto lines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(countHaving(lines, "Starting replay buffered keys") > 0, "split: keys typed during it replay from its timer", join(lines));
        check(ngosen::test::timersDestroyedInCallback == destroyedBefore, "split: its timer is not destroyed from its own callback",
              std::to_string(ngosen::test::timersDestroyedInCallback - destroyedBefore));
    }

    // Types "tie" with what the app shows after it, then the key that replaces its e.
    struct TailRun {
        FakeLoop                             loop;
        FakeResources                        resources;
        FakeHost*                            host = nullptr;
        std::unique_ptr<ngosen::TypingState> state;

        TailRun(const std::string& shown, unsigned int anchor) {
            realMode                = ngosen::Mode::Sen;
            Field field             = surroundingOnlyField();
            field.program           = "gnome-shell";
            auto owned              = std::make_unique<FakeHost>(loop, field);
            host                    = owned.get();
            state                   = std::make_unique<ngosen::TypingState>(&resources, std::move(owned));
            const std::string typed = "tie";
            for (size_t i = 0; i < typed.size(); ++i) {
                press(*state, static_cast<uint32_t>(typed[i]));
                host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
                state->surroundingUpdated();
                loop.pump(100);
            }
            host->setSurrounding(shown, 3, anchor);
            state->surroundingUpdated();
            press(*state, 'e');
        }
    };

    // Firefox's address bar drops its autocompletion only for a commit in the deletion's own turn.
    void testAutofillTailCommitsWithDeletion() {
        TailRun url("tieng.lan:8765/", 3);
        check(url.host->commits() == std::vector<std::string>{"ê"}, "autofill tail: the commit goes with the deletion", join(url.host->commits()));
        TailRun plainWord("tiechao", 3);
        check(plainWord.host->commits().empty(), "autofill tail: a plain word after the cursor keeps the split", join(plainWord.host->commits()));
        TailRun words("tie chào", 3);
        check(words.host->commits().empty(), "autofill tail: text with a space after the cursor keeps the split", join(words.host->commits()));
    }

    // Chromium's address bar ignores deletions while its autocompletion is selected; a commit removes that.
    void testSelectedTailGoesFirst() {
        using Deletes = std::vector<std::pair<int, unsigned int>>;
        TailRun run("tieng.lan:8765/", 15);
        check(run.host->commits() == std::vector<std::string>{" "}, "selected tail: a space goes over it first", join(run.host->commits()));
        check(run.host->deletes().empty(), "selected tail: no deletion while it stands", std::to_string(run.host->deletes().size()));
        run.host->setSurrounding("tie ", 4);
        run.state->surroundingUpdated();
        run.loop.pump(200);
        check(run.host->deletes() == Deletes{{-2, 2}}, "selected tail: then the space and the letters", std::to_string(run.host->deletes().size()));
        check(run.host->commits().size() > 1 && run.host->commits()[1] == "ê", "selected tail: then the commit", join(run.host->commits()));

        TailRun silent("tieng.lan:8765/", 15);
        silent.loop.pump(200);
        check(silent.host->deletes() == Deletes{{-2, 2}}, "selected tail: no report changes nothing", std::to_string(silent.host->deletes().size()));

        TailRun moved("tieng.lan:8765/", 15);
        moved.host->setHasFocus(false);
        moved.state->deactivate(true);
        moved.loop.pump(105);
        moved.host->setHasFocus(true);
        moved.state->activate(ngosen::Mode::Sen, true);
        moved.loop.pump(200);
        check(moved.host->deletes().empty(), "selected tail: another field gets none of the rest", std::to_string(moved.host->deletes().size()));

        TailRun inside("tieng.lan:8765/ x", 15);
        check(inside.host->deletes() == Deletes{{-1, 1}}, "selected tail: a selection ending before the text's end is the user's", std::to_string(inside.host->deletes().size()));
    }

    // The deletion is already out, so an app that bounces focus past the split must still get the commit.
    void testSplitOutlastsFocusBounce() {
        realMode      = ngosen::Mode::Sen;
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        const std::string   typed = "tie";
        for (size_t i = 0; i < typed.size(); ++i) {
            press(state, static_cast<uint32_t>(typed[i]));
            host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            state.surroundingUpdated();
            loop.pump(100);
        }
        press(state, 'e');
        host->setHasFocus(false);
        state.deactivate(true);
        loop.pump(50);
        host->setHasFocus(true);
        state.activate(ngosen::Mode::Sen, true);
        loop.pump(100);
        check(!host->commits().empty() && host->commits().front() == "ê", "split: the commit waits out a focus bounce", join(host->commits()));
    }

    // An empty commit asking for a report is not input, so a bounce after it says nothing about redraws.
    void testReportRequestIsNotInput() {
        realMode                  = ngosen::Mode::Sen;
        Field               field = makeField("ibus", true, "gnome-shell");
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        host->setCommitWatcher([&state](const std::string& text) { state.noteCommit(text); });
        const std::string typed = "tie";
        for (size_t i = 0; i < typed.size(); ++i) {
            press(state, static_cast<uint32_t>(typed[i]));
            host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            state.surroundingUpdated();
            loop.pump(100);
        }
        press(state, 'e');
        loop.pump(170);
        ngosen::test::captureLog(true);
        state.deactivate(true);
        state.activate(ngosen::Mode::Sen, true);
        loop.pump(200);
        host->setSurrounding("tiê", 3);
        state.surroundingUpdated();
        press(state, 's');
        loop.pump(100);
        const auto lines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(countHaving(lines, "field refocuses after edits") == 0, "a report request does not make a bounce look like a redraw", join(lines));
    }

    // A field going to backspaces anyway has nothing to gain from the copy.
    void testNoCopyWaitForBackspaceField() {
        realMode    = ngosen::Mode::Sen;
        Field field = makeField("wayland", true);
        field.url   = true;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        const std::string   typed = "tie";
        for (size_t i = 0; i < typed.size(); ++i) {
            press(state, static_cast<uint32_t>(typed[i]));
            host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            state.surroundingUpdated();
            loop.pump(100);
        }
        host->setSurrounding("other", 5);
        state.surroundingUpdated();
        press(state, 'e');
        check(!host->forwarded().empty(), "an address bar does not wait for a copy it will not use", "no backspaces at once");
    }

    // Focus moved inside the context to a field with no surrounding text while waiting for the copy.
    void testCopyWaitEndsOnInvalidReport() {
        realMode = ngosen::Mode::Sen;
        FakeLoop            loop;
        FakeResources       resources;
        auto                owned = std::make_unique<FakeHost>(loop, makeField("wayland", true));
        FakeHost*           host  = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        const std::string   typed = "tie";
        for (size_t i = 0; i < typed.size(); ++i) {
            press(state, static_cast<uint32_t>(typed[i]));
            host->setSurrounding(typed.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            state.surroundingUpdated();
            loop.pump(100);
        }
        ngosen::test::captureLog(true);
        host->setSurrounding("other", 5);
        state.surroundingUpdated();
        press(state, 'e');
        host->forgetSurrounding();
        state.surroundingUpdated();
        loop.pump(400);
        check(host->commits() == std::vector<std::string>{"e"}, "a field that stops reporting gets no replacement",
              join(host->commits()) + " | " + join(ngosen::test::capturedLog()));
        ngosen::test::captureLog(false);
    }

    // Firefox reports most edits unasked and misses a few; its copy for the deletion is then as old as
    // mutter's, so counting on it takes one letter too many. Qt, which rarely reports, needs that count.
    void testAnsweringAppGetsPlainCount() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks firefox{.repeatsOldTextAfterDelete = true, .emptyCommitsBeforeReport = 1, .ignoresRequestsOnEdits = {9}};
        FakeResources           resources;
        const auto              keys = keysOf("bef owr bef owr vieetj tieesng ");
        const auto              run  = ngosen::test::typeInAppTraced(resources, field, firefox, keys, std::vector<int>(keys.size(), 150));
        check(run.text == "bè ở bè ở việt tiếng ", "an app that mostly answers keeps its letters when it misses one report", run.text);

        // On a fresh page the misses can come before there are many answers to weigh them against.
        firefox.ignoresRequestsOnEdits = {2, 3};
        firefox.reportsOnFocus         = true;
        FakeResources early;
        const auto    earlyRun = ngosen::test::typeInAppTraced(early, field, firefox, keys, std::vector<int>(keys.size(), 150));
        check(earlyRun.text == "bè ở bè ở việt tiếng ", "an app that answered once keeps its letters when it misses the next reports", earlyRun.text);

        // CodeMirror in Firefox hardly answers, but it reported its empty field, which Qt does not do.
        ngosen::test::AppQuirks silent{.repeatsOldTextAfterDelete = true, .emptyCommitsBeforeReport = 9, .reportsOnFocus = true};
        FakeResources           silentResources;
        const auto              silentKeys = keysOf("bef owr tieesng ");
        const auto              silentRun  = ngosen::test::typeInAppTraced(silentResources, field, silent, silentKeys, std::vector<int>(silentKeys.size(), 150));
        check(silentRun.text == "bè ở tiếng ", "an app that reported its empty field keeps its letters without answering", silentRun.text);

        // Qt answers now and then too, but it applies the bytes to its real text.
        ngosen::test::AppQuirks qt{
            .dropsForwardedKeys = true, .reportsTextOnceFilled = true, .convertsDeleteOnReportedCopy = true, .reportsNothingAfterDelete = true, .reportsOnEdit = 1};
        FakeResources qtResources;
        Field         qtField   = surroundingOnlyField();
        qtField.preeditFallback = true;
        const auto qtKeys       = keysOf("bef bef tieesng ");
        const auto qtRun        = ngosen::test::typeInAppTraced(qtResources, qtField, qt, qtKeys, std::vector<int>(qtKeys.size(), 500));
        check(qtRun.text == "bè bè tiếng ", "an app that rarely answers keeps the count for its real text", qtRun.text);
    }

    // Backspaces reach Firefox on Plasma after the commit often enough to eat the new letter when typing fast.
    void testFirefoxOnPlasmaDeletesThroughSurrounding() {
        const Field             field = makeField("wayland", true, "firefox");
        ngosen::test::AppQuirks firefox{.repeatsOldTextAfterDelete = true, .emptyCommitsBeforeReport = 1};
        FakeResources           resources;
        const auto              keys = keysOf("tieengs vieetj nguwx ");
        const auto              run  = ngosen::test::typeInAppTraced(resources, field, firefox, keys, std::vector<int>(keys.size(), 8));
        check(run.text == "tiếng việt ngữ ", "firefox on plasma: the words come out right when typing fast", run.text);
        check(run.sent.surroundingDeletes > 0 && run.sent.forwardedBackSpaces == 0, "firefox on plasma: no backspaces",
              std::to_string(run.sent.surroundingDeletes) + " " + std::to_string(run.sent.forwardedBackSpaces));

        // CodeMirror and Ace in Firefox answer only on the next key, so waiting for them only adds delay.
        ngosen::test::AppQuirks silent{.repeatsOldTextAfterDelete = true, .keysBeforeNewReport = 3, .emptyCommitsBeforeReport = 9};
        FakeResources           silentResources;
        ngosen::test::captureLog(true);
        const auto silentRun   = ngosen::test::typeInAppTraced(silentResources, field, silent, keys, std::vector<int>(keys.size(), 150));
        const auto silentLines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(silentRun.text == "tiếng việt ngữ ", "firefox on plasma: a field that does not answer still types right", silentRun.text);
        check(countHaving(silentLines, "Copy of the text still short") <= 1, "firefox on plasma: a field that does not answer stops holding keys", join(silentLines));
    }

    // A busy shell can keep a turn open for longer than the hold, and an empty commit would replace the edit's.
    void testLongTurnKeepsTheEdit() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        FakeResources           resources;
        ngosen::test::AppQuirks shell{.dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .keepsLastEditPerTurn = true, .turnMs = 45};
        const auto              keys = keysOf("vieetj nam ");
        const auto              run  = ngosen::test::typeInAppTraced(resources, field, shell, keys, std::vector<int>(keys.size(), 200));
        check(run.text == "việt nam ", "a long turn: asking for a report keeps the edit", run.text);

        // A key replayed while that turn is open would replace the edit's commit too.
        FakeResources fastResources;
        shell.turnMs    = 25;
        const auto fast = ngosen::test::typeInAppTraced(fastResources, field, shell, keys, std::vector<int>(keys.size(), 40));
        check(fast.text == "việt nam ", "a long turn: a replayed key waits for the edit to come out", fast.text);
    }

    // The cursor moving over our own edit is not a click, so the word goes on.
    void testCursorMovesBeforeEditText() {
        realMode = ngosen::Mode::Sen;
        FakeResources           resources;
        ngosen::test::AppQuirks slate{.movesCursorBeforeText = true, .textAfterCursor = "chào"};
        const auto              keys = keysOf("nguwx ");
        const auto              run  = ngosen::test::typeInAppTraced(resources, makeField("wayland", true, "firefox"), slate, keys, std::vector<int>(keys.size(), 150));
        check(run.text == "ngữ chào", "cursor shown over the edit first: the word keeps its tone", run.text);

        // A deletion longer than one letter steps the cursor back further than a caret settling.
        FakeResources longResources;
        const auto    longKeys = keysOf("nguoiwf ");
        const auto    longRun  = ngosen::test::typeInAppTraced(longResources, makeField("wayland", true, "firefox"), slate, longKeys, std::vector<int>(longKeys.size(), 150));
        check(longRun.text == "người chào", "cursor shown over a longer edit first: the word keeps its tone", longRun.text);
    }

    // Firefox's cursor stays past the real one after a deletion and a commit in one turn, so the next
    // deletion would take what follows the word.
    void testEditBeforeOtherText() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        FakeResources           resources;
        ngosen::test::AppQuirks firefox{
            .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .keepsLastEditPerTurn = true, .turnMs = 3, .cursorStaysPastEdit = true, .textAfterCursor = "chào"};
        const auto keys = keysOf("nguwx ");
        const auto run  = ngosen::test::typeInAppTraced(resources, field, firefox, keys, std::vector<int>(keys.size(), 150));
        check(run.text == "ngữ chào", "text after the cursor: the deletion and the commit go apart", run.text);

        // GTK asks for a new copy only when a commit arrives, so a wait for the copy has to send one.
        ngosen::test::AppQuirks quiet{.dropsForwardedKeys           = true,
                                      .convertsDeleteOnReportedCopy = true,
                                      .repeatsOldTextAfterDelete    = true,
                                      .keysBeforeNewReport          = 99,
                                      .emptyCommitsBeforeReport     = 1,
                                      .reportsOnlyWhenAsked         = true,
                                      .keepsLastEditPerTurn         = true,
                                      .turnMs                       = 3};
        FakeResources           quietResources;
        const auto              fast  = keysOf("tieengs vieetj laf ");
        const auto              quick = ngosen::test::typeInAppTraced(quietResources, field, quiet, fast, std::vector<int>(fast.size(), 40));
        check(quick.text == "tiếng việt là ", "quiet copy on GNOME: the words come out right when typing fast", quick.text);
    }

    // Ace keeps a line break after its line, and the cursor can stay past it after a commit.
    void testLineBreakPastTheWord() {
        Field shell        = surroundingOnlyField();
        shell.program      = "gnome-shell";
        const auto    keys = keysOf("nguwx cuar");
        FakeResources resources;
        const auto run = ngosen::test::typeInAppTraced(resources, shell, {.dropsForwardedKeys = true, .keepsLineBreakAfterCommit = true}, keys, std::vector<int>(keys.size(), 150));
        check(run.text.rfind("ngữ của", 0) == 0, "a line break past the word stays out of the word", run.text);

        // Where backspaces reach the app, they act at its real cursor, so the copy's says nothing.
        FakeResources plasmaResources;
        const auto    plasma =
            ngosen::test::typeInAppTraced(plasmaResources, makeField("wayland", true, "firefox"), {.keepsLineBreakAfterCommit = true}, keys, std::vector<int>(keys.size(), 150));
        check(plasma.sent.forwardedBackSpaces > 0, "a line break past the word leaves the surrounding path", std::to_string(plasma.sent.forwardedBackSpaces));
    }

    // wlroots drops an edit sent before the done event that follows a report, and the wait for the copy
    // ends on a report.
    void testCopyWaitEndsAfterTheReport() {
        FakeResources resources;
        const auto    keys = keysOf("tieengs vieetj nam ");
        const auto run = ngosen::test::typeInAppTraced(resources, makeField("wayland_v2", true, "firefox"),
                                                       {.reportDelayMs = 30, .dropsCommitsInReport = true, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true}, keys,
                                                       std::vector<int>(keys.size(), 20));
        check(run.text == "tiếng việt nam ", "sway: an edit waiting for the copy goes out after the report", run.text);
    }

    // A copy that comes a moment late is no reason to leave the surrounding path.
    void testLateCopyKeepsSurroundingPath() {
        {
            const Field             field = makeField("wayland", true, "firefox");
            ngosen::test::AppQuirks firefox{.repeatsOldTextAfterDelete = true, .keysBeforeNewReport = 99, .emptyCommitsBeforeReport = 1, .reportsOnlyWhenAsked = true};
            FakeResources           resources;
            const auto              keys = keysOf("tieengs vieetj nguwx ");
            const auto              run  = ngosen::test::typeInAppTraced(resources, field, firefox, keys, std::vector<int>(keys.size(), 40));
            check(run.text == "tiếng việt ngữ ", "quiet copy: the words come out right", run.text);
            check(run.sent.forwardedBackSpaces == 0, "quiet copy: the wait asks for a report instead of leaving the path", std::to_string(run.sent.forwardedBackSpaces));
        }
        {
            // A key replayed right after a commit finds a copy that cannot show that commit yet.
            const Field   field = makeField("wayland", true, "firefox");
            FakeResources resources;
            const auto    keys = keysOf("tieengs vieetj nguwx ");
            const auto    run  = ngosen::test::typeInAppTraced(resources, field, {.reportDelayMs = 60, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true}, keys,
                                                               std::vector<int>(keys.size(), 20));
            check(run.text == "tiếng việt ngữ ", "replayed keys: the words come out right", run.text);
            check(run.sent.forwardedBackSpaces == 0, "replayed keys: the wait covers them too", std::to_string(run.sent.forwardedBackSpaces));
        }
        for (const char* frontend : {"wayland", "wayland_v2"}) {
            const Field   field = makeField(frontend, true, "chromium");
            FakeResources resources;
            const auto    keys = keysOf("tieengs vieetj nguwx ");
            const auto    run  = ngosen::test::typeInAppTraced(resources, field, {.reportDelayMs = 30, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true}, keys,
                                                               std::vector<int>(keys.size(), 10));
            check(run.text == "tiếng việt ngữ ", std::string(frontend) + " late copy: the words come out right", run.text);
            check(run.sent.forwardedBackSpaces <= 1, std::string(frontend) + " late copy: backspaces only before the field has shown text",
                  std::to_string(run.sent.forwardedBackSpaces));
        }
    }

    // A copy one key behind can hold the same number of bytes as the word, yet the deletion lands at
    // that copy's cursor, in front of the key the copy lacks.
    void testCopyBehindByAKey() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks firefox{.reportDelayMs = 40, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true, .deletesAtReportedCursor = true};
        FakeResources           resources;
        const auto              keys = keysOf("vieetj nam ");
        std::vector<int>        gaps(keys.size(), 150);
        gaps[4]        = 5; // t and j
        const auto run = ngosen::test::typeInAppTraced(resources, field, firefox, keys, gaps);
        check(run.text == "việt nam ", "copy behind by a key: the replacement waits for the copy to catch up", run.text);
    }

    // The copy can still end at the word before the keys just replayed, and a deletion counted on it
    // leaves part of the word in place.
    void testCopyBehindReplayedKey() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks firefox{
            .reportDelayMs = 3, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true, .keepsLastEditPerTurn = true, .turnMs = 3};
        const auto keys = keysOf("bef owr vieetj nam ");
        for (const int delay : {60, 80, 100, 120}) {
            firefox.oldTextAfterDeleteMs = delay;
            for (int gap = 5; gap <= 80; gap += 5) {
                // Slow words first, so the app counts as one that reports its edits.
                std::vector<int> gaps(keys.size(), 400);
                gaps[11] = 5;   // the second e and t: t waits behind the edit and is replayed
                gaps[12] = gap; // t and j
                FakeResources resources;
                const auto    run = ngosen::test::typeInAppTraced(resources, field, firefox, keys, gaps);
                check(run.text == "bè ở việt nam ", "copy behind a replayed key, delay " + std::to_string(delay) + ", gap " + std::to_string(gap), run.text);
            }
        }
    }

    // A report request sent right after a replayed key would replace that key's commit while the turn
    // is open, and the word loses the letter.
    void testReportRequestKeepsReplayedKey() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks shell{
            .reportDelayMs = 5, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true, .keepsLastEditPerTurn = true, .turnMs = 15};
        const auto keys = keysOf("vieetj nam ");
        for (int gap = 10; gap <= 100; gap += 5) {
            std::vector<int> gaps(keys.size(), 120);
            gaps[3] = 5;   // the second e and t: t waits behind the edit and is replayed
            gaps[4] = gap; // t and j
            FakeResources resources;
            const auto    run = ngosen::test::typeInAppTraced(resources, field, shell, keys, gaps);
            check(run.text == "việt nam ", "a report request keeps a replayed key, gap " + std::to_string(gap), run.text);
        }
    }

    // Two tone keys can reach us within one GNOME Shell turn, before the app has shown the first
    // replacement; sent at once, the app would keep only the second.
    void testReplacementsInOneTurn() {
        Field field   = surroundingOnlyField();
        field.program = "gnome-shell";
        ngosen::test::AppQuirks shell{.dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .keepsLastEditPerTurn = true};
        const auto              keys = keysOf("nguowfi ddoofng ");
        std::vector<int>        gaps(keys.size(), 100);
        gaps[4]  = 0; // w and f
        gaps[11] = 0; // the second o and f
        FakeResources resources;
        auto          run = ngosen::test::typeInAppTraced(resources, field, shell, keys, gaps);
        check(run.text == "người đồng ", "one turn: a replacement waits until the app shows the one before", run.text);
        // With no report in time the hold's own timer replays the held f, whose replacement holds the
        // keys after it again. The copy is stale by then, so only the timer's handling is checked here.
        shell.reportDelayMs = 30;
        FakeResources fresh;
        const int     destroyedBefore = ngosen::test::timersDestroyedInCallback;
        ngosen::test::captureLog(true);
        ngosen::test::typeInAppTraced(fresh, field, shell, keys, gaps);
        const auto lines = ngosen::test::capturedLog();
        ngosen::test::captureLog(false);
        check(countHaving(lines, "Perform replacement: ơ -> ờ") == 1, "one turn: the hold's timer replays a held tone key", join(lines));
        check(ngosen::test::timersDestroyedInCallback == destroyedBefore, "one turn: the hold's timer is not destroyed from its own callback",
              std::to_string(ngosen::test::timersDestroyedInCallback - destroyedBefore));
    }

    // An empty field reports an empty copy when it gets focus; that is all it can show before the first word.
    void testEmptyFieldFirstWord() {
        Field field           = surroundingOnlyField();
        field.preeditFallback = true;
        for (const auto& quirks : {ngosen::test::AppQuirks{.dropsForwardedKeys = true}, ngosen::test::AppQuirks{.dropsForwardedKeys = true, .reportsEmptyText = true}}) {
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, field, quirks);
            auto*               app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            ngosen::test::captureLog(true);
            state.surroundingUpdated();
            for (char c : std::string("vieetj vieetj ")) {
                app->type(static_cast<uint32_t>(c));
                loop.pump(100);
            }
            loop.pump(300);
            const auto lines = ngosen::test::capturedLog();
            ngosen::test::captureLog(false);
            if (!quirks.reportsEmptyText) {
                check(app->text() == "việt việt " && app->sent().preedits == 0 && countHaving(lines, "Type the word in the preedit") == 0,
                      "empty field: the first word is typed in the field", app->text() + " " + join(lines));
            } else {
                check(isEndsWith(app->text(), " việt ") && countHaving(lines, "Type the word in the preedit: no text since focus") == 1,
                      "empty field: a field that only ever reports empty types its next word in the preedit", app->text() + " " + join(lines));
            }
        }
    }

    // IBus names a client "<toolkit>-im:<program>", and a snap's program repeats the snap name.
    void testIbusClientNames() {
        auto ibus = [](const std::string& program, bool surroundingText = false) { return makeField("ibus", surroundingText, program); };
        auto said = [](bool ok) { return std::string(ok ? "true" : "false"); };
        bool ok   = ngosen::hidesAddressBarFlag(ibus("gtk3-im:firefox_firefox"));
        check(ok, "IBus snap Firefox hides the address bar flag", said(ok));
        ok = ngosen::hidesAddressBarFlag(ibus("gtk3-im:firefox"));
        check(ok, "IBus Firefox hides the address bar flag", said(ok));
        ok = ngosen::typesOverPrimarySelection(ibus("gtk3-im:google-chrome"));
        check(ok, "IBus Chrome types over the primary selection", said(ok));
        ok = ngosen::appliesBackspacesLate(ibus("gtk3-im:soffice"));
        check(ok, "IBus LibreOffice applies backspaces late", said(ok));
        ok = ngosen::appliesBackspacesLate(ibus("gtk3-im:soffice_soffice"));
        check(ok, "IBus snap LibreOffice applies backspaces late", said(ok));
        ok = ngosen::ignoresForwardedKeys(ibus("gtk4-im:python3"));
        check(ok, "IBus GTK4 still ignores forwarded keys", said(ok));
        ok = ngosen::ignoresForwardedKeys(ibus("gtk3-im:firefox"));
        check(!ok, "IBus GTK3 takes forwarded keys", said(ok));
        ok = ngosen::erasesWithCommittedDel(ibus("gtk4-im:ptyxis"));
        check(ok, "IBus Ptyxis erases with a committed DEL", said(ok));
        ok = ngosen::hidesAddressBarFlag(ibus("gtk3-im:gnome-terminal-server"));
        check(!ok, "IBus terminal is not Firefox", said(ok));
        ok = ngosen::hidesAddressBarFlag(ibus("librewolf_librewolf.desktop"));
        check(ok, "a snap's desktop ID is read as its program", said(ok));
        ok = ngosen::hidesAddressBarFlag(ibus("gnome-shell"));
        check(!ok, "GNOME Shell is not Firefox", said(ok));
        ok = ngosen::appliesBackspacesLate(ibus("gtk3-im:soffice_bin"));
        check(!ok, "an underscore that does not repeat the name stays", said(ok));
        ok = ngosen::appliesBackspacesLate(makeField("dbus", true, "soffice_soffice"));
        check(!ok, "snap names are only shortened for IBus clients", said(ok));
    }

    // VS Code reports the cursor at the start of the line and then back at the end of the word,
    // with the text unchanged; the word being typed must survive that.
    void testCursorBounceBack() {
        Field ibusEdition           = surroundingOnlyField();
        ibusEdition.preeditFallback = true;
        for (const auto& [field, quirks] :
             {std::pair{makeField("ibus", true), ngosen::test::AppQuirks{}}, std::pair{ibusEdition, ngosen::test::AppQuirks{.dropsForwardedKeys = true}}}) {
            realMode = ngosen::Mode::Sen;
            realtextLen.store(0);
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, field, quirks);
            auto*               app   = owned.get();
            ngosen::TypingState state(&resources, std::move(owned));
            app->attach(&state);
            const auto typeText = [&](const std::string& text) {
                for (char c : text) {
                    app->type(static_cast<uint32_t>(c));
                    loop.pump(100);
                }
            };
            typeText("xin v");
            loop.pump(300);
            app->moveCursor(0);
            loop.pump(30);
            app->moveCursor(5);
            loop.pump(30);
            typeText("ieetj ");
            loop.pump(300);
            check(app->text() == "xin việt ", "bounce: the cursor coming back to the word end keeps the word",
                  std::string(field.preeditFallback ? "IBus edition " : "fcitx5 ") + app->text());

            // A move that ends elsewhere is a click: the word before it is gone.
            ngosen::test::captureLog(true);
            typeText("v");
            loop.pump(300);
            app->moveCursor(0);
            loop.pump(30);
            app->moveCursor(3);
            loop.pump(30);
            typeText("ieetj");
            loop.pump(300);
            const auto lines = ngosen::test::capturedLog();
            ngosen::test::captureLog(false);
            check(app->text() == "xiniệt việt v" && countHaving(lines, "Cursor came back to the word") == 0, "bounce: a move that ends elsewhere still forgets the word",
                  std::string(field.preeditFallback ? "IBus edition " : "fcitx5 ") + app->text() + " " + join(lines));
        }
    }

    // VS Code's editor shows the cursor at the line start for a moment after a line's first letter.
    void testFirstLetterCursorBounce() {
        for (const int lateMs : {20, 36, 46}) {
            for (const bool comesBack : {true, false}) {
                realMode = ngosen::Mode::Sen;
                realtextLen.store(0);
                FakeLoop            loop;
                FakeResources       resources;
                auto                owned = std::make_unique<ngosen::test::FakeApp>(loop, makeField("ibus", true), ngosen::test::AppQuirks{});
                auto*               app   = owned.get();
                ngosen::TypingState state(&resources, std::move(owned));
                app->attach(&state);
                app->type('t');
                loop.pump(lateMs);
                app->copyTextAfterStartCursor();
                if (comesBack) {
                    loop.pump(2);
                    app->moveCursor(1);
                }
                loop.pump(250);
                for (const char c : std::string(comesBack ? "ieengs vieetj " : "ieetj")) {
                    app->type(static_cast<uint32_t>(c));
                    loop.pump(250);
                }
                if (comesBack)
                    check(app->text() == "tiếng việt t", "first letter: the cursor coming back keeps the word", std::to_string(lateMs) + " ms: " + app->text());
                else if (lateMs > 30)
                    check(app->text() == "iệttt", "first letter: a cursor staying at the line start forgets the word", std::to_string(lateMs) + " ms: " + app->text());
            }
        }
    }

    // The ways out when a deletion through the surrounding text goes wrong.
    void testSurroundingFallback() {
        ngosen::test::Typed run;
        const Field         gnome = makeField("ibus", true);
        auto                lines = logLinesWhileTyping(gnome, {}, "vieetj nam ", "Path changed", &run);
        auto                off   = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 2, .forwardedBackSpaces = 0});
        check(run.text == "việt nam " && off.empty() && lines.size() == 1, "fallback: a GNOME field deletes through its surrounding text",
              run.text + " " + join(off) + join(lines));

        // Both ways: the dropped deletion is fixed with backspaces, which then take over.
        lines = logLinesWhileTyping(gnome, {.ignoresSurroundingDel = true}, "too too ", "", &run);
        off   = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 1});
        check(run.text == "tô tô " && off.empty() && countHaving(lines, "Surrounding text untrusted") == 1 && countHaving(lines, "Fix the dropped deletion") == 1,
              "fallback: a dropped deletion is fixed and backspaces take over", run.text + " " + join(off) + join(lines));

        // Keys typed during a wait are replayed; one that replaces through the surrounding text must not
        // lose the words replayed after it.
        for (const auto& quirks : {ngosen::test::AppQuirks{.staleReport = true, .convertsDeleteOnReportedCopy = true},
                                   ngosen::test::AppQuirks{.staleReport = true, .lagMs = 40, .convertsDeleteOnReportedCopy = true}}) {
            lines = logLinesWhileTyping(gnome, quirks, "nguyeenx vieetj tieesng ", "", &run, 20);
            check(run.text == "nguyễn việt tiếng ", "fallback: words replayed after a surrounding replacement stay whole", run.text + " " + join(lines));
        }

        // No other way and no preedit: keys go to the app as they are.
        const ngosen::test::AppQuirks dropsAll{.dropsForwardedKeys = true, .ignoresSurroundingDel = true};
        lines = logLinesWhileTyping(surroundingOnlyField(), dropsAll, "too too ", "", &run);
        off   = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 1, .preedits = 0});
        check(run.text == "toô too " && off.empty() && countHaving(lines, "type keys as they are") == 1, "fallback: a field with no way out types raw keys",
              run.text + " " + join(off));

        // The mark holds for the app's next field too.
        {
            FakeResources resources;
            const auto    first = keysOf("too ");
            ngosen::test::typeInAppTraced(resources, surroundingOnlyField(), dropsAll, first, std::vector<int>(first.size(), 100));
            const auto next = keysOf("vieetj ");
            run             = ngosen::test::typeInAppTraced(resources, surroundingOnlyField(), dropsAll, next, std::vector<int>(next.size(), 100));
            off             = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 0});
            check(run.text == "vieetj " && off.empty(), "fallback: the mark holds for the next field of the app", run.text + " " + join(off));
        }

        Field preeditHost           = surroundingOnlyField();
        preeditHost.preeditFallback = true;
        lines                       = logLinesWhileTyping(preeditHost, dropsAll, "too too too ", "", &run);
        off                         = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 1});
        check(run.text == "tô toô tô " && off.empty() && run.sent.preedits > 0 && countHaving(lines, "Type the word in the preedit: marked untrusted") == 1,
              "fallback: a host with a preedit types the next word there", run.text + " " + join(off) + join(lines));

        // A mark that comes mid-word leaves the rest of that word raw.
        lines = logLinesWhileTyping(preeditHost, dropsAll, "too toos too ", "", &run);
        off   = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 1});
        check(run.text == "tô toôs tô " && off.empty() && countHaving(lines, "Keep the word as typed") == 1, "fallback: the rest of a word goes raw after a mark",
              run.text + " " + join(off) + join(lines));

        // Decided at the first key of each word.
        lines = logLinesWhileTyping(preeditHost, {.dropsForwardedKeys = true}, "vieetj vieetj ", "", &run);
        off   = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 2});
        check(run.text == "việt việt " && off.empty() && countHaving(lines, "Type the word in the preedit: no text since focus") == 1,
              "fallback: the first word goes to the preedit until the field reports text", run.text + " " + join(off) + join(lines));

        Field urlField = preeditHost;
        urlField.url   = true;
        run = ngosen::test::typeInAppTraced(*std::make_unique<FakeResources>(), urlField, {.dropsForwardedKeys = true}, keysOf("vieetj vieetj "), std::vector<int>(14, 100));
        off = ngosen::test::pathMismatches(run.sent, {.surroundingDeletes = 0});
        check(run.text == "việt việt " && off.empty(), "fallback: an address bar types every word in the preedit", run.text + " " + join(off));
    }
} // namespace

namespace {
    void testEnglishWordKeepsDoubledLetter() {
        const auto typed = [](const std::string& word, uint32_t end) {
            std::vector<uint32_t> keys(word.begin(), word.end());
            keys.push_back(end);
            return keys;
        };
        ngosen::test::FakeResources resources;
        const Field                 field = makeField("wayland", true);
        for (const uint32_t end : {uint32_t{ngosen::key::space}, uint32_t{ngosen::key::Return}}) {
            const auto        keys = typed("issue", end);
            const std::string sen  = ngosen::test::typeInApp(resources, field, {}, keys, std::vector<int>(keys.size(), 20));
            check(sen.rfind("issue", 0) == 0, "sen: issue keeps both letters before key " + std::to_string(end), sen);
            const std::string preedit = ngosen::test::telexOutput(resources, keys);
            check(preedit.rfind("issue", 0) == 0, "preedit: issue keeps both letters before key " + std::to_string(end), preedit);
        }
        ngosen::test::FakeResources noMacros;
        noMacros.mutableOptions().enableMacro = false;
        const auto        enterKeys           = typed("issue", ngosen::key::Return);
        const std::string senNoMacros         = ngosen::test::typeInApp(noMacros, field, {}, enterKeys, std::vector<int>(enterKeys.size(), 20));
        check(senNoMacros.rfind("issue", 0) == 0, "sen: issue keeps both letters before Enter with macros off", senNoMacros);
        const std::string tab = ngosen::test::telexOutput(resources, typed("issue", ngosen::key::Tab));
        check(tab.rfind("issue", 0) == 0, "preedit: issue keeps both letters before Tab", tab);
        const auto        keys = typed("tesst", ngosen::key::space);
        const std::string sen  = ngosen::test::typeInApp(resources, field, {}, keys, std::vector<int>(keys.size(), 20));
        check(sen == "test ", "sen: a doubled key still undoes the tone", sen);
    }
}

int main() {
    testEnglishWordKeepsDoubledLetter();
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
    for (const char* frontend : {"wayland", "wayland_v2", "dbus"})
        testSenDeletesReportedWord(frontend);
    testFirefoxDeletesThroughSurrounding();
    testForwardedKeysNewerThanTyped();
    testKeyDuringReplacement();
    testCommitAfterTheReport();
    testCommitAfterFieldRefocus();
    testKeyTypedBeforeCommitArrived();
    testGnomeKeyDuringTimedWaitJoinsCommit();
    testEnterDuringTimedWaitFollowsCommit();
    testBackspaceDuringTimedWaitFollowsCommit();
    testHeldSpaceCountsForDoubleSpace();
    testHeldSpaceReopensWord();
    testCommitAfterEditorRedraw();
    testQuickSwitchesNoRedrawWait();
    testMacroKeepsSpace();
    testMacroBeforeEnter();
    testEnterDuringReplacement();
    testLockWord();
    testLockWordPreedit();
    testSlowBackspaceOverIbus();
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
    testSilentFieldGetsBackspaces();
    testRefocusKeepsReport(true);
    testRefocusKeepsReport(false);
    testSdlGetsNoForwardedBackspace();
    testTerminalErasesWithCommittedDel();
    testSenWithoutSentKeys();
    testRecorderKeepsTheReplacement();
    testRecorderNotesFieldSwitch();
    testRecorderHidesPasswords();
    testRecorderKeepsOneEventPerLine();
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
    testPreeditBackspaceReopensWord();
    testModifierTapKeepsWord();
    testSurroundingQuirksBreakText();
    testSuggestion();
    testPathCheck();
    testSurroundingCheck();
    testSurroundingFallback();
    testEmptyFieldFirstWord();
    testSlowReport();
    testStaleCopyBeforeReplacement();
    testReplacementsInOneTurn();
    testFirefoxReportsOnlyAfterCommit();
    testReportRequestKeepsNextEdit();
    testAnsweringAppGetsPlainCount();
    testSplitTimerSurvivesReplay();
    testSplitOutlastsFocusBounce();
    testAutofillTailCommitsWithDeletion();
    testSelectedTailGoesFirst();
    testReportRequestIsNotInput();
    testNoCopyWaitForBackspaceField();
    testCopyWaitEndsOnInvalidReport();
    testCopyBehindByAKey();
    testCopyBehindReplayedKey();
    testReportRequestKeepsReplayedKey();
    testLateCopyKeepsSurroundingPath();
    testFirefoxOnPlasmaDeletesThroughSurrounding();
    testLineBreakPastTheWord();
    testLongTurnKeepsTheEdit();
    testCopyWaitEndsAfterTheReport();
    testEditBeforeOtherText();
    testCursorMovesBeforeEditText();
    testCursorBounceBack();
    testFirstLetterCursorBounce();
    testIbusClientNames();
    testAutofillMark();
    testReportWithNextKey();
    testSilentFromFirstDeletion();
    testBothPathsInOneWord();
    testBackspacePathDuringLongCheck();
    testLateReportAfterBackspace();
    testDroppedDeletionInLongCheck();
    testEditsDuringLongCheck();
    testLongCheckLeavesFocus();
    testUnansweredChecksLeaveNoMark();
    check(ngosen::test::timersDestroyedInCallback == 0, "no timer is destroyed from its own callback", std::to_string(ngosen::test::timersDestroyedInCallback));
    if (failures == 0)
        std::cout << "all core typing checks passed\n";
    return failures == 0 ? 0 : 1;
}
