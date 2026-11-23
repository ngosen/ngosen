// SPDX-License-Identifier: GPL-3.0-or-later
//
// Emoji are off until the user turns them on for an app. The settings window reads and writes those
// apps and the user's emoji words, and a changed list reaches the field being typed in at once (#110).
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    void focus(fcitx::NgoSenEngine& engine, TestInputContext& context) {
        context.focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, event);
    }

    fcitx::RawConfig subConfig(const fcitx::NgoSenEngine& engine, const std::string& path) {
        fcitx::RawConfig raw;
        if (const auto* config = engine.getSubConfig(path))
            config->save(raw);
        return raw;
    }

    std::string valueAt(const fcitx::RawConfig& raw, const std::string& path) {
        const auto* value = raw.valueByPath(path);
        return value ? *value : "(none)";
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream      file(path);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

} // namespace

int main() {
    const char* testName = "fcitx5-ngosen-emoji-settings";
    configureTestPaths(testName);
    const auto confDir = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf";
    std::filesystem::create_directories(confDir);
    std::ofstream(confDir / "ngosen.conf", std::ios::trunc) << "Mode=Sen\nVietnameseOn=True\nInputMethod=Telex\n";
    std::ofstream(confDir / "ngosen-emoji-apps.conf", std::ios::trunc) << "# comment\nkate\n";
    std::ofstream(confDir / "ngosen-emoji-words.conf", std::ios::trunc) << "[Words]\n[Words/0]\nKeyword=sếp\nEmoji=👔\nOn=True\n";

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    TestInputContext    kate(&testInstance.instance, "kate");
    TestInputContext    gedit(&testInstance.instance, "gedit");

    const auto          apps = subConfig(engine, "emoji_apps");
    check(valueAt(apps, "Apps/0/App") == "kate" && valueAt(apps, "Apps/1/App") == "(none)", "the window reads the apps with emoji, got " + valueAt(apps, "Apps/0/App"));
    const auto words = subConfig(engine, "emoji_words");
    check(valueAt(words, "Words/0/Keyword") == "sếp" && valueAt(words, "Words/0/Emoji") == "👔", "the window reads the saved words, got " + valueAt(words, "Words/0/Keyword"));
    const auto& edits = engine.options().emojiWordEdits;
    check(edits.size() == 1 && edits.front().keyword == "sếp" && edits.front().on, "the saved words reach the typing logic");

    focus(engine, kate);
    check(engine.emojiOnIn(&kate), "kate, turned on, has emoji");
    focus(engine, gedit);
    check(!engine.emojiOnIn(&gedit), "gedit, never turned on, has none");

    fcitx::RawConfig onlyGedit;
    onlyGedit.setValueByPath("Apps/0/App", "gedit");
    engine.setSubConfig("emoji_apps", onlyGedit);
    check(engine.emojiOnIn(&gedit), "turning gedit on in the window reaches the focused field");
    check(!engine.emojiOnIn(&kate), "turning kate off reaches its field");
    const std::string saved = readFile(confDir / "ngosen-emoji-apps.conf");
    check(saved.find("gedit\n") != std::string::npos && saved.find("kate") == std::string::npos, "the list is saved, got " + saved);

    focus(engine, gedit);
    check(engine.emojiOnIn(&gedit), "gedit is on before the reload");
    std::ofstream(confDir / "ngosen-emoji-apps.conf", std::ios::trunc) << "kate\n";
    engine.reloadConfig();
    check(!engine.emojiOnIn(&gedit), "a reloaded list reaches the focused field");

    fcitx::RawConfig turnedOff;
    turnedOff.setValueByPath("Words/0/Keyword", "cười");
    turnedOff.setValueByPath("Words/0/Emoji", "😂");
    turnedOff.setValueByPath("Words/0/On", "False");
    engine.setSubConfig("emoji_words", turnedOff);
    check(edits.size() == 1 && edits.front().keyword == "cười" && !edits.front().on, "edited words reach the typing logic");
    const std::string savedWords = readFile(confDir / "ngosen-emoji-words.conf");
    check(savedWords.find("Keyword=cười") != std::string::npos && savedWords.find("sếp") == std::string::npos, "the words are saved, got " + savedWords);

    return failures == 0 ? 0 : 1;
}
