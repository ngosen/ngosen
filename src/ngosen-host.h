/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // Keys the typing logic sends to the app on its own, outside the user's key presses.
    enum class EditKey {
        BackSpace,
        Right,
    };

    // What the typing logic needs from the input method framework for one text field, so that the
    // logic does not depend on fcitx5 and can be reused by another framework.
    class Host {
      public:
        virtual ~Host() = default;

        virtual void commitText(const std::string& text)   = 0;
        virtual void forwardKey(EditKey key, bool release) = 0;
        // Deletes size characters starting offset characters from the cursor.
        virtual void deleteSurrounding(int offset, unsigned int size) = 0;
        // Presses keys at the X server: count > 0 presses BackSpace count times, count < 0 selects
        // -count characters with Shift+Left. False when that is unavailable.
        virtual bool pressSystemKeys(int count) = 0;
    };

} // namespace ngosen
