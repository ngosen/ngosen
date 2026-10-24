/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// A tone key typed over a selected word changes that word's tone.
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

    Field makeField(const std::string& frontend = "wayland", bool surroundingText = true) {
        Field field;
        field.frontend        = frontend;
        field.surroundingText = surroundingText;
        field.preedit         = true;
        return field;
    }

    struct Run {
        Field                                    field = makeField();
        AppQuirks                                quirks;
        Mode                                     mode = Mode::Sen;
        std::string                              typed; // Telex keys typed first
        size_t                                   anchor        = 0;
        size_t                                   cursor        = 0;
        bool                                     repeatsReport = false; // reports the old state once more, as Firefox does
        uint32_t                                 tone          = 's';
        std::vector<uint32_t>                    after;          // keys once the tone has changed
        int                                      settleMs = 150; // wait after the tone key
        int                                      gapMs    = 150; // wait after each key in after
        std::function<void(FakeResources&)>      configure;
        std::function<void(FakeApp&, FakeLoop&)> beforeAfter;
    };

    struct Result {
        std::string hint; // the status line once the word has changed, empty when it did not
        std::string text;
    };

    Result retone(const Run& run) {
        realMode = run.mode;
        realtextLen.store(0);
        g_mouse_clicked.store(false);
        FakeLoop      loop;
        FakeResources resources;
        if (run.configure)
            run.configure(resources);
        auto                owned = std::make_unique<FakeApp>(loop, run.field, run.quirks);
        FakeApp*            app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        state.activate(run.mode, false);
        for (const char c : run.typed) {
            app->type(static_cast<uint32_t>(c));
            loop.pump(5);
        }
        loop.pump(150);
        app->select(run.anchor, run.cursor);
        loop.pump(150);
        app->type(run.tone);
        if (run.repeatsReport)
            state.surroundingUpdated();
        loop.pump(run.settleMs);
        Result result;
        result.hint = app->status();
        if (run.beforeAfter)
            run.beforeAfter(*app, loop);
        for (const uint32_t sym : run.after) {
            app->type(sym);
            loop.pump(run.gapMs);
        }
        loop.pump(300);
        result.text = app->text();
        g_mouse_clicked.store(false);
        return result;
    }

    // "abc nhanh xyz" with nhanh selected.
    Run nhanh(size_t anchor = 4, size_t cursor = 9) {
        Run run;
        run.typed  = "abc nhanh xyz";
        run.anchor = anchor;
        run.cursor = cursor;
        return run;
    }

    void testRetone() {
        auto r = retone(nhanh());
        check(r.text == "abc nhánh xyz" && !r.hint.empty(), "a tone key over a selected word changes it at once", r.text);
        r = retone(nhanh(9, 4));
        check(r.text == "abc nhánh xyz", "the selection may run either way", r.text);

        Run firefox                      = nhanh();
        firefox.quirks.hidesSelectionEnd = true;
        r                                = retone(firefox);
        check(r.text == "abc nhánh xyz", "a field that hides the selection end", r.text);
        firefox.repeatsReport = true;
        r                     = retone(firefox);
        check(r.text == "abc nhánh xyz", "a repeated old report is not the answer", r.text);

        Run gnome   = nhanh();
        gnome.field = makeField("ibus");
        r           = retone(gnome);
        check(r.text == "abc nhánh xyz", "a field behind GNOME Shell's IBus bridge", r.text);
        gnome.after = {'a', 'c', 'h', 's'};
        r           = retone(gnome);
        check(r.text == "abc sách xyz", "behind the IBus bridge a letter after the key puts it back", r.text);

        Run run   = nhanh();
        run.after = {ngosen::key::Escape};
        r         = retone(run);
        check(r.text == "abc s xyz", "Esc puts back the key", r.text);
        run.after = {ngosen::key::Escape, 'a'};
        r         = retone(run);
        check(r.text == "abc sa xyz", "after Esc, typing goes on from the key", r.text);
        run.after = {'a', 'c', 'h', 's'};
        r         = retone(run);
        check(r.text == "abc sách xyz", "a letter after the key puts it back and starts a word with it", r.text);
        run.after = {'f'};
        r         = retone(run);
        check(r.text == "abc nhành xyz", "another tone key changes the tone again", r.text);
        run.after = {'f', ngosen::key::Escape};
        r         = retone(run);
        check(r.text == "abc s xyz", "Esc after a second tone key puts back the first key", r.text);
        run.after = {ngosen::key::space};
        r         = retone(run);
        check(r.text == "abc nhánh  xyz", "a space keeps the new tone", r.text);

        // The letters arrive while the forwarded backspace for the retone is still on its way.
        Run fast              = nhanh();
        fast.field            = makeField("xim");
        fast.quirks.keyStepMs = 8;
        fast.settleMs         = 1;
        fast.gapMs            = 3;
        fast.after            = {'a', 'c', 'h', 's'};
        r                     = retone(fast);
        check(r.text == "abc sách xyz", "letters typed during the retone still put the key back", r.text);
        run.after = {ngosen::key::Tab};
        r         = retone(run);
        check(r.text == "abc s xyz", "Tab puts back the key", r.text);
        run.after = {'s'};
        r         = retone(run);
        check(r.text == "abc s xyz", "the tone key again puts it back", r.text);
        run.after = {'s', 'a'};
        r         = retone(run);
        check(r.text == "abc sa xyz", "after the tone key again, typing goes on", r.text);

        Run vni       = nhanh();
        vni.typed     = "abc hai xyz";
        vni.cursor    = 7;
        vni.tone      = '2';
        vni.configure = [](FakeResources& res) { res.mutableOptions().inputMethod = "VNI"; };
        r             = retone(vni);
        check(r.text == "abc hài xyz", "VNI: a digit over a word changes its tone", r.text);
        vni.after = {'2'};
        r         = retone(vni);
        check(r.text == "abc 2 xyz", "VNI: the digit again gives the digit", r.text);
    }

    void testWords() {
        const auto retoneOf = [](const std::string& typed, size_t from, size_t to, uint32_t tone, const std::function<void(FakeResources&)>& configure = {}) {
            Run run;
            run.typed     = typed;
            run.anchor    = from;
            run.cursor    = to;
            run.tone      = tone;
            run.configure = configure;
            return retone(run);
        };
        auto r = retoneOf("abc nhanhj xyz", 4, 9, 's');
        check(r.text == "abc nhánh xyz", "another tone replaces the old one", r.text);
        r = retoneOf("abc nhanhs xyz", 4, 9, 'z');
        check(r.text == "abc nhanh xyz", "the key that removes a tone", r.text);
        r = retoneOf("abc NHANH xyz", 4, 9, 's');
        check(r.text == "abc NHÁNH xyz", "capitals keep their case", r.text);
        r = retoneOf("abc dduwowngf xyz", 4, 9, 's');
        check(r.text == "abc đướng xyz", "a word with đ and horns", r.text);
        r = retoneOf("abc af xyz", 4, 5, 's');
        check(r.text == "abc á xyz", "a single letter that has a mark", r.text);
        r = retoneOf("abc nhanh xyz", 4, 9, '1', [](FakeResources& res) { res.mutableOptions().inputMethod = "VNI"; });
        check(r.text == "abc nhánh xyz", "VNI tone key", r.text);
        r = retoneOf("abc nhanh xyz", 4, 9, '\'', [](FakeResources& res) { res.mutableOptions().inputMethod = "VIQR"; });
        check(r.text == "abc nhánh xyz", "VIQR tone key", r.text);

        r = retoneOf("abc a xyz", 4, 5, 's');
        check(r.hint.empty() && r.text == "abc s xyz", "a bare letter is left as typed", r.text);
        r = retoneOf("abc tieengs xyz", 4, 9, 's');
        check(r.hint.empty() && r.text == "abc s xyz", "the tone the word already has", r.text);
        r = retoneOf("abc nhanh xyz", 4, 9, 'x');
        check(r.hint.empty() && r.text == "abc x xyz", "a word the engine restores as invalid", r.text);
        r = retoneOf("abc hello xyz", 4, 9, 's');
        check(r.hint.empty() && r.text == "abc s xyz", "an English word", r.text);
        r = retoneOf("abc nhanh xyz", 0, 9, 's');
        check(r.hint.empty() && r.text == "s xyz", "two words", r.text);
        r = retoneOf("abc nhanh xyz", 6, 9, 's');
        check(r.hint.empty() && r.text == "abc nhs xyz", "part of a word", r.text);
        r = retoneOf("abc nhanh xyz", 9, 9, 's');
        check(r.hint.empty(), "a key typed with nothing selected", r.text);
    }

    void testWhereNotOffered() {
        Run run   = nhanh();
        run.field = makeField("xim", false);
        auto r    = retone(run);
        check(r.hint.empty() && r.text == "abc s xyz", "a field that reports no text is left as typed", r.text);

        run                = nhanh();
        run.quirks.reports = false;
        r                  = retone(run);
        check(r.hint.empty(), "nothing changes until the field reports the key replaced the word", r.text);

        run           = nhanh();
        run.configure = [](FakeResources& r) { r.mutableOptions().retoneSelection = false; };
        r             = retone(run);
        check(r.hint.empty() && r.text == "abc s xyz", "the option off leaves the key as typed", r.text);

        run      = nhanh();
        run.mode = Mode::Preedit;
        r        = retone(run);
        check(r.hint.empty(), "Preedit mode is left as typed", r.text);

        run.mode           = Mode::Sen;
        run.field          = makeField();
        run.field.password = true;
        r                  = retone(run);
        check(r.hint.empty() && r.text == "abc s xyz", "a password field is left as typed", r.text);
    }

    void testClickKeepsTone() {
        Run run         = nhanh();
        run.beforeAfter = [](FakeApp& app, FakeLoop& loop) {
            app.moveCursor(0);
            loop.pump(150);
        };
        run.after = {ngosen::key::Escape};
        auto r    = retone(run);
        check(r.text == "abc nhánh xyz", "Esc after a click elsewhere keeps the new tone", r.text);
    }

} // namespace

int main() {
    testRetone();
    testWords();
    testWhereNotOffered();
    testClickKeepsTone();
    if (failures == 0)
        std::cout << "all retone checks passed\n";
    return failures == 0 ? 0 : 1;
}
