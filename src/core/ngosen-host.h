/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

    // Where the user is in the candidate list shown in the panel. Indexes count from the first
    // candidate of the whole list; cursor is -1 when nothing is highlighted.
    struct CandidatePage {
        int  total    = 0;
        int  page     = 0;
        int  pageSize = 0;
        int  cursor   = -1;
        bool hasNext  = false;
        bool hasPrev  = false;
    };

    // A timer from Host::startTimer. Destroying it cancels it; never destroy it from its own callback.
    class Timer {
      public:
        virtual ~Timer() = default;
        // Runs the callback once more at deadlineUs; the callback then returns true to keep the timer.
        virtual void rearm(uint64_t deadlineUs) = 0;
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

        // Shows labels as a vertical list, pageSize per page, with the first highlighted. Picking one,
        // by key or by mouse, calls onPick with its index.
        virtual void showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) = 0;
        virtual void hideCandidates()                                                                                         = 0;
        // Empty when the panel shows no candidate list.
        virtual std::optional<CandidatePage> candidates() const            = 0;
        virtual void                         highlightCandidate(int index) = 0;
        virtual void                         nextCandidatePage()           = 0;
        virtual void                         prevCandidatePage()           = 0;
        virtual void                         pickCandidate(int index)      = 0;
        // A line under the candidates, such as the page number.
        virtual void setStatus(const std::string& text) = 0;
        // text in the user's language. Messages.sh collects the literals passed here.
        virtual std::string translate(const char* text) const = 0;

        // Calls onTime at deadlineUs on the CLOCK_MONOTONIC clock, give or take accuracyUs, from the
        // input method's event loop. onTime returns false unless it rearmed the timer.
        virtual std::unique_ptr<Timer> startTimer(uint64_t deadlineUs, uint64_t accuracyUs, std::function<bool(Timer&)> onTime) = 0;

        // The text a key symbol types, empty for keys that type none.
        virtual std::string keyText(uint32_t sym) const = 0;
    };

} // namespace ngosen
