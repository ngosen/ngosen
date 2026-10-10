/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// The typing logic run against a fake app field, with no fcitx5 at all.
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-app-quirks.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-recorder.h"
#include "ngosen-state.h"

#include <iostream>
#include <memory>
#include <string>
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

    // Focus that leaves a field which cannot report its text and comes straight back keeps the
    // word; a real switch starts a new one.
    void testFocusBounce() {
        for (const bool bounce : {true, false}) {
            realMode = ngosen::Mode::Sen;
            FakeLoop            loop;
            FakeResources       resources;
            auto                owned = std::make_unique<FakeHost>(loop, makeField("xim", false));
            ngosen::TypingState state(&resources, std::move(owned));

            for (const char c : std::string("tie"))
                press(state, static_cast<uint32_t>(c));
            state.deactivate(true);
            if (!bounce)
                loop.pump(150);
            state.activate(ngosen::Mode::Sen, true);
            const bool taken = press(state, static_cast<uint32_t>('e'));
            if (bounce)
                check(taken, "focus bounce: keeps the word", "e went to the app as a plain key");
            else
                check(!taken, "focus switch: starts a new word", "e was taken to change the old word");
        }
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
    testSenForwards("wayland", true);
    testSenForwards("wayland_v2", true);
    testSenForwards("xim", false);
    testKeyDuringReplacement();
    testGtk4DeletesSurrounding();
    testSdlGetsNoForwardedBackspace();
    testRecorderKeepsTheReplacement();
    testRecorderHidesPasswords();
    testRecorderLimits();
    testFocusBounce();
    if (failures == 0)
        std::cout << "all core typing checks passed\n";
    return failures == 0 ? 0 : 1;
}
