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

#include <algorithm>
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
    constexpr uint32_t Reset    = 9;  // the app resets the input context, as Firefox does after each key it types
    constexpr uint32_t Home     = 10; // the cursor moves to the start of the field
    constexpr uint32_t Lag      = 11; // the app reports its text 5 ms after each change
    // Firefox resets right after the key and reports the text it typed only after that.
    constexpr uint32_t EarlyReset    = 12;
    constexpr uint32_t Chosen        = 13; // records the highlighted item of the panel
    constexpr uint32_t Pick          = 14; // a click on the first item of the panel
    constexpr uint32_t Gap           = 15; // 600 ms between keys, short of a pause
    constexpr uint32_t Bounce        = 16; // the field loses focus and gets it back 30 ms later
    constexpr uint32_t PanelReset    = 17; // the engine clears the panel, as for a mode notice
    constexpr uint32_t BrowserBounce = 18; // a bounce, a reset and an empty first report, as Brave on Wayland
    constexpr uint32_t Backspace     = ngosen::key::BackSpace;
    constexpr uint32_t Left          = ngosen::key::Left;
    constexpr uint32_t Right         = ngosen::key::Right;
    constexpr uint32_t Down          = ngosen::key::Down;

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
        std::vector<int>         chosen;
        int                      editsUnderHint = 0;
        int                      listsHidden    = 0;
    };

    AppQuirks chatQuirks() {
        return AppQuirks{.showsEnter = true, .showsTab = true};
    }

    Result type(const std::vector<uint32_t>& keys, Mode mode = Mode::Sen, Field field = makeField(), const std::function<void(FakeResources&)>& configure = {},
                AppQuirks quirks = chatQuirks()) {
        FakeResources resources;
        if (configure)
            configure(resources);
        realMode = mode;
        realtextLen.store(0);
        FakeLoop            loop;
        auto                owned = std::make_unique<FakeApp>(loop, field, quirks);
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
            } else if (key == FocusOut) {
                state.reset(true);
            } else if (key == SlowHint) {
                app->setHintLag(30);
            } else if (key == Pause) {
                loop.pump(1000);
            } else if (key == InField) {
                result.inField.push_back(app->hint());
            } else if (key == Gap) {
                loop.pump(600);
            } else if (key == Bounce) {
                app->leaveFocus(30);
                loop.pump(60);
                // As the engine's showMode does after activate().
                app->resetPanel();
                state.panelWasCleared(true);
            } else if (key == BrowserBounce) {
                app->bounceReportingEmpty(30);
                loop.pump(60);
                app->resetPanel();
                state.panelWasCleared(true);
            } else if (key == PanelReset) {
                app->resetPanel();
                state.panelWasCleared(false);
            } else if (key == Pick) {
                app->pickCandidate(0);
                loop.pump(20);
            } else if (key == Chosen) {
                result.chosen.push_back(app->highlighted());
            } else if (key == Status) {
                result.statuses.push_back(app->status());
            } else if (key == Hint) {
                std::string shown = app->labels().empty() ? "(none)" : app->labels().front();
                for (size_t n = 1; app->labelsInRow() && n < app->labels().size(); ++n)
                    shown += " " + app->labels()[n];
                result.hints.push_back(shown);
            } else {
                app->type(key);
                if (i + 1 == keys.size() || keys[i + 1] != EarlyReset)
                    loop.pump(20);
            }
        }
        loop.pump(200);
        result.text           = app->text();
        result.editsUnderHint = app->editsUnderHint();
        result.listsHidden    = app->candidatesHidden();
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
        check(r.inField.front() == " [1.😂] 2.🤣 3.😄 4.😆 5.😁", "no panel: Tab numbers the emoji in the field", r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, Tab, '2', InField}, Mode::Sen, noPanel);
        check(r.text == "cười 🤣" && r.inField.front().empty(), "no panel: a number picks that emoji", r.text + " / " + r.inField.front());
        r = type(word + std::vector<uint32_t>{Pause, Tab, Tab}, Mode::Sen, noPanel);
        check(r.text == "cười 😂", "no panel: Tab again picks the first", r.text);
        r = type(word + std::vector<uint32_t>{Pause, Tab, Right, InField, Enter}, Mode::Sen, noPanel);
        check(r.inField.front() == " 1.😂 [2.🤣] 3.😄 4.😆 5.😁" && r.text == "cười 🤣", "no panel: arrows mark the emoji Enter picks", r.inField.front() + " / " + r.text);
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
        check(r.statuses == std::vector<std::string>{"", "1–5: pick, or arrows then Enter, Esc: keep the word", ""}, "the keys are named while the list is open",
              r.statuses[0] + " / " + r.statuses[1] + " / " + r.statuses[2]);
        r = type(keysOf("camr own") + std::vector<uint32_t>{Tab, Status, Esc, Status});
        check(r.statuses == std::vector<std::string>{"1–2: pick, or arrows then Enter, Esc: keep the word", ""}, "the line counts the emoji and goes with Esc",
              r.statuses[0] + " / " + r.statuses[1]);
        r = type(word + std::vector<uint32_t>{Tab, Right, Right, Chosen, Enter});
        check(r.chosen == std::vector<int>{2} && r.text == "cười 😄", "arrows move the highlight and Enter picks it",
              std::to_string(r.chosen.empty() ? -1 : r.chosen[0]) + " / " + r.text);
        r = type(word + std::vector<uint32_t>{Tab, Left, Chosen, ' '});
        check(r.chosen == std::vector<int>{4} && r.text == "cười 😁", "Left from the first wraps to the last", r.text);
        r = type(word + std::vector<uint32_t>{Tab, Down, Tab});
        check(r.text == "cười 🤣", "Down moves on as Right does, and Tab picks the highlighted", r.text);
        r = type(word + std::vector<uint32_t>{Left, ' '});
        check(r.text.find("😂") == std::string::npos, "an arrow before Tab is not taken", r.text);
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

    // With emoji offered at the end of a sentence, after . ! ? or a pause, for every keyword in it.
    void testSentenceEnd() {
        const auto atEnd = [](FakeResources& r) { r.mutableOptions().emojiAtSentenceEnd = true; };
        auto       r     = type(keysOf("cuwowif") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() == "😂 🤣 😄 😆 😁", "sentence end: a preview while the sentence goes on", r.hints.front());
        r = type(keysOf("cuwowif.") + std::vector<uint32_t>{Hint, Tab, Tab}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() == "1.😂 2.🤣 3.😄 4.😆 5.😁" && r.text == "cười. 😂", "sentence end: a full stop offers the emoji, put after it", r.hints.front() + " / " + r.text);
        r = type(keysOf("con meof cuwowif quas") + std::vector<uint32_t>{Pause, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() == "1.😂 2.🤣 3.😄 4.🐈 5.🐱", "sentence end: a pause offers the emoji of the sentence", r.hints.front());
        r = type(keysOf("con meof cuwowif quas!") + std::vector<uint32_t>{Tab, '4'}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo cười quá! 🐈", "sentence end: an emoji of an earlier word", r.text);
        r = type(keysOf("hahaha!") + std::vector<uint32_t>{Tab, Tab}, Mode::Sen, makeField(), atEnd);
        check(r.text.rfind("hahaha! ", 0) == 0 && r.text.size() > 8, "sentence end: a sound before the stop stays", r.text);
        r = type(keysOf("cuwowif. ") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() == "(none)" && r.text == "cười. ", "sentence end: typing on takes the offer away", r.hints.front() + " / " + r.text);
        r = type(keysOf("gias 3.5") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() == "(none)", "sentence end: a decimal point ends nothing", r.hints.front());

        Field noPanel           = makeField();
        noPanel.drawsCandidates = false;
        r                       = type(keysOf("con meof.") + std::vector<uint32_t>{Pause, InField, Tab, Tab}, Mode::Sen, noPanel);
        check(r.inField.front() == " Tab: 🐈 🐱" && r.text == "con mèo. 🐈", "a word before a full stop counts in the field", r.inField.front() + " / " + r.text);
    }

    // A list left from before the cursor moved would put the emoji, or replace a word, at the new cursor.
    void testPickAfterCursorMoved() {
        auto r = type(keysOf("cuwowif") + std::vector<uint32_t>{Pause, Hint, Home, Pause, Pick});
        check(r.hints.front() != "(none)" && r.text == "cười", "a click on the list after the cursor moved puts nothing", r.hints.front() + " / " + r.text);
        const auto atEnd = [](FakeResources& res) { res.mutableOptions().emojiAtSentenceEnd = true; };
        r                = type(keysOf("con meof cuwowif.") + std::vector<uint32_t>{Pause, Hint, Home, Pause, Pick}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() != "(none)" && r.text == "con mèo cười.", "sentence end: a click after the cursor moved puts nothing", r.hints.front() + " / " + r.text);
    }

    // Keys in, with the panel's first label recorded after each from the given one on.
    std::vector<uint32_t> watched(const std::string& typed, size_t from = 0) {
        std::vector<uint32_t> keys;
        for (size_t i = 0; i < typed.size(); ++i) {
            keys.push_back(static_cast<unsigned char>(typed[i]));
            if (i >= from)
                keys.push_back(Hint);
        }
        return keys;
    }

    bool allShown(const std::vector<std::string>& hints) {
        return std::none_of(hints.begin(), hints.end(), [](const std::string& h) { return h == "(none)"; });
    }

    std::string joined(const std::vector<std::string>& hints) {
        std::string all;
        for (const auto& h : hints)
            all += "[" + h + "]";
        return all;
    }

    // With a panel, the emoji of the sentence show from its first keyword on, for a look only.
    void testSentencePreview() {
        const auto atEnd = [](FakeResources& r) { r.mutableOptions().emojiAtSentenceEnd = true; };
        // From the f that makes mèo.
        auto r = type(watched("con meof cuwowif", 7), Mode::Sen, makeField(), atEnd);
        check(allShown(r.hints) && r.hints.front() == "🐈 🐱" && r.hints.back() == "😂 🤣 😄 🐈 🐱", "preview: shows from the first keyword to the end", joined(r.hints));

        AppQuirks slow = chatQuirks();
        slow.lagMs     = 60;
        r              = type(watched("con meof cuwowif", 7) + std::vector<uint32_t>{Gap, Hint}, Mode::Sen, makeField(), atEnd, slow);
        check(allShown(r.hints) && r.hints.back() == "😂 🤣 😄 🐈 🐱", "preview: stays while a slow app takes a tone", joined(r.hints));

        std::vector<uint32_t> firefox;
        for (const uint32_t key : watched("con meof cuwowif", 7)) {
            firefox.push_back(key);
            if (key != Hint)
                firefox.push_back(Reset);
        }
        // A one-letter word may match on its own and go with the next letter, reset or not.
        const int hiddenWithoutReset = type(watched("con meof cuwowif", 7), Mode::Sen, makeField(), atEnd).listsHidden;
        r                            = type(firefox, Mode::Sen, makeField(), atEnd);
        check(allShown(r.hints) && r.listsHidden == hiddenWithoutReset, "preview: stays through Firefox's reset after each key",
              joined(r.hints) + " hidden " + std::to_string(r.listsHidden) + " vs " + std::to_string(hiddenWithoutReset));

        r = type(keysOf("con meof ") + std::vector<uint32_t>{Hint} + keysOf("cuwo") + std::vector<uint32_t>{Hint} + keysOf("wif ") + std::vector<uint32_t>{Hint}, Mode::Preedit,
                 makeField(), atEnd);
        check(r.hints.size() == 3 && r.hints[0] == "🐈 🐱" && r.hints[1] == "🐈 🐱" && r.hints[2] == "😂 🤣 😄 🐈 🐱", "preview: an open word keeps the list until it ends",
              joined(r.hints));

        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Bounce, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.size() == 2 && r.hints[0] == "🐈 🐱" && r.hints[1] == "🐈 🐱", "preview: shows again after a focus bounce", joined(r.hints));
        r = type(keysOf("con meof ") + std::vector<uint32_t>{BrowserBounce} + keysOf("cuw") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.size() == 1 && r.hints[0] == "🐈 🐱" && r.text == "con mèo cư", "preview: keeps the sentence when the field first reports empty after a bounce",
              r.text + " / " + joined(r.hints));

        r = type(keysOf("con meof") + std::vector<uint32_t>{Gap, ' ', Gap, 'c', Gap, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints.front() != "(none)" && r.hints.front().rfind("Tab:", 0) != 0, "preview: a pause counts from the last key", r.hints.front());

        r = type(keysOf("meof") + std::vector<uint32_t>{Hint, Backspace, Backspace, Backspace, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "🐈 🐱" && r.hints[1] == "(none)", "preview: goes with its only keyword", joined(r.hints));

        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Pick, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "🐈 🐱" && r.text == "con mèo", "preview: a click on it puts nothing", joined(r.hints) + " / " + r.text);
    }

    // The preview holds no key: everything typed mid-sentence reaches the app.
    void testPreviewKeys() {
        const auto atEnd = [](FakeResources& r) { r.mutableOptions().emojiAtSentenceEnd = true; };
        auto       r     = type(keysOf("con meof lucs 3") + std::vector<uint32_t>{Hint} + keysOf(" giowf"), Mode::Sen, makeField(), atEnd);
        check(r.hints.front() != "(none)" && r.text == "con mèo lúc 3 giờ", "preview: digits are typed", r.hints.front() + " / " + r.text);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Tab, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "🐈 🐱" && r.hints[1] == "(none)" && r.text == "con mèo⇥", "preview: Tab goes to the app and closes it", joined(r.hints) + " / " + r.text);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Enter, Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "🐈 🐱" && r.hints[1] == "(none)" && r.text == "con mèo\n", "preview: Enter goes to the app and closes it", joined(r.hints) + " / " + r.text);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Pause, Hint, Enter}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "1.🐈 2.🐱" && r.text == "con mèo\n", "after a pause Enter still sends", r.hints[0] + " / " + r.text);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Esc, Pause, Hint} + keysOf(". cuwowif") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "🐈 🐱" && r.hints[1] == "(none)" && r.hints[2] == "😂 🤣 😄 😆 😁", "Esc: no emoji for the rest of the sentence", joined(r.hints));
        const std::vector<uint32_t> eraseAll(7, Backspace);
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Esc} + eraseAll + keysOf("cuwowif") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.text == "cười" && r.hints[1] == "😂 🤣 😄 😆 😁", "a sentence typed after the muted one is erased gets emoji again", r.text + " / " + joined(r.hints));
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Esc, Backspace} + keysOf("o") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo" && r.hints[1] == "(none)", "fixing a letter keeps the sentence muted", r.text + " / " + joined(r.hints));
        // Ctrl+A shows up as a selection, so the Backspace after it comes as a key after a click.
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, Esc, Click, Backspace} + keysOf(" cuwowif") + std::vector<uint32_t>{Hint}, Mode::Sen, makeField(), atEnd);
        check(r.hints[1] == "😂 🤣 😄 😆 😁", "a selection or click forgets the muted sentence", r.text + " / " + joined(r.hints));
        r = type(keysOf("con meof") + std::vector<uint32_t>{Hint, PanelReset, Tab}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo⇥", "a list the engine cleared holds no key", r.text);
    }

    // Where digits type nothing but digits, a digit picks from the list once it can be picked.
    void testDigitsPick() {
        const auto atEnd = [](FakeResources& r) { r.mutableOptions().emojiAtSentenceEnd = true; };
        auto       r     = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, Hint, '2'}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0] == "1.😂 2.🤣 3.😄 4.🐈 5.🐱" && r.text == "con mèo cười 🤣", "a digit picks the emoji", r.hints[0] + " / " + r.text);
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, '7'}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo cười7", "a digit past the list is typed", r.text);
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, ngosen::key::KP_0 + 1}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo cười1", "a keypad digit is typed", r.text);
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, PanelReset, '1'}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo cười1", "a list the engine cleared takes no digit", r.text);
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, Pick}, Mode::Sen, makeField(), atEnd);
        check(r.text == "con mèo cười 😂", "a click on a numbered emoji picks it", r.text);
        r = type(keysOf("con meof gias 3.") + std::vector<uint32_t>{Hint, '5'}, Mode::Sen, makeField(), atEnd);
        check(r.hints[0].rfind("1.", 0) != 0 && r.hints[0].rfind("Tab:", 0) != 0 && r.text == "con mèo giá 3.5", "a dot after a digit ends no sentence",
              r.hints[0] + " / " + r.text);

        AppQuirks oldText                 = chatQuirks();
        oldText.repeatsOldTextAfterDelete = true;
        r                                 = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, Hint, '2'}, Mode::Sen, makeField(), atEnd, oldText);
        check(r.hints[0] == "1.😂 2.🤣 3.😄 4.🐈 5.🐱" && r.text == "con mèo cười 🤣", "a field showing its text from before the tone keeps the word", r.hints[0] + " / " + r.text);

        const auto vni = [](FakeResources& r) {
            r.mutableOptions().emojiAtSentenceEnd = true;
            r.mutableOptions().inputMethod        = "VNI";
        };
        r = type(keysOf("con meo2 cu7o7i2") + std::vector<uint32_t>{Pause, Hint, '1'}, Mode::Sen, makeField(), vni);
        check(r.hints[0].rfind("Tab: ", 0) == 0 && r.text.find("😂") == std::string::npos, "VNI: digits keep their marks", r.hints[0] + " / " + r.text);

        const std::vector<ngosen::KeymapEntry> telexKeys = {{"z", "XoaDauThanh"}, {"s", "DauSac"}, {"f", "DauHuyen"}, {"r", "DauHoi"},    {"x", "DauNga"}, {"j", "DauNang"},
                                                            {"a", "A_Â"},         {"e", "E_Ê"},    {"o", "O_Ô"},      {"w", "UOAÔ_ƯƠĂƠ"}, {"d", "D_Đ"}};
        const auto                             custom    = [](std::vector<ngosen::KeymapEntry> keys) {
            return [keys](FakeResources& r) {
                r.mutableOptions().emojiAtSentenceEnd = true;
                r.mutableOptions().inputMethod        = "Custom";
                r.setCustomKeymap(keys);
            };
        };
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, Hint, '2'}, Mode::Sen, makeField(), custom(telexKeys));
        check(r.hints[0] == "1.😂 2.🤣 3.😄 4.🐈 5.🐱" && r.text == "con mèo cười 🤣", "custom keys without digits: digits pick", r.hints[0] + " / " + r.text);
        auto digitKeys = telexKeys;
        digitKeys.push_back({"2", "DauHuyen"});
        r = type(keysOf("con meof cuwowif") + std::vector<uint32_t>{Pause, Hint, '1'}, Mode::Sen, makeField(), custom(digitKeys));
        check(r.hints[0].rfind("Tab: ", 0) == 0 && r.text.find("😂") == std::string::npos, "custom keys with a digit: digits keep their marks", r.hints[0] + " / " + r.text);
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
    testSentenceEnd();
    testPickAfterCursorMoved();
    testSentencePreview();
    testPreviewKeys();
    testDigitsPick();
    testRelease();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
