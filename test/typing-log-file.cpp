// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tray menu and the ngosen-save-log command both save the typing log through writeTypingLog.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <sys/stat.h>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream in(path);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-typing-log-file");
    const auto root = std::filesystem::temp_directory_path() / "fcitx5-ngosen-typing-log-file";
    setenv("XDG_STATE_HOME", (root / "state").c_str(), 1);
    // The common default, under which files are readable by everyone.
    umask(022);

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    engine.recorder().add("commit", "\"việt\"");

    const auto path = engine.writeTypingLog();
    check("the log is saved under XDG_STATE_HOME/ngosen", path.parent_path() == root / "state" / "ngosen");
    const std::string text = readFile(path);
    check("the log starts with its header", text.rfind("# Ngó Sen ", 0) == 0);
    check("the log holds the recorded events", text.find("commit\t\"việt\"") != std::string::npos);
    using std::filesystem::perms;
    check("only the user can open the log directory", std::filesystem::status(path.parent_path()).permissions() == perms::owner_all);
    check("only the user can read the log", std::filesystem::status(path).permissions() == (perms::owner_read | perms::owner_write));

    // A file where the state directory should be: nothing can be written there.
    std::ofstream(root / "blocked").put('x');
    setenv("XDG_STATE_HOME", (root / "blocked").c_str(), 1);
    check("a failed save returns an empty path", engine.writeTypingLog().empty());

    return failures == 0 ? 0 : 1;
}
