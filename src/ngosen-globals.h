/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-mode.h"

#include <atomic>

// State shared by the engine, every field's typing logic and the helper threads.
extern std::atomic<ngosen::Mode> realMode;          ///< Current active input mode
extern std::atomic<bool>         needEngineReset;   ///< Flag to trigger engine reset
extern std::atomic<bool>         g_mouse_clicked;   ///< Mouse click detection flag
extern std::atomic<bool>         is_deleting_;      ///< Deletion in progress flag
extern std::atomic<bool>         stop_flag_monitor; ///< Signal to stop monitor threads
extern std::atomic<unsigned int> realtextLen;       ///< Current text length
