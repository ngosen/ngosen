/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <utility>

namespace ngosen {

    // Keys the typing logic sends to the app on its own, outside the user's key presses.
    enum class EditKey {
        BackSpace,
        Right,
    };

    // The text around the cursor as the app last reported it. Positions count characters.
    class Surrounding {
      public:
        Surrounding() = default;
        Surrounding(std::string text, unsigned int cursor, unsigned int anchor) : valid_(true), text_(std::move(text)), cursor_(cursor), anchor_(anchor) {}

        bool isValid() const {
            return valid_;
        }
        const std::string& text() const {
            return text_;
        }
        unsigned int cursor() const {
            return cursor_;
        }
        unsigned int anchor() const {
            return anchor_;
        }

      private:
        bool         valid_ = false;
        std::string  text_;
        unsigned int cursor_ = 0;
        unsigned int anchor_ = 0;
    };

    // What the app told the input method about the focused field.
    struct Field {
        std::string frontend; // how the app talks to the input method: wayland, xim, ibus, dbus, fcitx4
        std::string program;
        bool        surroundingText  = false; // the app reports the text around the cursor
        bool        preedit          = false; // the app can show uncommitted text
        bool        formattedPreedit = false;
        bool        url              = false; // the field is an address bar
        bool        keyEventOrderFix = false;
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

        // Read fresh on every call: the app may report a new state between two calls.
        virtual Surrounding surrounding() const = 0;
        virtual Field       field() const       = 0;
        virtual bool        hasFocus() const    = 0;

        // Shows text not committed yet: in the app when it can draw it, otherwise in the panel.
        virtual void showPreedit(const std::string& text, bool underline) = 0;
        virtual void clearPreedit()                                       = 0;
        // Clears the preedit, the candidates and the status line of the panel.
        virtual void resetPanel() = 0;
        // Changes above reach the app and the panel only when refreshed.
        virtual void refreshPreedit() = 0;
        virtual void refreshPanel()   = 0;
    };

} // namespace ngosen
