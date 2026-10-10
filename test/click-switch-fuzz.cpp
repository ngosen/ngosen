// SPDX-License-Identifier: GPL-3.0-or-later
//
// Random typing in Gõ Sen mode with mouse clicks into earlier text and switches between apps, run
// through the real engine; some clicks come while a word is still being fixed. A click or a switch
// ends the word: the keys typed after it must give what they give when typed calmly at the same
// place in the same kind of app.
//
// NGOSEN_FUZZ_RUNS sets the number of scenarios (default 2); NGOSEN_FUZZ_SEED repeats one and prints
// its typing log if it fails. NGOSEN_TEST_LOG=1 shows the engine's log.

#include "ngosen-engine.h"
#include "ngosen-globals.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

    std::u32string decode(const std::string& s) {
        std::u32string out;
        for (auto it = s.begin(); it != s.end();) {
            uint32_t c = 0;
            it         = fcitx::utf8::getNextChar(it, s.end(), &c);
            out.push_back(static_cast<char32_t>(c));
        }
        return out;
    }

    std::string encode(const std::u32string& s) {
        std::string out;
        for (char32_t c : s)
            out += fcitx::utf8::UCS4ToUTF8(c);
        return out;
    }

    // What the engine sent to the app, in the order it was sent.
    struct Sent {
        enum Kind {
            Commit,
            BackSpace,
            Delete
        } kind;
        std::u32string text;
        int            offset = 0;
        unsigned int   size   = 0;
    };

    class OrderedContext final : public fcitx::InputContext {
      public:
        OrderedContext(fcitx::Instance* instance, const std::string& program, std::string frontend) :
            InputContext(instance->inputContextManager(), program), frontend_(std::move(frontend)) {
            created();
        }
        ~OrderedContext() override {
            destroy();
        }
        const char* frontend() const override {
            return frontend_.c_str();
        }
        void commitStringImpl(const std::string& text) override {
            sent.push_back({Sent::Commit, decode(text)});
        }
        void deleteSurroundingTextImpl(int offset, unsigned int size) override {
            sent.push_back({Sent::Delete, {}, offset, size});
        }
        void forwardKeyImpl(const fcitx::ForwardKeyEvent& event) override {
            if (event.key().sym() == FcitxKey_BackSpace && !event.isRelease())
                sent.push_back({Sent::BackSpace});
        }
        void              updatePreeditImpl() override {}

        std::vector<Sent> sent;

      private:
        std::string frontend_;
    };

    // An app with its own text. What the engine sends is applied as soon as the event loop runs,
    // and every change is reported back when the field reports surrounding text.
    class App {
      public:
        App(TestInstance& t, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, const std::string& program, const std::string& frontend, bool surrounding,
            bool clicksSeen) :
            engine_(engine), entry_(entry), context_(std::make_unique<OrderedContext>(&t.instance, program, frontend)), surrounding_(surrounding), clicksSeen_(clicksSeen) {
            context_->setCapabilityFlags(surrounding ? fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText} : fcitx::CapabilityFlags{});
        }

        void setText(const std::u32string& text, size_t cursor) {
            text_   = text;
            cursor_ = cursor;
        }

        void focusIn() {
            context_->focusIn();
            fcitx::InputContextEvent event(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, event);
            report();
        }

        void focusOut() {
            context_->focusOut();
            fcitx::InputContextEvent event(context_.get(), fcitx::EventType::InputContextFocusOut);
            engine_.deactivate(entry_, event);
            sync();
        }

        void press(uint32_t sym) {
            fcitx::KeyEvent down(context_.get(), fcitx::Key(static_cast<fcitx::KeySym>(sym)), false);
            engine_.keyEvent(entry_, down);
            sync();
            if (!down.accepted()) {
                if (sym == FcitxKey_BackSpace)
                    erase(1);
                else
                    insert(decode(fcitx::Key::keySymToUTF8(static_cast<fcitx::KeySym>(sym))));
                report();
            }
            fcitx::KeyEvent up(context_.get(), fcitx::Key(static_cast<fcitx::KeySym>(sym)), true);
            engine_.keyEvent(entry_, up);
            sync();
        }

        // Moves the cursor to a share of the text; an X11 app's click is also seen by the engine.
        void click(double where) {
            cursor_ = static_cast<size_t>(where * static_cast<double>(text_.size()));
            if (clicksSeen_)
                g_mouse_clicked.store(true, std::memory_order_release);
            report();
        }

        // Applies what the engine sent since the last call.
        void sync() {
            if (applied_ == context_->sent.size())
                return;
            for (; applied_ < context_->sent.size(); ++applied_) {
                const Sent& s = context_->sent[applied_];
                if (s.kind == Sent::Commit) {
                    insert(s.text);
                } else if (s.kind == Sent::BackSpace) {
                    erase(1);
                } else {
                    const long from = static_cast<long>(cursor_) + s.offset;
                    if (from >= 0 && static_cast<size_t>(from) + s.size <= text_.size()) {
                        text_.erase(static_cast<size_t>(from), s.size);
                        if (cursor_ > static_cast<size_t>(from))
                            cursor_ = cursor_ >= static_cast<size_t>(from) + s.size ? cursor_ - s.size : static_cast<size_t>(from);
                    }
                }
            }
            report();
        }

        std::string text() const {
            return encode(text_);
        }
        const std::u32string& chars() const {
            return text_;
        }
        size_t cursor() const {
            return cursor_;
        }

      private:
        void insert(const std::u32string& s) {
            text_.insert(cursor_, s);
            cursor_ += s.size();
        }
        void erase(size_t n) {
            n = n < cursor_ ? n : cursor_;
            text_.erase(cursor_ - n, n);
            cursor_ -= n;
        }
        void report() {
            if (!surrounding_)
                return;
            context_->surroundingText().setText(encode(text_), static_cast<unsigned int>(cursor_), static_cast<unsigned int>(cursor_));
            context_->updateSurroundingText();
        }

        fcitx::NgoSenEngine&            engine_;
        const fcitx::InputMethodEntry&  entry_;
        std::unique_ptr<OrderedContext> context_;
        bool                            surrounding_;
        bool                            clicksSeen_;
        std::u32string                  text_;
        size_t                          cursor_  = 0;
        size_t                          applied_ = 0;
    };

    struct Step {
        enum Kind {
            Key,
            Click,
            Switch
        } kind;
        uint32_t sym     = 0;     // Key
        double   where   = 0;     // Click, or Switch by click: where in the text
        int      app     = 0;     // Switch: the app to go to
        int      gapMs   = 0;     // wait after the step
        bool     byClick = false; // Switch
    };

    const char* const words[] = {"vieetj", "nguowif", "dduwowcj", "tooi", "khoong", "hoaf", "thuyeets", "ddi", "chuaw", "quas", "giof", "muwaf", "xin", "chaof", "banj"};

    int               keyGap(std::mt19937& rng) {
        return static_cast<int>(rng() % 5 == 0 ? 8 + rng() % 18 : 40 + rng() % 120);
    }

    // An X11 GTK app, where the engine sees clicks; a Wayland app, where it only sees the cursor
    // move; and an XIM app that reports no text at all.
    struct AppKind {
        const char* program;
        const char* frontend;
        bool        surrounding;
        bool        clicksSeen;
    };
    const AppKind     kinds[]  = {{"gedit", "dbus", true, true}, {"gtk4app", "wayland", true, false}, {"xterm", "xim", false, true}};
    const int         appCount = static_cast<int>(sizeof(kinds) / sizeof(kinds[0]));

    std::vector<Step> scenario(std::mt19937& rng) {
        std::vector<Step> steps;
        int               current = 0;
        for (int w = 0; w < 10; ++w) {
            const char* word = words[rng() % (sizeof(words) / sizeof(words[0]))];
            // Stop part way through a word now and then, so clicks and switches land mid-word.
            const size_t len  = std::char_traits<char>::length(word);
            const size_t stop = rng() % 3 == 0 ? 1 + rng() % len : len;
            for (size_t i = 0; i < stop; ++i)
                steps.push_back({Step::Key, static_cast<uint32_t>(word[i]), 0, 0, keyGap(rng)});
            if (stop == len && rng() % 2 == 0)
                steps.push_back({Step::Key, FcitxKey_space, 0, 0, keyGap(rng)});
            if (rng() % 8 == 0)
                steps.push_back({Step::Key, FcitxKey_BackSpace, 0, 0, keyGap(rng)});
            switch (rng() % 4) {
                case 0:
                    // Reaching for the mouse takes a moment, unless one hand already holds it. Moves
                    // sooner after a key are taken for the app settling its caret, not for clicks.
                    steps.back().gapMs = rng() % 3 == 0 ? 40 + static_cast<int>(rng() % 60) : 150 + static_cast<int>(rng() % 250);
                    steps.push_back({Step::Click, 0, static_cast<double>(rng() % 101) / 100.0, 0, 60 + static_cast<int>(rng() % 200)});
                    break;
                case 1: {
                    // Alt+Tab can follow the last key closely; a click into the other window lands
                    // somewhere in its text.
                    const bool byClick = rng() % 2 == 0;
                    steps.back().gapMs = 30 + static_cast<int>(rng() % 90);
                    const int other    = (current + 1 + static_cast<int>(rng() % (appCount - 1))) % appCount;
                    steps.push_back({Step::Switch, 0, static_cast<double>(rng() % 101) / 100.0, other, 40 + static_cast<int>(rng() % 120), byClick});
                    current = other;
                    break;
                }
                default: break;
            }
        }
        steps.push_back({Step::Key, FcitxKey_space, 0, 0, 300});
        return steps;
    }

    void wait(TestInstance& t, std::vector<std::unique_ptr<App>>& apps, int ms) {
        for (int i = 0; i < ms; ++i) {
            pumpEventLoop(t.instance, 1);
            for (auto& app : apps)
                app->sync();
        }
    }

    std::vector<std::string> typeScenario(TestInstance& t, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, const std::vector<Step>& steps) {
        std::vector<std::unique_ptr<App>> apps;
        for (const auto& k : kinds)
            apps.push_back(std::make_unique<App>(t, engine, entry, k.program, k.frontend, k.surrounding, k.clicksSeen));
        int current = 0;
        apps[0]->focusIn();
        for (const Step& s : steps) {
            if (s.kind == Step::Key) {
                apps[current]->press(s.sym);
            } else if (s.kind == Step::Click) {
                apps[current]->click(s.where);
            } else {
                apps[current]->focusOut();
                current = s.app;
                apps[current]->focusIn();
                if (s.byClick)
                    apps[current]->click(s.where);
            }
            wait(t, apps, s.gapMs);
        }
        apps[current]->focusOut();
        wait(t, apps, 100);
        std::vector<std::string> texts;
        for (auto& app : apps)
            texts.push_back(app->text());
        g_mouse_clicked.store(false);
        return texts;
    }

    // Types one run of keys calmly into a fresh field of the same kind holding text, with the cursor
    // at cursor, and leaves the field's text and cursor there.
    void typeCalmly(TestInstance& t, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, const AppKind& kind, const std::vector<uint32_t>& keys,
                    std::u32string& text, size_t& cursor) {
        std::vector<std::unique_ptr<App>> apps;
        apps.push_back(std::make_unique<App>(t, engine, entry, kind.program, kind.frontend, kind.surrounding, kind.clicksSeen));
        App& app = *apps.front();
        app.setText(text, cursor);
        app.focusIn();
        wait(t, apps, 20);
        for (uint32_t sym : keys) {
            app.press(sym);
            wait(t, apps, 120);
        }
        app.focusOut();
        wait(t, apps, 100);
        text   = app.chars();
        cursor = app.cursor();
    }

    // The texts the apps should end with: each run of keys between clicks and switches gives what
    // it gives typed calmly at the same place.
    std::vector<std::string> expectedTexts(TestInstance& t, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, const std::vector<Step>& steps) {
        std::vector<std::u32string> texts(appCount);
        std::vector<size_t>         cursors(appCount, 0);
        int                         current = 0;
        std::vector<uint32_t>       run;
        auto                        endRun = [&] {
            if (!run.empty())
                typeCalmly(t, engine, entry, kinds[current], run, texts[current], cursors[current]);
            run.clear();
        };
        for (const Step& s : steps) {
            if (s.kind == Step::Key) {
                run.push_back(s.sym);
                continue;
            }
            endRun();
            if (s.kind == Step::Switch)
                current = s.app;
            if (s.kind == Step::Click || s.byClick)
                cursors[current] = static_cast<size_t>(s.where * static_cast<double>(texts[current].size()));
        }
        endRun();
        std::vector<std::string> out;
        for (const auto& text : texts)
            out.push_back(encode(text));
        return out;
    }

    std::string describe(const std::vector<Step>& steps) {
        std::string out;
        for (const Step& s : steps) {
            if (s.kind == Step::Key)
                out += s.sym == FcitxKey_BackSpace ? std::string("<BS>") : std::string(1, static_cast<char>(s.sym));
            else if (s.kind == Step::Click)
                out += "<click " + std::to_string(static_cast<int>(s.where * 100)) + "%>";
            else
                out += "<" + std::string(s.byClick ? "click into " : "alt-tab to ") + kinds[s.app].program +
                    (s.byClick ? " " + std::to_string(static_cast<int>(s.where * 100)) + "%" : "") + ">";
        }
        return out;
    }

    unsigned long envNumber(const char* name, unsigned long fallback) {
        const char* value = std::getenv(name);
        return value != nullptr && *value != '\0' ? std::strtoul(value, nullptr, 10) : fallback;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-click-switch-fuzz");
    if (std::getenv("NGOSEN_TEST_LOG") != nullptr)
        fcitx::Log::setLogRule("ngosen=5");
    TestInstance        t;
    fcitx::NgoSenEngine engine(&t.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");

    const unsigned long     runs     = envNumber("NGOSEN_FUZZ_RUNS", 2);
    const unsigned long     seedBase = envNumber("NGOSEN_FUZZ_SEED", 1);
    const bool              oneSeed  = std::getenv("NGOSEN_FUZZ_SEED") != nullptr;
    int                     failures = 0;
    for (unsigned long run = 0; run < (oneSeed ? 1 : runs); ++run) {
        const unsigned long seed = seedBase + run;
        std::mt19937        rng(static_cast<std::mt19937::result_type>(seed));
        const auto          steps = scenario(rng);
        engine.recorder().clear();
        const auto        seen   = typeScenario(t, engine, entry, steps);
        const std::string typing = engine.recorder().dump();
        const auto        want   = expectedTexts(t, engine, entry, steps);
        for (size_t i = 0; i < seen.size(); ++i) {
            if (seen[i] != want[i]) {
                ++failures;
                std::printf("FAIL: seed %lu, %s: \"%s\", want \"%s\"\n  steps: %s\n", seed, kinds[i].program, seen[i].c_str(), want[i].c_str(), describe(steps).c_str());
            }
        }
        // One repeated scenario also shows its typing log.
        if (oneSeed && failures > 0)
            std::printf("%s", typing.c_str());
        std::fflush(stdout);
    }
    return failures == 0 ? 0 : 1;
}
