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

    constexpr uint32_t Tab      = ngosen::key::Tab;
    constexpr uint32_t Esc      = ngosen::key::Escape;
    constexpr uint32_t Enter    = ngosen::key::Return;
    constexpr uint32_t Click    = 0;
    constexpr uint32_t Hint     = 1; // records the panel's labels
    constexpr uint32_t OffMode  = 2;
    constexpr uint32_t Status   = 3; // records the line under the panel
    constexpr uint32_t AppOff   = 4; // the user turns emoji off for this app
    constexpr uint32_t InField  = 5; // records the hint shown in the field
    constexpr uint32_t Pause    = 6; // the user stops typing for a second
    constexpr uint32_t SlowHint = 7; // the app shows a change of the hint 30 ms late
    constexpr uint32_t FocusOut = 8;

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
        std::vector<std::string> inField;
        int                      editsUnderHint = 0;
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
        for (const uint32_t key : keys) {
            if (key == Click) {
                g_mouse_clicked.store(true);
            } else if (key == OffMode) {
                realMode = Mode::Off;
            } else if (key == AppOff) {
                state.setEmojiOn(false);
            } else if (key == FocusOut) {
                state.reset(true);
            } else if (key == SlowHint) {
                app->setHintLag(30);
            } else if (key == Pause) {
                loop.pump(1000);
            } else if (key == InField) {
                result.inField.push_back(app->hint());
            } else if (key == Status) {
                result.statuses.push_back(app->status());
            } else if (key == Hint) {
                result.hints.push_back(app->labels().empty() ? "(none)" : app->labels().front());
            } else {
                app->type(key);
                loop.pump(20);
            }
        }
        loop.pump(200);
        result.text           = app->text();
        result.editsUnderHint = app->editsUnderHint();
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
        hint = hintAfter("=))");
        check(hint == "Tab: 🤣", "a Yahoo Messenger code typed in Telex", hint);
        hint = hintAfter("cuwowif", Mode::Preedit);
        check(hint == "Tab: 😂 🤣 😄 😆 😁", "the preedit shows the hint too", hint);
    }

    void testWhereNoHint() {
        // Nor can the field show it.
        Field unknownPanel           = makeField();
        unknownPanel.drawsCandidates = false;
        unknownPanel.preedit         = false;
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

    // Where no panel can show the list, as for apps GNOME Shell relays without the Input Method Panel extension.
    void testHintInField() {
        Field noPanel                    = makeField();
        noPanel.drawsCandidates          = false;
        const std::vector<uint32_t> word = keysOf("cuwowif");
        auto                        r    = type(word + std::vector<uint32_t>{InField}, Mode::Sen, noPanel);
        check(r.inField.front().empty(), "no panel: nothing shows while typing", r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, InField, Hint}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 😂 🤣 😄 😆 😁" && r.hints.front() == "(none)", "no panel: a pause shows the hint in the field",
              r.inField.front() + " / " + r.hints.front());
        check(r.text == "cười", "no panel: the hint is never committed", r.text);
        r = type(word + std::vector<uint32_t>{Tab}, Mode::Sen, noPanel);
        check(r.text == "cười⇥", "no panel: Tab before the hint shows reaches the app", r.text);
        r = type(word + std::vector<uint32_t>{Pause, Tab, InField}, Mode::Sen, noPanel);
        check(r.inField.front() == " 1.😂 2.🤣 3.😄 4.😆 5.😁", "no panel: Tab numbers the emoji in the field", r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, Tab, '2', InField}, Mode::Sen, noPanel);
        check(r.text == "cười 🤣" && r.inField.front().empty(), "no panel: a number picks that emoji", r.text + " / " + r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, Tab, Tab}, Mode::Sen, noPanel);
        check(r.text == "cười 😂", "no panel: Tab again picks the first", r.text);
        r = type(word + std::vector<uint32_t>{Pause, Tab, Esc, InField}, Mode::Sen, noPanel);
        check(r.text == "cười" && r.inField.front().empty(), "no panel: Esc keeps the word", r.text + " / " + r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, ' ', InField}, Mode::Sen, noPanel);
        check(r.text == "cười " && r.inField.front().empty(), "no panel: a key takes the hint away", r.text + " / " + r.inField.front());
        r = type(word + std::vector<uint32_t>{' ', Pause, Tab, Tab}, Mode::Sen, noPanel);
        check(r.text == "cười 😂", "no panel: after a space the emoji goes at the cursor", r.text);
        r = type(std::vector<uint32_t>{SlowHint} + word + std::vector<uint32_t>{Pause, 's', Pause, InField}, Mode::Sen, noPanel);
        check(r.text == "cưới" && r.inField.front().empty() && r.editsUnderHint == 0, "no panel: the hint goes before a key edits the field",
              r.text + " / " + std::to_string(r.editsUnderHint));
        r = type(std::vector<uint32_t>{SlowHint} + keysOf("cua") + std::vector<uint32_t>{Pause, 'r', ' '}, Mode::Sen, noPanel);
        check(r.text == "của " && r.editsUnderHint == 0, "no panel: a tone key after the hint keeps the word", r.text + " / " + std::to_string(r.editsUnderHint));
        r = type(std::vector<uint32_t>{SlowHint} + keysOf("hahaha") + std::vector<uint32_t>{Pause, Tab, Tab}, Mode::Sen, noPanel);
        check(r.text == "😂" && r.editsUnderHint == 0, "no panel: a sound at the end is replaced once the hint is gone", r.text + " / " + std::to_string(r.editsUnderHint));
        r = type(std::vector<uint32_t>{SlowHint} + keysOf("hahaha") + std::vector<uint32_t>{Pause, Tab, Tab} + keysOf(" vui"), Mode::Sen, noPanel);
        check(r.text == "😂 vui", "no panel: keys typed after the pick come after the emoji", r.text);
        r = type(std::vector<uint32_t>{SlowHint} + keysOf("hahaha") + std::vector<uint32_t>{Pause, Tab, Tab, FocusOut, Pause} + keysOf(" a"), Mode::Sen, noPanel);
        check(r.text == "hahaha a", "no panel: an emoji still waiting for the hint to go is dropped on focus out", r.text);
        r = type(word + std::vector<uint32_t>{Pause, Enter, InField}, Mode::Sen, noPanel);
        check(r.text == "cười\n" && r.inField.front().empty(), "no panel: Enter goes to the app without the hint", r.text + " / " + r.inField.front());

        // The whole sentence: the keyword nearest the cursor first, the emoji added at the cursor.
        r = type(keysOf("con meof cuwowif quas") + std::vector<uint32_t>{Pause, InField}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 😂 🤣 😄 🐈 🐱", "no panel: a pause offers the emoji of the whole sentence", r.inField.front());
        r = type(keysOf("con meof cuwowif quas") + std::vector<uint32_t>{Pause, Tab, '4'}, Mode::Sen, noPanel);
        check(r.text == "con mèo cười quá 🐈", "no panel: an emoji of an earlier word goes at the cursor", r.text);
        r = type(keysOf("con meof. cuwowif") + std::vector<uint32_t>{Pause, InField}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 😂 🤣 😄 😆 😁", "no panel: an earlier sentence offers nothing", r.inField.front());
        r = type(keysOf("meof") + std::vector<uint32_t>{Pause, ' ', 'a', Pause, InField}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 🐈 🐱", "no panel: a word ended by a key held for the hint still counts", r.inField.front());
        r = type(keysOf("meof") + std::vector<uint32_t>{Pause} + keysOf(". cuwowif") + std::vector<uint32_t>{Pause, InField}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 😂 🤣 😄 😆 😁", "no panel: keys typed while one is held still end the sentence", r.inField.front());
        r = type(keysOf("hahaha vui") + std::vector<uint32_t>{Pause, Tab, Tab}, Mode::Sen, noPanel);
        check(r.text.rfind("hahaha vui ", 0) == 0, "no panel: a sound before the last word stays", r.text);

        r = type(word + std::vector<uint32_t>{InField}, Mode::Sen, makeField());
        check(r.inField.front().empty(), "a panel that shows the list keeps the hint out of the field", r.inField.front());
        Field noPreedit   = noPanel;
        noPreedit.preedit = false;
        r                 = type(word + std::vector<uint32_t>{Pause, InField, Tab}, Mode::Sen, noPreedit);
        check(r.inField.front().empty() && r.text == "cười⇥", "no panel and no preedit: Tab reaches the app", r.inField.front() + " / " + r.text);
        r = type(word + std::vector<uint32_t>{Pause, InField, Tab}, Mode::Preedit, noPanel);
        check(r.inField.front().empty() && r.text == "cười⇥", "no panel in Preedit mode: Tab reaches the app", r.inField.front() + " / " + r.text);
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
    testHintInField();
    testPicking();
    testReplacing();
    testReplacingWords();
    testSlowApp();
    testRelease();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
