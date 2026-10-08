/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-globals.h"

std::atomic<ngosen::Mode> realMode{ngosen::Mode::Sen};
std::atomic<bool>         needEngineReset{false};
std::atomic<bool>         g_mouse_clicked{false};
std::atomic<bool>         is_deleting_{false};
std::atomic<bool>         stop_flag_monitor{false};
std::atomic<unsigned int> realtextLen{0};
