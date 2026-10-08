/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

/**
 * @brief Checks if string starts with prefix.
 * @param str String to check.
 * @param prefix Prefix to check.
 * @return True if string starts with prefix.
 */
bool isStartsWith(const std::string& str, const std::string& prefix);

/**
 * @brief Strip the ".desktop" suffix GNOME adds to program names.
 *
 * Under GNOME's IBus path a window reports "firefox.desktop" where KDE reports
 * "firefox"; rules and app checks are written against the bare name.
 * @param program Program name as reported by the frontend.
 * @return Program name without a trailing ".desktop".
 */
std::string stripDesktopSuffix(const std::string& program);
