/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "bamboo-core.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace ngosen {

    namespace {

        size_t byteOffset(const std::string& text, unsigned int chars) {
            size_t at = 0;
            for (unsigned int i = 0; i < chars && at < text.size(); ++i)
                at += std::max<size_t>(utf8::charBytes(std::string_view(text).substr(at)), 1);
            return at;
        }

        // The selection as the app reports it, empty when it reports none.
        std::string reportedSelection(const Surrounding& s) {
            if (!s.isValid() || s.cursor() == s.anchor() || !utf8::validate(s.text()))
                return {};
            const unsigned int from = std::min(s.cursor(), s.anchor());
            const unsigned int to   = std::max(s.cursor(), s.anchor());
            if (to > utf8::length(s.text()))
                return {};
            const size_t begin = byteOffset(s.text(), from);
            return s.text().substr(begin, byteOffset(s.text(), to) - begin);
        }

        // Apps that report no selection put their cursor at its start, so a primary selection
        // anywhere else is one the app has let go of.
        bool primaryAtCursor(const Surrounding& s, const std::string& primary) {
            if (!s.isValid() || !utf8::validate(s.text()) || s.cursor() > utf8::length(s.text()))
                return true;
            const std::string after = s.text().substr(byteOffset(s.text(), s.cursor()));
            // A long selection can run past the part of the text the app sends.
            return !after.empty() && (isStartsWith(after, primary) || isStartsWith(primary, after));
        }

    } // namespace

    ConvertResult TypingState::convertSelection() {
        if (host_->field().hidesText())
            return ConvertResult::Refused;
        if (!emojiBuffer_.empty())
            return ConvertResult::Busy;
        if (mode() == Mode::Preedit && bambooEngine_) {
            const UniqueCPtr<char> word(EnginePullPreedit(bambooEngine_.handle()));
            if (word && *word.get() != '\0')
                return ConvertResult::Busy;
        }

        const Surrounding s        = host_->surrounding();
        std::string       selected = reportedSelection(s);
        const bool        reported = !selected.empty();
        // Firefox and LibreOffice report no selection, but put it in the primary selection.
        if (!reported) {
            selected = host_->primarySelection();
            if (!selected.empty() && !primaryAtCursor(s, selected)) {
                NGOSEN_INFO("Convert: the primary selection is not at the cursor");
                return ConvertResult::NoSelection;
            }
        }
        if (selected.empty())
            return ConvertResult::NoSelection;

        const Options&   o = engine_->options();
        UniqueCPtr<char> converted(ConvertText(selected.c_str(), o.convertFrom.c_str(), o.convertTo.c_str(), o.convertCase, o.convertStripDiacritics));
        if (!converted) {
            NGOSEN_WARN("Cannot convert from " + o.convertFrom + " to " + o.convertTo);
            return ConvertResult::Refused;
        }
        if (selected == converted.get())
            return ConvertResult::Unchanged;

        reset();
        // GTK 4 on Wayland types a commit next to the selection rather than over it.
        if (reported) {
            const unsigned int from = std::min(s.cursor(), s.anchor());
            host_->deleteSurrounding(static_cast<int>(from) - static_cast<int>(s.cursor()), std::max(s.cursor(), s.anchor()) - from);
        }
        host_->commitText(converted.get());
        noteCommit(converted.get());
        return ConvertResult::Converted;
    }

} // namespace ngosen
