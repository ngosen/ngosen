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
bool isEndsWith(const std::string& str, const std::string& suffix);

// True for a file in a user's home, such as the custom dictionary the settings window writes.
bool isInHomeDirectory(const std::string& path);

/**
 * @brief Strip the ".desktop" suffix GNOME adds to program names.
 *
 * Under GNOME's IBus path a window reports "firefox.desktop" where KDE reports
 * "firefox"; rules and app checks are written against the bare name.
 * @param program Program name as reported by the frontend.
 * @return Program name without a trailing ".desktop".
 */
std::string stripDesktopSuffix(const std::string& program);

// The word with its tone mark (sắc, huyền, hỏi, ngã, nặng) taken off; circumflex, breve, horn and đ stay.
std::string withoutToneMark(const std::string& word);

// Vietnamese letters lie in these Latin blocks; quotes, ellipses and other symbols end a word.
bool isWordChar(char32_t c);
