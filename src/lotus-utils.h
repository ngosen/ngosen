/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-utils.h
 * @brief Utility functions and global state for fcitx5-lotus.
 */

#ifndef _FCITX5_LOTUS_UTILS_H_
#define _FCITX5_LOTUS_UTILS_H_

#include <atomic>
#include <fcitx-utils/log.h>
#include <fcitx/inputcontext.h>

#include "lotus-config.h"

FCITX_DECLARE_LOG_CATEGORY(lotus);

#define LOTUS_DEBUG(msg) FCITX_LOGC(lotus, Debug) << "[DEBUG] " << msg
#define LOTUS_INFO(msg)  FCITX_LOGC(lotus, Info) << "[INFO] " << msg
#define LOTUS_WARN(msg)  FCITX_LOGC(lotus, Warn) << "[WARN] " << msg
#define LOTUS_ERROR(msg) FCITX_LOGC(lotus, Error) << "[ERROR] " << msg

// Forward declaration for fcitx types
using KeySym = uint32_t;

// Global state variables for input processing
extern std::atomic<fcitx::LotusMode> realMode;          ///< Current active input mode
extern std::atomic<bool>             needEngineReset;   ///< Flag to trigger engine reset
extern std::atomic<bool>             g_mouse_clicked;   ///< Mouse click detection flag
extern std::atomic<bool>             is_deleting_;      ///< Deletion in progress flag
extern std::atomic<bool>             stop_flag_monitor; ///< Signal to stop monitor threads
extern std::atomic<unsigned int>     realtextLen;       ///< Current text length

/**
 * @brief Gets current time in milliseconds.
 * @return Timestamp in milliseconds.
 */
int64_t now_ms();

/**
 * @brief Checks if key symbol is a backspace.
 * @param sym Key symbol to check.
 * @return True if backspace.
 */
bool isBackspace(uint32_t sym);

/**
 * @brief Whether a mode delivers text through the uinput (fake backspace) path.
 * @param mode Mode to check.
 * @return True for Sen.
 */
bool isUinputMode(fcitx::LotusMode mode);

/**
 * @brief Erases the last UTF-8 codepoint from a string in place.
 *
 * Walks back past any continuation bytes (10xxxxxx) to find the leading
 * byte, then erases from that position. Correctly handles 2/3/4-byte
 * sequences; no-op on empty input. Used by the emoji-mode backspace
 * handler so the preedit stays valid UTF-8 after each backspace.
 */
void eraseLastUtf8Codepoint(std::string& buffer);

/**
 * @brief Compares two strings and computes diff.
 * @param A First string.
 * @param B Second string.
 * @param deletedPart Output deleted portion.
 * @param addedPart Output added portion.
 * @return Comparison result code.
 */
int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& deletedPart, std::string& addedPart);

/**
 * @brief Checks if string starts with prefix.
 * @param str String to check.
 * @param prefix Prefix to check.
 * @return True if string starts with prefix.
 */
bool isStartsWith(const std::string& str, const std::string& prefix);

/**
 * @brief Get the frontend name from the input context.
 * @param ic Input context.
 * @return Frontend name.
 */
std::string getFrontendName(fcitx::InputContext* ic);

/**
 * @brief Drop surrounding text the client cannot have sent.
 *
 * GNOME Shell multiplexes every window through one IBus input context, and
 * fcitx5 keeps the last SetSurroundingText when focus moves to a client that
 * does not advertise surrounding support (e.g. kitty). Without this, the
 * engine reads the previous window's text as if it belonged to the current one.
 * @param ic Input context.
 * @return True if stale text was dropped.
 */
bool dropStaleSurroundingText(fcitx::InputContext* ic);

/**
 * @brief Strip the ".desktop" suffix GNOME adds to program names.
 *
 * Under GNOME's IBus path a window reports "firefox.desktop" where KDE reports
 * "firefox"; rules and app checks are written against the bare name.
 * @param program Program name as reported by the frontend.
 * @return Program name without a trailing ".desktop".
 */
std::string stripDesktopSuffix(const std::string& program);

/**
 * @brief Whether surrounding-text updates for this context arrive too late to wait on.
 *
 * Under GNOME, apps reach fcitx5 through GNOME Shell's IBus bridge, which forwards the
 * client's surrounding text tens of milliseconds late and often one edit behind.
 * @param ic Input context.
 * @return True for contexts served by the IBus frontend.
 */
bool surroundingTextLags(fcitx::InputContext* ic);

// True when the frontend can deliver backspaces itself, so XTEST is not needed: the
// Wayland input-method frontend, XIM clients, and IBus, D-Bus and fcitx4 clients other than SDL.
bool forwardsBackspaces(fcitx::InputContext* ic);

// True for GTK4 clients, whose IM modules drop forwarded keys; delete through surrounding text there.
bool ignoresForwardedKeys(fcitx::InputContext* ic);

// True for Chromium-based browsers on the D-Bus and fcitx4 frontends: they take forwarded keys but report
// no surrounding text, and their address bar selects an inline autocompletion after the typed text.
bool selectsOverAutocompletion(fcitx::InputContext* ic);

/**
 * @brief Key event entry for replay buffer.
 */
struct KeyEntry {
    uint32_t sym;   ///< Key symbol
    uint32_t state; ///< Key state (modifiers)
};

/**
 * @brief get environement variable.
 * @param name Name of the variable.
 * @return Value of the variable.
 */
std::string getEnv(const std::string& name);

#endif // _FCITX5_LOTUS_UTILS_H_
