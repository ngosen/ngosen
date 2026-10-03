/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-utils.h"
#include "lotus-config.h"

#include <cstddef>
#include <cstdlib>
#include <fcitx-utils/utf8.h>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

// Global variables
std::atomic<fcitx::LotusMode> realMode{fcitx::LotusMode::Uinput};
std::atomic<bool>             needEngineReset{false};
std::atomic<bool>             g_mouse_clicked{false};
std::atomic<bool>             is_deleting_{false};
std::atomic<bool>             stop_flag_monitor{false};
std::atomic<int>              uinput_client_fd_{-1};
std::atomic<unsigned int>     realtextLen{0};
std::atomic<int>              mouse_socket_fd{-1};

FCITX_DEFINE_LOG_CATEGORY(lotus, "lotus", fcitx::LogLevel::NoLog);

std::string buildSocketPath(const char* base_path_suffix) {
    struct passwd  pwd{};
    struct passwd* result   = nullptr;
    long           buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buf_size == -1) {
        buf_size = 16384;
    }
    std::vector<char> buf(buf_size);
    std::string       username;
    int               res = getpwuid_r(getuid(), &pwd, buf.data(), buf_size, &result);
    if (res == 0 && result != nullptr) {
        username = result->pw_name;
    } else {
        username = "unknown";
    }
    std::string path;
    path.reserve(32);
    path += "lotussocket-";
    path += username;
    path += '-';
    path += base_path_suffix;
    // Optional namespace so a test can own a private socket name instead of
    // fighting the running fcitx5-lotus-server for the well-known one.
    // Unset in production, where the addon and the server must agree on the name.
    if (const char* ns = std::getenv("LOTUS_SOCKET_NAMESPACE"); ns != nullptr && *ns != '\0') {
        path += '-';
        path += ns;
    }
    const size_t max_socket_path_length = UNIX_PATH_MAX - 1;
    path.resize(std::min(path.length(), max_socket_path_length));
    return path;
}

#ifndef LOTUS_UINPUT_PROXY_USER
#define LOTUS_UINPUT_PROXY_USER "uinput_proxy"
#endif

bool isTrustedServerUid(uid_t peer, uid_t proxy, uid_t self, bool privateNamespace) {
    if (peer == proxy && proxy != static_cast<uid_t>(-1)) {
        return true;
    }
    return privateNamespace && peer == self;
}

bool isTrustedServerSocket(int fd) {
    struct ucred cred{};
    socklen_t    len = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        LOTUS_ERROR("Failed to get peer credentials: " + std::string(strerror(errno)));
        return false;
    }

    uid_t          proxy = static_cast<uid_t>(-1);
    struct passwd  pwd{};
    struct passwd* result = nullptr;
    char           buf[1024];
    if (getpwnam_r(LOTUS_UINPUT_PROXY_USER, &pwd, buf, sizeof(buf), &result) == 0 && result != nullptr) {
        proxy = result->pw_uid;
    }

    const char* ns      = std::getenv("LOTUS_SOCKET_NAMESPACE");
    const bool  trusted = isTrustedServerUid(cred.uid, proxy, getuid(), ns != nullptr && *ns != '\0');
    if (!trusted) {
        LOTUS_WARN("Refusing uinput server socket owned by UID " + std::to_string(cred.uid));
    }
    return trusted;
}

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool isBackspace(uint32_t sym) {
    return sym == 65288 || sym == 8 || sym == FcitxKey_BackSpace;
}

bool isUinputMode(fcitx::LotusMode mode) {
    return mode == fcitx::LotusMode::Uinput || mode == fcitx::LotusMode::Minecraft;
}

int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& deletedPart, std::string& addedPart) {
    size_t i = 0;
    size_t j = 0;
    while (i < A.size() && j < B.size()) {
        unsigned int lenA = fcitx_utf8_char_len(&A[i]);
        unsigned int lenB = fcitx_utf8_char_len(&B[j]);
        if (lenA == 0 || lenB == 0) {
            break;
        }
        if (i + lenA > A.size() || j + lenB > B.size()) {
            break;
        }
        if (lenA == lenB && std::strncmp(&A[i], &B[j], lenA) == 0) {
            i += lenA;
            j += lenB;
        } else {
            break;
        }
    }

    deletedPart.assign(A, i);
    addedPart.assign(B, j);
    return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
}

bool isStartsWith(const std::string& str, const std::string& prefix) {
#if __cplusplus >= 202002L
    return str.starts_with(prefix);
#else
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
#endif
}

std::string getFrontendName(fcitx::InputContext* ic) {
    if (ic == nullptr) {
        return "unknown";
    }
    return ic->frontend();
}

void eraseLastUtf8Codepoint(std::string& buffer) {
    if (buffer.empty()) {
        return;
    }
    size_t pos = buffer.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(buffer[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    buffer.erase(pos);
}

std::string getEnv(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return ((value != nullptr) && ((*value) != 0)) ? value : "";
}