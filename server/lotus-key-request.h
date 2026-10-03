/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _LOTUS_KEY_REQUEST_H_
#define _LOTUS_KEY_REQUEST_H_

#include <sys/types.h>

/**
 * @brief One request from the addon: a single int, N > 0 sends N backspaces, -N selects N chars.
 */
struct KeyRequest {
    enum class Kind {
        Invalid,
        Backspace,
        Select,
    };
    Kind kind  = Kind::Invalid;
    int  count = 0;
};

// Far above any word the addon replaces; a huge select would allocate its key events up front.
constexpr int     kMaxKeysPerRequest = 1024;

inline KeyRequest parseKeyRequest(ssize_t bytes, int value) {
    if (bytes != static_cast<ssize_t>(sizeof(int)) || value == 0 || value > kMaxKeysPerRequest || value < -kMaxKeysPerRequest) {
        return {};
    }
    if (value > 0) {
        return {KeyRequest::Kind::Backspace, value};
    }
    return {KeyRequest::Kind::Select, -value};
}

#endif // _LOTUS_KEY_REQUEST_H_
