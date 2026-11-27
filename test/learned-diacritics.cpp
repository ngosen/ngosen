// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Word guessing page saves and forgets what Đoán dấu learned. Forgetting must reach memory too,
// even with Đoán dấu off, or the save at exit writes it all back.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    fcitx::RawConfig config(bool guess) {
        fcitx::RawConfig c;
        c.setValueByPath("Mode", "Preedit");
        c.setValueByPath("VietnameseOn", "True");
        c.setValueByPath("InputMethod", "Telex");
        c.setValueByPath("GuessDiacritics", guess ? "True" : "False");
        c.setValueByPath("LearnDiacritics", "True");
        return c;
    }

    void type(fcitx::NgoSenEngine& engine, TestInputContext& context, const std::string& text) {
        const fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
        for (const char c : text) {
            const fcitx::Key key(c == ' ' ? FcitxKey_space : static_cast<fcitx::KeySym>(c));
            fcitx::KeyEvent  down(&context, key, false);
            engine.keyEvent(entry, down);
            if (!down.accepted())
                context.commitString(std::string(1, c));
            fcitx::KeyEvent up(&context, key, true);
            engine.keyEvent(entry, up);
        }
    }

    int lines(const std::filesystem::path& path) {
        std::ifstream file(path);
        std::string   line;
        int           count = 0;
        while (std::getline(file, line))
            ++count;
        return count;
    }

    void forgetsWhatWasLearnedWithGuessingOff(const std::filesystem::path& learned) {
        TestInstance testInstance;
        {
            fcitx::NgoSenEngine engine(&testInstance.instance);
            engine.setConfig(config(true));
            check(engine.diacriticsStatus() == std::pair<std::string, std::string>{"loaded", ""}, "the table loads");
            TestInputContext context(&testInstance.instance, "kate");
            context.setCapabilityFlags({fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText});
            context.focusIn();
            fcitx::InputContextEvent focus(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(fcitx::InputMethodEntry("ngosen", "Ngó Sen", "vi", "ngosen"), focus);
            type(engine, context, "ngos sen. ");
            check(engine.saveLearned().first == "saved", "what was typed is saved");
            check(lines(learned) > 1, "the file holds what was learned");
            check(engine.saveLearned().first == "unchanged", "nothing new to save");
            type(engine, context, "gox phims. ");
            engine.setConfig(config(false));
            check(engine.forgetLearned().first, "forgetting with Đoán dấu off works");
            check(lines(learned) == 1, "the file is empty after forgetting");
        }
        check(lines(learned) == 1, "the save at exit writes nothing back");
    }

    void forgetsTheFilesWhenNeverOpened(const std::filesystem::path& learned) {
        const auto dir = learned.parent_path();
        for (const char* name : {"learned-diacritics", ".learned-diacritics.tmp", "learned-diacritics.broken", "learned-diacritics.broken.1"})
            std::ofstream(dir / name) << "timsen-personal 1\n";
        TestInstance        testInstance;
        fcitx::NgoSenEngine engine(&testInstance.instance);
        engine.setConfig(config(false));
        check(engine.diacriticsStatus().first == "off", "never opened");
        check(engine.saveLearned().first == "unchanged", "nothing to save without Đoán dấu");
        check(engine.forgetLearned().first, "forgetting without Đoán dấu works");
        for (const auto& entry : std::filesystem::directory_iterator(dir))
            check(entry.path().filename().string().find("learned-diacritics") == std::string::npos, "removed " + entry.path().filename().string());
    }

} // namespace

int main() {
    const char* testName = "fcitx5-ngosen-learned-diacritics";
    configureTestPaths(testName);
    // Ngó Sen's own data, not fcitx5's: $XDG_DATA_HOME/ngosen.
    const auto dir = std::filesystem::temp_directory_path() / testName / "data/ngosen";
    std::filesystem::create_directories(dir);
    std::filesystem::copy_file(NGOSEN_TEST_DIACRITICS, dir / "diacritics.timsen");
    forgetsWhatWasLearnedWithGuessingOff(dir / "learned-diacritics");

    configureTestPaths(testName);
    std::filesystem::create_directories(dir);
    forgetsTheFilesWhenNeverOpened(dir / "learned-diacritics");
    return failures == 0 ? 0 : 1;
}
