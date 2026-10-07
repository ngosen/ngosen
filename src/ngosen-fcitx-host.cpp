/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-fcitx-host.h"

#include "ngosen-xtest.h"

#include <fcitx-utils/key.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/instance.h>
#include <fcitx-utils/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>

namespace ngosen {

    namespace {
        // XKB keycode of BackSpace: evdev KEY_BACKSPACE (14) + 8.
        constexpr int BackSpaceKeycode = 22;

        fcitx::Key    toFcitxKey(EditKey key) {
            switch (key) {
                case EditKey::BackSpace: return fcitx::Key(FcitxKey_BackSpace, fcitx::KeyStates(), BackSpaceKeycode);
                case EditKey::Right: return fcitx::Key(FcitxKey_Right);
            }
            return fcitx::Key();
        }

        class FcitxTimer : public Timer {
          public:
            void rearm(uint64_t deadlineUs) override {
                source_->setTime(deadlineUs);
                source_->setOneShot();
            }

            std::unique_ptr<fcitx::EventSourceTime> source_;
        };

        class PickableCandidate : public fcitx::CandidateWord {
          public:
            PickableCandidate(fcitx::Text text, std::function<void(size_t)> onPick, size_t index) :
                fcitx::CandidateWord(std::move(text)), onPick_(std::move(onPick)), index_(index) {}

            void select(fcitx::InputContext* /*inputContext*/) const override {
                onPick_(index_);
            }

          private:
            std::function<void(size_t)> onPick_;
            size_t                      index_;
        };
    } // namespace

    void FcitxHost::commitText(const std::string& text) {
        ic_->commitString(text);
    }

    void FcitxHost::forwardKey(EditKey key, bool release) {
        ic_->forwardKey(toFcitxKey(key), release);
    }

    void FcitxHost::deleteSurrounding(int offset, unsigned int size) {
        ic_->deleteSurroundingText(offset, size);
    }

    bool FcitxHost::pressSystemKeys(int count) {
        return xtestSendKeys(count);
    }

    Surrounding FcitxHost::surrounding() const {
        const auto& s = ic_->surroundingText();
        if (!s.isValid()) {
            return {};
        }
        return {s.text(), s.cursor(), s.anchor()};
    }

    Field FcitxHost::field() const {
        const auto& caps = ic_->capabilityFlags();
        Field       f;
        f.frontend         = ic_->frontend();
        f.program          = ic_->program();
        f.surroundingText  = caps.test(fcitx::CapabilityFlag::SurroundingText);
        f.preedit          = caps.test(fcitx::CapabilityFlag::Preedit);
        f.formattedPreedit = caps.test(fcitx::CapabilityFlag::FormattedPreedit);
        f.url              = caps.test(fcitx::CapabilityFlag::Url);
        f.keyEventOrderFix = caps.test(fcitx::CapabilityFlag::KeyEventOrderFix);
        return f;
    }

    bool FcitxHost::hasFocus() const {
        return ic_->hasFocus();
    }

    void FcitxHost::showPreedit(const std::string& text, bool underline) {
        fcitx::Text preedit;
        if (!text.empty())
            preedit.append(text, underline ? fcitx::TextFormatFlag::Underline : fcitx::TextFormatFlag::NoFlag);
        preedit.setCursor(static_cast<int>(preedit.textLength()));
        if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit))
            ic_->inputPanel().setClientPreedit(preedit);
        else
            ic_->inputPanel().setPreedit(preedit);
    }

    void FcitxHost::clearPreedit() {
        ic_->inputPanel().setClientPreedit(fcitx::Text());
        ic_->inputPanel().setPreedit(fcitx::Text());
    }

    void FcitxHost::resetPanel() {
        ic_->inputPanel().reset();
    }

    void FcitxHost::refreshPreedit() {
        ic_->updatePreedit();
    }

    void FcitxHost::refreshPanel() {
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void FcitxHost::showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) {
        auto list = std::make_unique<fcitx::CommonCandidateList>();
        list->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);
        list->setPageSize(pageSize);
        for (size_t i = 0; i < labels.size(); ++i) {
            fcitx::Text label;
            label.append(labels[i], fcitx::TextFormatFlag::NoFlag);
            list->append(std::make_unique<PickableCandidate>(std::move(label), onPick, i));
        }
        list->setGlobalCursorIndex(0);
        ic_->inputPanel().setCandidateList(std::move(list));
    }

    void FcitxHost::hideCandidates() {
        ic_->inputPanel().setCandidateList(nullptr);
    }

    std::shared_ptr<fcitx::CommonCandidateList> FcitxHost::candidateList() const {
        return std::dynamic_pointer_cast<fcitx::CommonCandidateList>(ic_->inputPanel().candidateList());
    }

    std::optional<CandidatePage> FcitxHost::candidates() const {
        auto list = candidateList();
        if (!list) {
            return std::nullopt;
        }
        CandidatePage page;
        page.total    = list->totalSize();
        page.page     = list->currentPage();
        page.pageSize = list->pageSize();
        page.cursor   = list->globalCursorIndex();
        page.hasNext  = list->hasNext();
        page.hasPrev  = list->hasPrev();
        return page;
    }

    void FcitxHost::highlightCandidate(int index) {
        if (auto list = candidateList())
            list->setGlobalCursorIndex(index);
    }

    void FcitxHost::nextCandidatePage() {
        if (auto list = candidateList())
            list->next();
    }

    void FcitxHost::prevCandidatePage() {
        if (auto list = candidateList())
            list->prev();
    }

    void FcitxHost::pickCandidate(int index) {
        if (auto list = candidateList())
            list->candidateFromAll(index).select(ic_);
    }

    void FcitxHost::setStatus(const std::string& text) {
        ic_->inputPanel().setAuxDown(fcitx::Text(text));
    }

    std::unique_ptr<Timer> FcitxHost::startTimer(uint64_t deadlineUs, uint64_t accuracyUs, std::function<bool(Timer&)> onTime) {
        auto  timer    = std::make_unique<FcitxTimer>();
        auto* self     = timer.get();
        timer->source_ = instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC, deadlineUs, accuracyUs,
                                                             [self, onTime = std::move(onTime)](fcitx::EventSourceTime*, uint64_t) { return onTime(*self); });
        return timer;
    }

    std::string FcitxHost::keyText(uint32_t sym) const {
        return fcitx::Key::keySymToUTF8(static_cast<fcitx::KeySym>(sym));
    }

    uint32_t FcitxKeyPress::sym() const {
        return event_.rawKey().sym();
    }

    uint32_t FcitxKeyPress::states() const {
        return event_.rawKey().states();
    }

    bool FcitxKeyPress::isRelease() const {
        return event_.isRelease();
    }

    bool FcitxKeyPress::isModifier() const {
        return event_.rawKey().isModifier();
    }

    bool FcitxKeyPress::isBareShift() const {
        return event_.rawKey().check(FcitxKey_Shift_L) || event_.rawKey().check(FcitxKey_Shift_R);
    }

    bool FcitxKeyPress::hasModifier() const {
        return event_.key().hasModifier();
    }

    bool FcitxKeyPress::isCursorMove() const {
        return event_.key().isCursorMove();
    }

    std::string FcitxKeyPress::name() const {
        return event_.key().toString();
    }

    void FcitxKeyPress::replaceSym(uint32_t sym) {
        event_.setKey(fcitx::Key(static_cast<fcitx::KeySym>(sym), event_.rawKey().states()));
    }

    void FcitxKeyPress::accept() {
        event_.filterAndAccept();
    }

} // namespace ngosen
