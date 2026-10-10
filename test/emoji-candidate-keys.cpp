// SPDX-License-Identifier: GPL-3.0-or-later
//
// The emoji picker's keys: digits and Space pick a candidate, Tab/arrows move within a page,
// Left/Right and Page_Up/Page_Down change page, Return commits the typed text, Escape closes.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok, const std::string& actual) {
        if (!ok) {
            std::cerr << "FAIL: " << step << " (got " << actual << ")\n";
            ++failures;
        }
    }

    struct Picker {
        fcitx::LotusEngine&      engine;
        fcitx::InputMethodEntry& entry;
        TestInputContext&        context;

        bool                     press(fcitx::KeySym symbol) {
            fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
            engine.keyEvent(entry, event);
            return event.accepted();
        }

        std::shared_ptr<fcitx::CommonCandidateList> list() const {
            return std::dynamic_pointer_cast<fcitx::CommonCandidateList>(context.inputPanel().candidateList());
        }

        std::string state() const {
            auto l = list();
            if (!l)
                return "no list";
            return "total=" + std::to_string(l->totalSize()) + " page=" + std::to_string(l->currentPage()) + " cursor=" + std::to_string(l->globalCursorIndex()) +
                " status=" + context.inputPanel().auxDown().toString() + " first=" + l->candidateFromAll(0).text().toString();
        }

        std::string lastCommit() const {
            return context.commits().empty() ? "(none)" : context.commits().back();
        }

        void expect(const std::string& step, const std::string& expected) {
            const std::string actual = state();
            check(step + ": " + expected, actual == expected, actual);
        }
    };

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-emoji-candidate-keys");
    const auto    historyPath = std::filesystem::path(getEnv("XDG_CONFIG_HOME")) / "fcitx5/conf/ngosen-emoji-history.conf";
    std::ofstream history(historyPath);
    const char*   emoji[] = {"😀", "😁", "😂", "😃", "😄", "😅", "😆", "😉", "😊", "😋", "😎", "😍", "😘", "😗", "😙", "😚", "🙂", "🤗"};
    for (int i = 0; i < 18; ++i)
        history << "history" << i << '=' << emoji[i] << '\n';
    history.close();

    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Emoji Picker");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit});
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    Picker p{engine, entry, *context};

    p.expect("history opens on page one", "total=18 page=0 cursor=0 status=Page 1/2 first=1: 😀");

    p.press(FcitxKey_Down);
    p.press(FcitxKey_Tab);
    p.expect("Down and Tab move down", "total=18 page=0 cursor=2 status=Page 1/2 first=1: 😀");
    p.press(FcitxKey_Up);
    p.press(FcitxKey_ISO_Left_Tab);
    p.expect("Up and Shift+Tab move up", "total=18 page=0 cursor=0 status=Page 1/2 first=1: 😀");
    p.press(FcitxKey_Up);
    p.expect("Up on the first candidate wraps to the last of the page", "total=18 page=0 cursor=8 status=Page 1/2 first=1: 😀");
    p.press(FcitxKey_Down);
    p.expect("Down on the last candidate wraps to the first of the page", "total=18 page=0 cursor=0 status=Page 1/2 first=1: 😀");

    p.press(FcitxKey_Right);
    p.expect("Right opens the next page", "total=18 page=1 cursor=9 status=Page 2/2 first=1: 😀");
    check("Right on the last page is passed on", !p.press(FcitxKey_Right), "accepted");
    p.press(FcitxKey_Page_Up);
    p.expect("Page_Up opens the previous page", "total=18 page=0 cursor=0 status=Page 1/2 first=1: 😀");
    check("Left on the first page is passed on", !p.press(FcitxKey_Left), "accepted");
    p.press(FcitxKey_Page_Down);
    p.press(FcitxKey_Left);
    p.expect("Page_Down and Left change page", "total=18 page=0 cursor=0 status=Page 1/2 first=1: 😀");

    check("a digit picks from the page", p.press(FcitxKey_3) && p.lastCommit() == "😂", p.lastCommit());
    p.expect("the pick moves to the front of the history", "total=9 page=0 cursor=0 status=Page 1/1 first=1: 😂");

    p.press(FcitxKey_Down);
    check("Space picks the highlighted candidate", p.press(FcitxKey_space) && p.lastCommit() == "😀", p.lastCommit());
    p.expect("history after Space", "total=9 page=0 cursor=0 status=Page 1/1 first=1: 😀");

    p.list()->candidateFromAll(2).select(context.get());
    check("clicking a candidate picks it", p.lastCommit() == "😁", p.lastCommit());

    p.press(FcitxKey_a);
    p.press(FcitxKey_b);
    check("typed text shows underlined",
          context->inputPanel().clientPreedit().toString() == "ab" && context->inputPanel().clientPreedit().formatAt(0) == fcitx::TextFormatFlag::Underline,
          context->inputPanel().clientPreedit().toString());
    p.press(FcitxKey_BackSpace);
    check("BackSpace erases typed text", context->inputPanel().clientPreedit().toString() == "a", context->inputPanel().clientPreedit().toString());
    const size_t commits = context->commits().size();
    p.press(FcitxKey_b);
    // The test instance loads no emoji data, so typed text matches nothing.
    check("nothing matches the typed text", !p.list(), p.state());
    check("Return commits the typed text when nothing matches", p.press(FcitxKey_Return) && p.lastCommit() == "ab" && context->commits().size() == commits + 1, p.lastCommit());
    check("the typed text is cleared after Return", context->inputPanel().clientPreedit().toString().empty(), context->inputPanel().clientPreedit().toString());

    p.press(FcitxKey_x);
    check("Escape is taken", p.press(FcitxKey_Escape), "rejected");
    check("Escape closes the list", !p.list() && context->inputPanel().auxDown().toString().empty(), p.state());

    return failures == 0 ? 0 : 1;
}
