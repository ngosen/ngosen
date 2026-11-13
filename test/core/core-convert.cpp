/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// The convert hotkey changes selected text from one charset to another (#655).
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-globals.h"
#include "ngosen-state.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using ngosen::ConvertResult;
using ngosen::Field;
using ngosen::test::FakeHost;
using ngosen::test::FakeKey;
using ngosen::test::FakeLoop;
using ngosen::test::FakeResources;

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    std::string describe(const FakeHost& host) {
        std::string out = "commits:";
        for (const auto& c : host.commits())
            out += " [" + c + "]";
        out += " deletes:";
        for (const auto& [offset, size] : host.deletes())
            out += " (" + std::to_string(offset) + "," + std::to_string(size) + ")";
        return out;
    }

    Field makeField(bool preedit = false) {
        Field field;
        field.frontend        = "wayland";
        field.surroundingText = true;
        field.preedit         = preedit;
        return field;
    }

    class Setup {
      public:
        explicit Setup(Field field = makeField(), ngosen::Mode mode = ngosen::Mode::Sen) {
            realMode   = mode;
            auto owned = std::make_unique<FakeHost>(loop_, std::move(field));
            host_      = owned.get();
            state_     = std::make_unique<ngosen::TypingState>(&resources_, std::move(owned));
        }
        FakeHost& host() {
            return *host_;
        }
        ngosen::Options& options() {
            return resources_.mutableOptions();
        }
        ngosen::TypingState& state() {
            return *state_;
        }
        void type(const std::string& keys) {
            for (const char c : keys) {
                FakeKey down(static_cast<uint32_t>(c));
                state_->keyEvent(down);
                FakeKey up(static_cast<uint32_t>(c), true);
                state_->keyEvent(up);
                loop_.pump(20);
            }
        }

      private:
        FakeLoop                             loop_;
        FakeResources                        resources_;
        FakeHost*                            host_ = nullptr;
        std::unique_ptr<ngosen::TypingState> state_;
    };

    using Deletes = std::vector<std::pair<int, unsigned int>>;
    using Commits = std::vector<std::string>;

    void expect(Setup& s, const std::string& what, ConvertResult want, const Commits& commits, const Deletes& deletes) {
        const ConvertResult got = s.state().convertSelection();
        check(got == want, what + ": result " + std::to_string(static_cast<int>(got)) + ", " + describe(s.host()));
        check(s.host().commits() == commits && s.host().deletes() == deletes, what + ": " + describe(s.host()));
    }

    // The app reports the selection: it is deleted first, since GTK 4 on Wayland commits beside it.
    void reportedSelection() {
        {
            Setup s;
            s.host().setSurrounding("a TiÕng b", 7, 2);
            expect(s, "selected left to right", ConvertResult::Converted, {"Tiếng"}, {{-5, 5}});
        }
        {
            Setup s;
            s.host().setSurrounding("a TiÕng b", 2, 7);
            expect(s, "selected right to left", ConvertResult::Converted, {"Tiếng"}, {{0, 5}});
        }
        {
            Setup s;
            s.host().setSurrounding("a TiÕng b", 7, 2);
            s.host().setPrimary("something older");
            expect(s, "reported selection wins over the primary one", ConvertResult::Converted, {"Tiếng"}, {{-5, 5}});
        }
        {
            Setup s;
            s.options().convertCase            = 1;
            s.options().convertStripDiacritics = true;
            s.host().setSurrounding("Hµ Néi", 6, 0);
            expect(s, "letter case and diacritics", ConvertResult::Converted, {"HA NOI"}, {{-6, 6}});
        }
    }

    // Firefox and LibreOffice report no selection; their primary selection holds it.
    void primarySelection() {
        {
            Setup s;
            s.host().setSurrounding("a TiÕng b", 7);
            s.host().setPrimary("ViÖt");
            expect(s, "primary selection", ConvertResult::Converted, {"Việt"}, {});
        }
        {
            Setup s;
            s.host().forgetSurrounding();
            s.host().setPrimary("ViÖt");
            expect(s, "primary selection without surrounding text", ConvertResult::Converted, {"Việt"}, {});
        }
        {
            Setup s;
            s.host().setSurrounding("abc", 9, 0);
            s.host().setPrimary("ViÖt");
            expect(s, "a selection past the text is not trusted", ConvertResult::Converted, {"Việt"}, {});
        }
    }

    void nothingToDo() {
        {
            Setup s;
            s.host().setSurrounding("a TiÕng b", 7);
            expect(s, "no selection", ConvertResult::NoSelection, {}, {});
        }
        {
            Setup s;
            s.options().convertFrom = "Unicode";
            s.host().setSurrounding("abc", 3, 0);
            expect(s, "same text", ConvertResult::Unchanged, {}, {});
        }
        {
            Setup s;
            s.options().convertTo = "EBCDIC";
            s.host().setSurrounding("a TiÕng b", 7, 2);
            expect(s, "unknown charset", ConvertResult::Refused, {}, {});
        }
        {
            Field field    = makeField();
            field.password = true;
            Setup s(field);
            s.host().setSurrounding("TiÕng", 5, 0);
            s.host().setPrimary("TiÕng");
            expect(s, "password field", ConvertResult::Refused, {}, {});
        }
        {
            Setup s(makeField(true), ngosen::Mode::Preedit);
            s.type("vie");
            s.host().setPrimary("TiÕng");
            const size_t commits = s.host().commits().size();
            check(s.state().convertSelection() == ConvertResult::Busy, "word being typed: " + describe(s.host()));
            check(s.host().commits().size() == commits && s.host().preedit() == "vie", "word being typed is kept: " + describe(s.host()) + " preedit " + s.host().preedit());
        }
    }

} // namespace

int main() {
    reportedSelection();
    primarySelection();
    nothingToDo();
    if (failures != 0) {
        std::cerr << failures << " failed\n";
        return 1;
    }
    std::cout << "core_convert: all passed\n";
    return 0;
}
