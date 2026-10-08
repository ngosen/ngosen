/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// TypingState: the emoji picker mode.
#include "ngosen-state.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"

#include <cstddef>
#include <algorithm>
#include <string>

namespace ngosen {

    void TypingState::updateEmojiPageStatus() {
        const auto list = host_->candidates();
        if (!list || list->total == 0) {
            return;
        }

        int pageSize = list->pageSize;
        if (pageSize <= 0) {
            pageSize = 9;
        }

        int         totalItems  = list->total;
        int         currentPage = list->page + 1;
        int         totalPages  = (totalItems + pageSize - 1) / pageSize;

        std::string status = host_->translate("Page ") + std::to_string(currentPage) + "/" + std::to_string(totalPages);
        host_->setStatus(status);
    }

    void TypingState::pickEmoji(const EmojiEntry& entry) {
        host_->commitText(entry.output);
        NGOSEN_INFO("Emoji committed: " + entry.output);

        engine_->recordEmoji(entry);

        emojiBuffer_.clear();
        emojiCandidates_.clear();

        host_->resetPanel();
        host_->refreshPanel();
        updateEmojiPreedit();
    }

    void TypingState::handleEmojiMode(ngosen::KeyPress& keyEvent) {
        const uint32_t currentSym      = keyEvent.sym();
        bool           isCtrlBackspace = ngosen::key::isBackspace(currentSym) && ((keyEvent.states() & ngosen::modifier::Ctrl) != 0U);

        if (keyEvent.hasModifier() && !isCtrlBackspace) {
            keyEvent.passToApp();
            return;
        }

        const auto list = host_->candidates();
        if (list && currentSym >= ngosen::key::Digit1 && currentSym <= ngosen::key::Digit9) {
            int offset      = currentSym - ngosen::key::Digit1;
            int globalIndex = (list->page * list->pageSize) + offset;

            if (globalIndex < list->total) {
                host_->pickCandidate(globalIndex);
                keyEvent.accept();
                return;
            }
        }

        if (list && list->total > 0) {
            int  globalCursorIndex = list->cursor;
            int  totalSize         = list->total;
            int  currentPage       = list->page;
            int  pageSize          = list->pageSize;
            int  localCursorIndex  = globalCursorIndex - (currentPage * pageSize);

            bool handled = false;

            switch (currentSym) {
                case ngosen::key::Tab:
                case ngosen::key::Down: {
                    if (localCursorIndex < pageSize - 1 && globalCursorIndex < totalSize - 1) {
                        host_->highlightCandidate(globalCursorIndex + 1);
                    } else {
                        host_->highlightCandidate(currentPage * pageSize);
                    }
                    handled = true;
                    break;
                }

                case ngosen::key::ISO_Left_Tab:
                case ngosen::key::Up: {
                    if (localCursorIndex > 0) {
                        host_->highlightCandidate(globalCursorIndex - 1);
                    } else {
                        int lastIndex = std::min((currentPage * pageSize) + pageSize - 1, totalSize - 1);
                        host_->highlightCandidate(lastIndex);
                    }
                    handled = true;
                    break;
                }
                case ngosen::key::Page_Down:
                case ngosen::key::Right: {
                    if (list->hasNext) {
                        host_->nextCandidatePage();
                        int newPage = host_->candidates()->page;
                        host_->highlightCandidate(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                case ngosen::key::Page_Up:
                case ngosen::key::Left: {
                    if (list->hasPrev) {
                        host_->prevCandidatePage();
                        int newPage = host_->candidates()->page;
                        host_->highlightCandidate(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                default: break;
            }

            if (handled) {
                updateEmojiPageStatus();
                host_->refreshPanel();
                keyEvent.accept();
                return;
            }
        }

        if (ngosen::key::isBackspace(currentSym)) {
            if (!emojiBuffer_.empty()) {
                if (isCtrlBackspace) {
                    emojiBuffer_.clear();
                } else {
                    ngosen::utf8::eraseLastCodepoint(emojiBuffer_);
                }
                keyEvent.accept();
            } else {
                keyEvent.passToApp();
            }
            updateEmojiPreedit();
            return;
        }

        switch (currentSym) {
            case ngosen::key::space:
            case ngosen::key::Return: {
                if (list && list->total > 0) {
                    host_->pickCandidate(list->cursor);
                    keyEvent.accept();
                } else if (currentSym == ngosen::key::Return && !emojiBuffer_.empty()) {
                    host_->commitText(emojiBuffer_);
                    emojiBuffer_.clear();
                    updateEmojiPreedit();
                    keyEvent.accept();
                } else {
                    keyEvent.passToApp();
                }
                return;
            }

            case ngosen::key::Escape: {
                emojiBuffer_.clear();
                emojiCandidates_.clear();
                host_->resetPanel();
                host_->refreshPanel();
                keyEvent.accept();
                return;
            }

            default: break;
        }

        {
            std::string utf8Char = host_->keyText(currentSym);
            if (!utf8Char.empty()) {
                emojiBuffer_.append(utf8Char);
                keyEvent.accept();
                updateEmojiPreedit();
            } else {
                keyEvent.passToApp();
            }
        }
    }
    void TypingState::updateEmojiPreedit() {
        if (emojiBuffer_.empty()) {
            emojiCandidates_ = engine_->emojiHistory();
            if (emojiCandidates_.empty()) {
                host_->resetPanel();
                host_->refreshPreedit();
                host_->refreshPanel();
                return;
            }
        } else {
            emojiCandidates_ = engine_->searchEmoji(emojiBuffer_);
        }

        if (!emojiBuffer_.empty()) {
            host_->showPreedit(emojiBuffer_, true);
        } else {
            host_->clearPreedit();
        }

        if (!emojiCandidates_.empty()) {
            std::vector<std::string> labels;
            labels.reserve(emojiCandidates_.size());
            for (size_t i = 0; i < emojiCandidates_.size(); ++i) {
                size_t localIndex = (i % 9) + 1;
                if (emojiBuffer_.empty()) {
                    labels.push_back(std::to_string(localIndex) + ": " + emojiCandidates_[i].output);
                } else {
                    labels.push_back(std::to_string(localIndex) + ": " + emojiCandidates_[i].trigger + " " + emojiCandidates_[i].output);
                }
            }
            // The list keeps its own copy: emojiCandidates_ may change while it is still shown.
            host_->showCandidates(labels, 9, [this, entries = emojiCandidates_](size_t index) { pickEmoji(entries[index]); });
            updateEmojiPageStatus();
        } else {
            host_->hideCandidates();
        }

        host_->refreshPreedit();
        host_->refreshPanel();
    }

} // namespace ngosen
