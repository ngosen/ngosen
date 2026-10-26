/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// A word that starts with '/' reaches the app as typed, key by key; other words keep their tones.
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

    Field makeField(bool surroundingText = true) {
        Field field;
        field.frontend        = "wayland";
        field.surroundingText = surroundingText;
        field.preedit         = true;
        return field;
    }

    std::vector<uint32_t> keysOf(const std::string& typed) {
        return {typed.begin(), typed.end()};
    }

    constexpr uint32_t Click    = 0;
    constexpr uint32_t NewFocus = 1;

    // The app's text after each key, typed 20 ms apart. Click and NewFocus stand for those events.
    std::vector<std::string> textAfterEachKey(Mode mode, const std::vector<uint32_t>& keys, bool surroundingText = true,
                                              const std::function<void(FakeResources&)>& configure = {}) {
        FakeResources resources;
        if (configure)
            configure(resources);
        realMode = mode;
        realtextLen.store(0);
        FakeLoop            loop;
        auto                owned = std::make_unique<FakeApp>(loop, makeField(surroundingText), AppQuirks{});
        FakeApp*            app   = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        std::vector<std::string> texts;
        for (const uint32_t key : keys) {
            if (key == Click) {
                g_mouse_clicked.store(true);
                continue;
            }
            if (key == NewFocus) {
                state.reset(true);
                continue;
            }
            app->type(key);
            loop.pump(20);
            texts.push_back(app->text());
        }
        return texts;
    }

    std::string finalText(Mode mode, const std::string& typed, bool surroundingText = true, const std::function<void(FakeResources&)>& configure = {}) {
        return textAfterEachKey(mode, keysOf(typed), surroundingText, configure).back();
    }

    const char* modeName(Mode mode) {
        return mode == Mode::Sen ? "sen" : "preedit";
    }

    // Counts the keys from the '/' on after which the app shows anything but the keys typed so far.
    // Preedit holds a word before the '/' until the space after it, as it should.
    void checkTypedAsIs(const std::string& what, const std::vector<std::string>& typed) {
        for (const Mode mode : {Mode::Sen, Mode::Preedit}) {
            int         steps = 0, wrong = 0;
            std::string shown;
            for (const auto& word : typed) {
                const auto texts = textAfterEachKey(mode, keysOf(word));
                for (size_t i = word.find('/'); i < texts.size(); ++i) {
                    ++steps;
                    if (texts[i] != word.substr(0, i + 1)) {
                        ++wrong;
                        shown += " " + texts[i];
                    }
                }
            }
            check(wrong == 0, what + " (" + modeName(mode) + "): each key reaches the app as typed", std::to_string(wrong) + "/" + std::to_string(steps) + " keys wrong:" + shown);
        }
    }

    void testCommands() {
        checkTypedAsIs("commands", {"/resume", "/clear",  "/compact", "/cost",    "/model",  "/help",  "/status", "/init",        "/memory", "/login",    "/doctor", "/review",
                                    "/exit",   "/agents", "/config",  "/context", "/export", "/hooks", "/mcp",    "/permissions", "/plan",   "/rewind",   "/usage",  "/vim",
                                    "/theme",  "/remind", "/giphy",   "/invite",  "/topic",  "/away",  "/me",     "/nick",        "/shrug",  "/home/pibi"});
    }

    void testPathsAfterSpace() {
        checkTypedAsIs("paths", {"cd /usr/share", "cd /etc/hosts", "cd /var/log", "cd /tmp", "cd /opt", "cd /dev/null", "cd /proc", "cd /boot", "cd /srv", "cd /sys"});
        checkTypedAsIs("other path characters", {"cd /usr/include/c++/14/bits", "ls /home/u/.config/foo@bar/test", "cd /tmp/a,b=c/test"});
        checkTypedAsIs("relative paths and quoted commands", {"cd ./test", "cd ../test", "ls ~/test", "`/cost`", "(/usage)"});
    }

    void testVietnameseKeepsTones() {
        const std::vector<std::pair<std::string, std::string>> cases = {
            {"@nguyeenx ", "@nguyễn "},
            {"@Hoangf Anh ", "@Hoàng Anh "},
            {"vaf/hoawcj ", "và/hoặc "},
            {"1/2 ngayf ", "1/2 ngày "},
            {"km/h ", "km/h "},
            {"26/10 ", "26/10 "},
            {"a/b ", "a/b "},
            {"xin chaof /resume ", "xin chào /resume "},
            {"/resume xin chaof ", "/resume xin chào "},
        };
        for (const Mode mode : {Mode::Sen, Mode::Preedit}) {
            for (const auto& [typed, want] : cases) {
                const std::string got = finalText(mode, typed);
                check(got == want, std::string(modeName(mode)) + ": " + typed + "-> " + want, got);
            }
        }
    }

    void testWordEnds() {
        using namespace ngosen::key;
        std::vector<uint32_t> keys = {'/', BackSpace};
        for (const char c : std::string("vieetj "))
            keys.push_back(static_cast<uint32_t>(c));
        std::string got = textAfterEachKey(Mode::Sen, keys).back();
        check(got == "việt ", "deleting the '/' types Vietnamese again", got);

        keys = {'/', 'r', 'e', 's', Tab};
        for (const char c : std::string("vieetj "))
            keys.push_back(static_cast<uint32_t>(c));
        got = textAfterEachKey(Mode::Sen, keys).back();
        check(got == "/resviệt ", "Tab, as when an app completes the command, types Vietnamese again", got);
    }

    std::vector<uint32_t> keysAround(const std::string& before, uint32_t event, const std::string& after) {
        std::vector<uint32_t> keys = keysOf(before);
        keys.push_back(event);
        for (const uint32_t k : keysOf(after))
            keys.push_back(k);
        return keys;
    }

    void testFieldWithoutReports() {
        std::string got = finalText(Mode::Sen, "xin chaof /cost", false);
        check(got == "xin chào /cost", "a field without surrounding text: '/' after a space starts a word", got);
        got = textAfterEachKey(Mode::Sen, keysAround("xin", NewFocus, "/cost"), false).back();
        check(got == "xin/cost", "a field without surrounding text: '/' first after focus starts a word", got);
        got = finalText(Mode::Sen, "xin/cost", false);
        check(got == "xin/cót", "a field without surrounding text: '/' inside a word is not a command", got);
    }

    void testClickEndsRawWord() {
        const std::string got = textAfterEachKey(Mode::Sen, keysAround("/us", Click, "vieetj"), false).back();
        check(got == "/usviệt", "a click ends the raw word", got);
    }

    void testDoubledKeysStayRaw() {
        std::string got = finalText(Mode::Sen, "/tmp/my--dir", true, [](FakeResources& r) { r.mutableOptions().doubleHyphenToEmDash = true; });
        check(got == "/tmp/my--dir", "-- stays as typed in a path", got);
        got = finalText(Mode::Sen, "a /x  b", true, [](FakeResources& r) { r.mutableOptions().doubleSpaceToPeriod = true; });
        check(got == "a /x. b", "two spaces after a raw word still give a period", got);
        got = finalText(Mode::Sen, "a /x b", true, [](FakeResources& r) { r.mutableOptions().doubleSpaceToPeriod = true; });
        check(got == "a /x b", "the space before the '/' and the one after the word are not a double space", got);
    }

    void testOptionOff() {
        const std::string got = finalText(Mode::Sen, "/cost", true, [](FakeResources& r) { r.mutableOptions().rawWordAfterSlash = false; });
        check(got == "/cót", "with the option off, /cost gets a tone as before", got);
    }

} // namespace

int main() {
    testCommands();
    testPathsAfterSpace();
    testVietnameseKeepsTones();
    testWordEnds();
    testFieldWithoutReports();
    testClickEndsRawWord();
    testDoubledKeysStayRaw();
    testOptionOff();
    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "all raw word tests passed\n";
    return 0;
}
