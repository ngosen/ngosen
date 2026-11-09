/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-pty.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>

namespace ngosen {

    bool hidesInput(const termios& mode) {
        return (mode.c_lflag & ECHO) == 0 && (mode.c_lflag & (ICANON | ECHONL)) != 0;
    }

    bool anyHidesInput(const std::vector<std::string>& paths) {
        const uid_t user = getuid();
        for (const auto& path : paths) {
            struct stat st{};
            if (stat(path.c_str(), &st) != 0 || st.st_uid != user || !S_ISCHR(st.st_mode))
                continue;
            // Without O_NOCTTY the open could make another session's terminal the controlling one of fcitx5.
            const int fd = open(path.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                continue;
            termios    mode{};
            const bool read = tcgetattr(fd, &mode) == 0;
            close(fd);
            if (read && hidesInput(mode))
                return true;
        }
        return false;
    }

    bool anyPtyHidesInput() {
        std::vector<std::string> paths;
        if (DIR* dir = opendir("/dev/pts")) {
            while (const dirent* entry = readdir(dir)) {
                if (std::isdigit(static_cast<unsigned char>(entry->d_name[0])))
                    paths.push_back(std::string("/dev/pts/") + entry->d_name);
            }
            closedir(dir);
        }
        return anyHidesInput(paths);
    }

} // namespace ngosen
