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
    void testFirefoxKeepsBackspaces() {
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
            check(host->deletes().empty() && host->forwarded().size() == 2, std::string("firefox ") + frontend + ": backspaces, no surrounding deletion",
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

        // A forwarded tap would lose the Shift, so the line break stays in the commit.
        const std::vector<uint32_t> shifted{'n', 'h', 'e', 's', ngosen::key::Return | ngosen::test::WithShift, 'a'};
        std::string                 lineBreak = ngosen::test::typeInApp(makeField("wayland", true, "libreoffice-calc"), quirks, shifted, gaps);
        std::replace(lineBreak.begin(), lineBreak.end(), '\r', '\n');
        check(lineBreak == "nhé\na", "Enter during replacement: Shift+Enter", lineBreak);

        // A dbus client gets the commit after the reply to the key, so a passed Enter would land first.
        Field dbus                = makeField("dbus", false);
        dbus.keyEventOrderFix     = true;
        const std::string ordered = ngosen::test::typeInApp(dbus, quirks, keys, gaps);
        check(ordered == "nhé\na", "Enter during replacement: dbus", ordered);
        std::string shiftedOnDbus = ngosen::test::typeInApp(dbus, quirks, shifted, gaps);
        std::replace(shiftedOnDbus.begin(), shiftedOnDbus.end(), '\r', '\n');
        check(shiftedOnDbus == "nhé\na", "Enter during replacement: dbus Shift+Enter", shiftedOnDbus);

        std::vector<uint32_t> shiftedMacro = macro;
        shiftedMacro[7] |= ngosen::test::WithShift;
        std::string macroLineBreak = ngosen::test::typeInApp(resources, makeField("wayland", true, "libreoffice-calc"), quirks, shiftedMacro, macroGaps);
        std::replace(macroLineBreak.begin(), macroLineBreak.end(), '\r', '\n');
        check(macroLineBreak == "nhé được\na", "Enter during replacement: Shift+Enter after a macro", macroLineBreak);
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
        const ngosen::test::AppQuirks qt{.dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .reportsNothingAfterDelete = true};
        const auto                    lines = logLinesWhileTyping(field, qt, "nam tieesng ", "", &run, 150);
        check(run.text == "nam tiếng " && countHaving(lines, "Keep the word as typed") == 0 && countHaving(lines, "copy shows the earlier word") > 0,
              "stale copy: a replacement after a replacement deletes on the older copy", run.text + " " + join(lines));
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
    for (const char* frontend : {"wayland", "wayland_v2", "dbus"})
        testSenDeletesReportedWord(frontend);
    testFirefoxKeepsBackspaces();
    testForwardedKeysNewerThanTyped();
    testKeyDuringReplacement();
    testCommitAfterTheReport();
    testCommitAfterFieldRefocus();
    testKeyTypedBeforeCommitArrived();
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
    testCursorBounceBack();
    testIbusClientNames();
    testAutofillMark();
    if (failures == 0)
        std::cout << "all core typing checks passed\n";
    return failures == 0 ? 0 : 1;
}
