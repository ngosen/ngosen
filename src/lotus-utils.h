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

#include <fcitx-utils/log.h>
#include <fcitx/inputcontext.h>

#include "lotus-config.h"
#include "ngosen-globals.h"
#include "ngosen-strings.h"

FCITX_DECLARE_LOG_CATEGORY(lotus);

#define LOTUS_DEBUG(msg) FCITX_LOGC(lotus, Debug) << "[DEBUG] " << msg
#define LOTUS_INFO(msg)  FCITX_LOGC(lotus, Info) << "[INFO] " << msg
#define LOTUS_WARN(msg)  FCITX_LOGC(lotus, Warn) << "[WARN] " << msg
#define LOTUS_ERROR(msg) FCITX_LOGC(lotus, Error) << "[ERROR] " << msg

// Forward declaration for fcitx types
using KeySym = uint32_t;

/**
 * @brief Gets current time in milliseconds.
 * @return Timestamp in milliseconds.
 */
int64_t now_ms();

/**
 * @brief Whether a mode delivers text through the uinput (fake backspace) path.
 * @param mode Mode to check.
 * @return True for Sen.
 */
bool isUinputMode(fcitx::LotusMode mode);

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
 * @brief get environement variable.
 * @param name Name of the variable.
 * @return Value of the variable.
 */
std::string getEnv(const std::string& name);

#endif // _FCITX5_LOTUS_UTILS_H_
