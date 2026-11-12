// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-pty.h"

#include <pty.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace {
    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    termios withFlags(tcflag_t lflag) {
        termios mode{};
        mode.c_lflag = lflag;
        return mode;
    }

    void testEveryEchoCombination() {
        // Only echo off with line reading or a newline echo is a prompt hiding input.
        check(!ngosen::hidesInput(withFlags(0)), "raw mode (shell, editor)");
        check(ngosen::hidesInput(withFlags(ICANON)), "sudo, passwd, getpass");
        check(ngosen::hidesInput(withFlags(ECHONL)), "sudo-rs");
        check(ngosen::hidesInput(withFlags(ICANON | ECHONL)), "line reading with newline echo");
        check(!ngosen::hidesInput(withFlags(ECHO)), "echo, raw");
        check(!ngosen::hidesInput(withFlags(ECHO | ICANON)), "cooked mode (cat)");
        check(!ngosen::hidesInput(withFlags(ECHO | ECHONL)), "echo with newline echo");
        check(!ngosen::hidesInput(withFlags(ECHO | ICANON | ECHONL)), "cooked mode with newline echo");
    }

    void testRealPty() {
        int  master = -1;
        int  slave  = -1;
        char name[64]{};
        if (openpty(&master, &slave, name, nullptr, nullptr) != 0) {
            check(false, "openpty");
            return;
        }
        const std::vector<std::string> paths{name};
        termios                        mode{};
        tcgetattr(slave, &mode);
        check(!ngosen::anyHidesInput(paths), "a new pty echoes");

        mode.c_lflag &= ~ECHO;
        tcsetattr(slave, TCSANOW, &mode);
        check(ngosen::anyHidesInput(paths), "echo turned off as at a password prompt");

        mode.c_lflag |= ECHO;
        tcsetattr(slave, TCSANOW, &mode);
        check(!ngosen::anyHidesInput(paths), "echo back on");

        check(!ngosen::anyHidesInput({"/dev/null", "/nonexistent/pts/0", "/etc/hostname"}), "paths that are not terminals");
        close(slave);
        close(master);
    }
} // namespace

int main() {
    testEveryEchoCombination();
    testRealPty();
    if (failures == 0)
        std::cout << "core_pty: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
