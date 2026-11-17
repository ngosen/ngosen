/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Emoji offered for the words just typed, and the keys that pick one (#110).
#include "fake-app.h"
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-state.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using ngosen::Field;
using ngosen::Mode;
using ngosen::test::AppQuirks;
using ngosen::test::FakeApp;
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

    constexpr uint32_t Tab     = ngosen::key::Tab;
    constexpr uint32_t Esc     = ngosen::key::Escape;
    constexpr uint32_t Enter   = ngosen::key::Return;
    constexpr uint32_t Click   = 0;
    constexpr uint32_t Hint    = 1; // records the panel's labels
    constexpr uint32_t OffMode = 2;
    constexpr uint32_t Status  = 3; // records the line under the panel
    constexpr uint32_t AppOff  = 4; // the user turns emoji off for this app
    constexpr uint32_t Reset   = 5; // the app resets the input context, as Firefox does after each key it types
    constexpr uint32_t Home    = 6; // the cursor moves to the start of the field
    constexpr uint32_t Lag     = 7; // the app reports its text 5 ms after each change
    // Firefox resets right after the key and reports the text it typed only after that.
    constexpr uint32_t EarlyReset = 8;

    Field              makeField() {
        Field field;
        field.frontend        = "wayland";
        field.surroundingText = true;
        field.preedit         = true;
        field.drawsCandidates = true;
        return field;
    }

    std::vector<uint32_t> keysOf(const std::string& typed) {
        return {typed.begin(), typed.end()};
    }

    std::vector<uint32_t> operator+(std::vector<uint32_t> a, const std::vector<uint32_t>& b) {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    struct Result {
        std::string              text;
        std::vector<std::string> hints; // the first label at each Hint
        std::vector<std::string> statuses;
    };

    Result type(const std::vector<uint32_t>& keys, Mode mode = Mode::Sen, Field field = makeField(), const std::function<void(FakeResources&)>& configure = {}) {
        FakeResources resources;
        if (configure)
            configure(resources);
        realMode = mode;
        realtextLen.store(0);
        FakeLoop            loop;
        auto                owned = std::make_unique<FakeApp>(loop, field, AppQuirks{.showsEnter = true, .showsTab = true});
        FakeApp*            app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        Result result;
        for (size_t i = 0; i < keys.size(); ++i) {
            const uint32_t key = keys[i];
            if (key == Click) {
                g_mouse_clicked.store(true);
            } else if (key == OffMode) {
                realMode = Mode::Off;
            } else if (key == Reset || key == EarlyReset) {
                // The engine passes a reset on only while no word is open.
                if (state.isEmptyHistory())
                    state.appReset();
                if (key == EarlyReset)
                    loop.pump(20);
            } else if (key == Lag) {
                app->setReportDelay(5);
            } else if (key == Home) {
                app->moveCursor(0);
            } else if (key == AppOff) {
                state.setEmojiOn(false);
            } else if (key == Status) {
                result.statuses.push_back(app->status());
            } else if (key == Hint) {
                result.hints.push_back(app->labels().empty() ? "(none)" : app->labels().front());
            } else {
                app->type(key);
                if (i + 1 == keys.size() || keys[i + 1] != EarlyReset)
                    loop.pump(20);
            }
        }
        loop.pump(200);
        result.text = app->text();
        return result;
    }

    std::string hintAfter(const std::string& typed, Mode mode = Mode::Sen, Field field = makeField(), const std::function<void(FakeResources&)>& configure = {}) {
        return type(keysOf(typed) + std::vector<uint32_t>{Hint}, mode, field, configure).hints.front();
    }

    void testHint() {
        auto hint = hintAfter("cuwowif");
        check(hint == "Tab: 😂 🤣 😄 😆 😁", "a keyword shows its emoji", hint);
        hint = hintAfter("cuwowif ");
        check(hint == "(none)", "the hint goes once the word is over", hint);
        hint = hintAfter("cuwowi");
        check(hint == "(none)", "no hint before the word matches", hint);
        hint = hintAfter("camr own");
        check(hint == "Tab: 🙏 😊", "a keyword of two words", hint);
        hint = type(keysOf("camr") + std::vector<uint32_t>{Click} + keysOf(" own") + std::vector<uint32_t>{Hint}).hints.front();
        check(hint == "(none)", "a click starts the words over", hint);
        hint = hintAfter(":)");
        check(hint == "Tab: 🙂 😊", "an emoticon outside a word", hint);
        hint = type(std::vector<uint32_t>{':', Reset, ')', Reset, Hint}).hints.front();
        check(hint == "Tab: 🙂 😊", "an emoticon outlasts the app's reset after each key", hint);
        hint = type(std::vector<uint32_t>{'=', Reset, ')', Reset, ')', Reset, Hint}).hints.front();
        check(hint.rfind("Tab: 🤣", 0) == 0, "=)) outlasts the app's resets", hint);
        hint = type(std::vector<uint32_t>{':', Home, Reset, ')', Reset, Hint}).hints.front();
        check(hint == "(none)", "a reset after the cursor moved drops the emoticon typed before", hint);
        hint = type(std::vector<uint32_t>{Lag, ':', EarlyReset, ')', EarlyReset, Hint}).hints.front();
        check(hint == "Tab: 🙂 😊", "an emoticon outlasts a reset that comes before the app reports the key", hint);
        hint = type(std::vector<uint32_t>{Lag, '=', EarlyReset, ')', EarlyReset, ')', EarlyReset, Hint}).hints.front();
        check(hint.rfind("Tab: 🤣", 0) == 0, "=)) outlasts resets that come before the app reports", hint);
        hint = type(std::vector<uint32_t>{Lag, ':', Home, EarlyReset, ')', EarlyReset, Hint}).hints.front();
        check(hint == "(none)", "a reset before the report of a moved cursor drops the emoticon", hint);
        hint = hintAfter("=))");
        check(hint == "Tab: 🤣", "a Yahoo Messenger code typed in Telex", hint);
        hint = hintAfter("cuwowif", Mode::Preedit);
        check(hint == "Tab: 😂 🤣 😄 😆 😁", "the preedit shows the hint too", hint);
    }

    void testWhereNoHint() {
        Field unknownPanel           = makeField();
        unknownPanel.drawsCandidates = false;
        check(hintAfter("cuwowif", Mode::Sen, unknownPanel) == "(none)", "no hint where the panel may not show it", "hint");
        Field password    = makeField();
        password.password = true;
        check(hintAfter("cuwowif", Mode::Sen, password) == "(none)", "no hint in a password field", "hint");
        Field terminal    = makeField();
        terminal.terminal = true;
        check(hintAfter("cuwowif", Mode::Sen, terminal) == "(none)", "no hint in a terminal", "hint");
        Field url = makeField();
        url.url   = true;
        check(hintAfter("cuwowif", Mode::Sen, url) == "(none)", "no hint in an address bar", "hint");
        const auto off = hintAfter("cuwowif", Mode::Sen, makeField(), [](FakeResources& r) { r.mutableOptions().emojiSuggestions = false; });
        check(off == "(none)", "no hint with the setting off", off);
        const auto appOff = type(std::vector<uint32_t>{AppOff} + keysOf("cuwowif") + std::vector<uint32_t>{Hint, Tab});
        check(appOff.hints.front() == "(none)" && appOff.text == "cười⇥", "no hint in an app with emoji turned off", appOff.hints.front() + " / " + appOff.text);
        const auto turnedOff = type(keysOf("cuwowif") + std::vector<uint32_t>{AppOff, Hint, Tab});
        check(turnedOff.hints.front() == "(none)" && turnedOff.text == "cười⇥", "turning emoji off drops the hint already shown", turnedOff.hints.front() + " / " + turnedOff.text);
        const auto noTab = type(keysOf("cuwowif") + std::vector<uint32_t>{Tab}, Mode::Sen, unknownPanel).text;
        check(noTab == "cười⇥", "Tab reaches the app where no hint shows", noTab);
    }

    void testPicking() {
        const std::vector<uint32_t> word = keysOf("cuwowif");
        auto                        r    = type(word + std::vector<uint32_t>{Tab, Hint});
        check(r.hints.front() == "1. 😂", "Tab numbers the emoji", r.hints.front());
        r = type(word + std::vector<uint32_t>{Tab, '2'});
        check(r.text == "cười 🤣", "a number picks that emoji and keeps the word", r.text);
        r = type(word + std::vector<uint32_t>{Tab, Tab});
        check(r.text == "cười 😂", "Tab again picks the first", r.text);
        r = type(word + std::vector<uint32_t>{Tab, ' '});
        check(r.text == "cười 😂", "Space picks the first", r.text);
        r = type(word + std::vector<uint32_t>{Tab, Enter});
        check(r.text == "cười 😂", "Enter picks the first and stays out of the app", r.text);
        r = type(word + std::vector<uint32_t>{Tab, Esc, Hint, ' '});
        check(r.text == "cười " && r.hints.front() == "(none)", "Esc keeps the word", r.text + " / " + r.hints.front());
        r = type(word + std::vector<uint32_t>{Tab, ','});
        check(r.text == "cười,", "another key closes the list and is typed", r.text);
        r = type(word + std::vector<uint32_t>{'1'});
        check(r.text == "cười1", "a number before Tab is typed", r.text);
        r = type(word + std::vector<uint32_t>{Tab, Click, ' '});
        check(r.text == "cười ", "a click closes the list, so Space after it is typed", r.text);
        r = type(word + std::vector<uint32_t>{Tab, OffMode, '1'});
        check(r.text == "cười1", "switching to English closes the list", r.text);
        r = type(word + std::vector<uint32_t>{Status, Tab, Status, '2', Status});
        check(r.statuses == std::vector<std::string>{"", "1–5: pick, Space or Enter: the first, Esc: keep the word", ""}, "the keys are named while the list is open",
              r.statuses[0] + " / " + r.statuses[1] + " / " + r.statuses[2]);
        r = type(keysOf("camr own") + std::vector<uint32_t>{Tab, Status, Esc, Status});
        check(r.statuses == std::vector<std::string>{"1–2: pick, Space or Enter: the first, Esc: keep the word", ""}, "the line counts the emoji and goes with Esc",
              r.statuses[0] + " / " + r.statuses[1]);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Tab, Tab, ' ', 'a'});
        check(r.text == "con mèo 🐈 a", "typing goes on after the emoji", r.text);
    }

    void testReplacing() {
        auto r = type(keysOf("hahaha") + std::vector<uint32_t>{Tab, Tab});
        check(r.text == "😂", "a sound is replaced", r.text);
        r = type(keysOf("hi :)") + std::vector<uint32_t>{Tab, Tab});
        check(r.text == "hi 🙂", "an emoticon is replaced", r.text);
        r = type(keysOf("hahaha") + std::vector<uint32_t>{Tab, Tab}, Mode::Preedit);
        check(r.text == "😂", "a sound in the preedit is replaced", r.text);
        r = type(keysOf("cuwowif") + std::vector<uint32_t>{Tab, Tab}, Mode::Preedit);
        check(r.text == "cười 😂", "a word in the preedit is kept", r.text);
    }

    void testReplacingWords() {
        const auto replace = [](FakeResources& r) { r.mutableOptions().emojiReplacesWord = true; };
        auto       r       = type(keysOf("cuwowif") + std::vector<uint32_t>{Tab, Tab}, Mode::Sen, makeField(), replace);
        check(r.text == "😂", "the setting replaces the word", r.text);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Tab, Tab}, Mode::Sen, makeField(), replace);
        check(r.text == "con 🐈", "only the keyword goes", r.text);
        r = type(keysOf("mays bay") + std::vector<uint32_t>{Tab, Tab}, Mode::Sen, makeField(), replace);
        check(r.text == "✈️", "a keyword of two words is replaced whole", r.text);
        r = type(keysOf("mays  bay") + std::vector<uint32_t>{Tab, Tab}, Mode::Sen, makeField(), replace);
        check(r.text == "✈️", "the spaces between its words go too", r.text);
        r = type(keysOf("mays  bay") + std::vector<uint32_t>{Tab, Tab}, Mode::Preedit, makeField(), replace);
        check(r.text == "✈️", "and in the preedit", r.text);
        r = type(keysOf("cuwowif") + std::vector<uint32_t>{Tab, Tab}, Mode::Preedit, makeField(), replace);
        check(r.text == "😂", "a word in the preedit is replaced", r.text);
        r = type(keysOf("mays bay") + std::vector<uint32_t>{Tab, Tab}, Mode::Preedit, makeField(), replace);
        check(r.text == "✈️", "the words before the preedit go too", r.text);
        r = type(keysOf("ddi mays bay") + std::vector<uint32_t>{Tab, Tab, ' ', 'a'}, Mode::Sen, makeField(), replace);
        check(r.text == "đi ✈️ a", "words before the keyword stay", r.text);
    }

    // In a slow app the tone key's replacement is still on its way, and a pick then would land inside the word.
    void testSlowApp() {
        FakeResources resources;
        realMode = Mode::Sen;
        FakeLoop            loop;
        auto                owned = std::make_unique<FakeApp>(loop, makeField(), AppQuirks{.lagMs = 80});
        FakeApp*            app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (const char c : std::string("cuwowif"))
            app->type(static_cast<uint32_t>(c));
        const bool early = !app->labels().empty();
        loop.pump(400);
        const std::string later = app->labels().empty() ? "(none)" : app->labels().front();
        check(!early, "no hint while the word is still being replaced", "hint shown");
        check(later == "Tab: 😂 🤣 😄 😆 😁", "the hint comes once the app shows the word", later + ", text " + app->text());
    }

    // The release of a key the list took stays away from the app as well.
    void testRelease() {
        FakeResources resources;
        realMode = Mode::Sen;
        FakeLoop            loop;
        auto                owned = std::make_unique<FakeApp>(loop, makeField(), AppQuirks{});
        FakeApp*            app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (const char c : std::string("cuwowif")) {
            app->type(static_cast<uint32_t>(c));
            loop.pump(20);
        }
        ngosen::test::FakeKey down(Tab);
        state.keyEvent(down);
        ngosen::test::FakeKey up(Tab, true);
        state.keyEvent(up);
        check(down.accepted() && up.accepted(), "Tab's press and release both stay away from the app", up.accepted() ? "press passed" : "release passed");

        // A number pressed before Tab is let go.
        for (const char c : std::string(" cuwowif")) {
            app->type(static_cast<uint32_t>(c));
            loop.pump(20);
        }
        ngosen::test::FakeKey tabDown(Tab), oneDown('1'), tabUp(Tab, true), oneUp('1', true);
        for (auto* key : {&tabDown, &oneDown, &tabUp, &oneUp})
            state.keyEvent(*key);
        check(tabUp.accepted() && oneUp.accepted(), "both releases stay away when keys overlap", tabUp.accepted() ? "1 released to the app" : "Tab released to the app");
    }

} // namespace

int main() {
    testHint();
    testWhereNoHint();
    testPicking();
    testReplacing();
    testReplacingWords();
    testSlowApp();
    testRelease();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
