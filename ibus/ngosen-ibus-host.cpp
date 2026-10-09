/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-ibus-host.h"

#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-shell-keys.h"
#include "ngosen-xtest.h"

#include <libintl.h>

namespace ngosen {

    static_assert(key::BackSpace == IBUS_KEY_BackSpace);
    static_assert(key::Right == IBUS_KEY_Right);
    static_assert(key::space == IBUS_KEY_space);
    static_assert(key::Shift_L == IBUS_KEY_Shift_L);
    static_assert(modifier::Ctrl == IBUS_CONTROL_MASK);

    namespace {
        // evdev KEY_BACKSPACE, KEY_RIGHT and KEY_LEFTSHIFT; IBus keycodes are evdev codes.
        constexpr guint BackSpaceKeycode = 14;
        constexpr guint RightKeycode     = 106;
        constexpr guint ShiftKeycode     = 42;

        constexpr guint SimpleModifiers = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_MOD4_MASK | IBUS_SUPER_MASK | IBUS_HYPER_MASK | IBUS_META_MASK;

        class GlibTimer final : public Timer {
          public:
            GlibTimer(uint64_t deadlineUs, std::function<bool(Timer&)> onTime) : onTime_(std::move(onTime)) {
                source_       = g_source_new(&funcs_, sizeof(Source));
                auto* source  = reinterpret_cast<Source*>(source_);
                source->timer = this;
                g_source_set_ready_time(source_, static_cast<gint64>(deadlineUs));
                g_source_attach(source_, nullptr);
            }
            ~GlibTimer() override {
                g_source_destroy(source_);
                g_source_unref(source_);
            }

            void rearm(uint64_t deadlineUs) override {
                g_source_set_ready_time(source_, static_cast<gint64>(deadlineUs));
            }

          private:
            struct Source {
                GSource    base;
                GlibTimer* timer;
            };

            static gboolean dispatch(GSource* base, GSourceFunc /*callback*/, gpointer /*data*/) {
                auto* timer = reinterpret_cast<Source*>(base)->timer;
                // Fires once unless the callback rearms it and returns true.
                g_source_set_ready_time(base, -1);
                if (!timer->onTime_(*timer))
                    g_source_set_ready_time(base, -1);
                return G_SOURCE_CONTINUE;
            }

            static GSourceFuncs         funcs_;
            GSource*                    source_ = nullptr;
            std::function<bool(Timer&)> onTime_;
        };

        GSourceFuncs GlibTimer::funcs_ = {nullptr, nullptr, &GlibTimer::dispatch, nullptr, nullptr, nullptr};
    } // namespace

    std::unique_ptr<Timer> startGlibTimer(uint64_t deadlineUs, std::function<bool(Timer&)> onTime) {
        return std::make_unique<GlibTimer>(deadlineUs, std::move(onTime));
    }

    void IBusHost::commitText(const std::string& text) {
        ibus_engine_commit_text(engine_, ibus_text_new_from_string(text.c_str()));
        if (onCommit_)
            onCommit_(text);
    }

    void IBusHost::forwardKey(EditKey key, bool release) {
        const guint state = release ? IBUS_RELEASE_MASK : 0;
        switch (key) {
            case EditKey::BackSpace: ibus_engine_forward_key_event(engine_, IBUS_KEY_BackSpace, BackSpaceKeycode, state); break;
            case EditKey::Right: ibus_engine_forward_key_event(engine_, IBUS_KEY_Right, RightKeycode, state); break;
            case EditKey::Shift: ibus_engine_forward_key_event(engine_, IBUS_KEY_Shift_L, ShiftKeycode, state); break;
        }
    }

    void IBusHost::deleteSurrounding(int offset, unsigned int size) {
        ibus_engine_delete_surrounding_text(engine_, offset, size);
    }

    bool IBusHost::pressSystemKeys(int count) {
        if (xtestAvailable())
            return xtestSendKeys(count);
        return count > 0 && !x11Class_.empty() && shell::pressBackSpace(count);
    }

    bool IBusHost::canPressSystemKeys() const {
        return xtestAvailable() || !x11Class_.empty();
    }

    void IBusHost::refreshWindow() {
        if (x11ClassKnown_)
            return;
        // GNOME Shell's own context serves Wayland windows only.
        x11Class_      = client_ == "gnome-shell" ? std::string() : shell::focusedX11Class();
        x11ClassKnown_ = true;
    }

    Surrounding IBusHost::surrounding() const {
        if ((engine_->client_capabilities & IBUS_CAP_SURROUNDING_TEXT) == 0)
            return {};
        IBusText* text   = nullptr;
        guint     cursor = 0;
        guint     anchor = 0;
        ibus_engine_get_surrounding_text(engine_, &text, &cursor, &anchor);
        if (text == nullptr)
            return {};
        return {ibus_text_get_text(text), cursor, anchor};
    }

    Field IBusHost::field() const {
        Field f;
        // Same as an IBus client of fcitx5, so the app checks in ngosen-app-quirks.cpp apply as they are.
        f.frontend        = "ibus";
        f.program         = client_;
        f.surroundingText = (engine_->client_capabilities & IBUS_CAP_SURROUNDING_TEXT) != 0;
        f.preedit         = (engine_->client_capabilities & IBUS_CAP_PREEDIT_TEXT) != 0;
        f.url             = purpose_ == IBUS_INPUT_PURPOSE_URL;
        f.x11Class        = x11Class_;
        return f;
    }

    bool IBusHost::hasFocus() const {
        return focus_;
    }

    void IBusHost::showPreedit(const std::string& text, bool underline) {
        preedit_   = text;
        underline_ = underline;
    }

    void IBusHost::clearPreedit() {
        preedit_.clear();
    }

    void IBusHost::resetPanel() {
        preedit_.clear();
        status_.clear();
    }

    void IBusHost::refreshPreedit() {
        IBusText*   text   = ibus_text_new_from_string(preedit_.c_str());
        const guint length = static_cast<guint>(g_utf8_strlen(preedit_.c_str(), -1));
        if (underline_ && length > 0)
            ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, length);
        ibus_engine_update_preedit_text(engine_, text, length, length > 0);
    }

    void IBusHost::refreshPanel() {
        ibus_engine_update_auxiliary_text(engine_, ibus_text_new_from_string(status_.c_str()), !status_.empty());
    }

    void IBusHost::showCandidates(const std::vector<std::string>& /*labels*/, int /*pageSize*/, std::function<void(size_t)> /*onPick*/) {
        NGOSEN_WARN("The IBus version has no candidate list yet");
    }

    void                         IBusHost::hideCandidates() {}

    std::optional<CandidatePage> IBusHost::candidates() const {
        return std::nullopt;
    }

    void IBusHost::highlightCandidate(int /*index*/) {}
    void IBusHost::nextCandidatePage() {}
    void IBusHost::prevCandidatePage() {}
    void IBusHost::pickCandidate(int /*index*/) {}

    void IBusHost::setStatus(const std::string& text) {
        status_ = text;
    }

    std::string IBusHost::translate(const char* text) const {
        return dgettext("fcitx5-lotus", text);
    }

    std::unique_ptr<Timer> IBusHost::startTimer(uint64_t deadlineUs, uint64_t /*accuracyUs*/, std::function<bool(Timer&)> onTime) {
        return startGlibTimer(deadlineUs, std::move(onTime));
    }

    std::string IBusHost::keyText(uint32_t sym) const {
        const gunichar c = ibus_keyval_to_unicode(sym);
        if (c == 0)
            return {};
        char      buf[6];
        const int n = g_unichar_to_utf8(c, buf);
        return std::string(buf, static_cast<size_t>(n));
    }

    uint32_t IBusKeyPress::sym() const {
        return sym_;
    }

    uint32_t IBusKeyPress::states() const {
        return state_ & IBUS_MODIFIER_MASK & ~IBUS_RELEASE_MASK;
    }

    bool IBusKeyPress::isRelease() const {
        return (state_ & IBUS_RELEASE_MASK) != 0;
    }

    bool IBusKeyPress::isModifier() const {
        return (sym_ >= IBUS_KEY_Shift_L && sym_ <= IBUS_KEY_Hyper_R) || sym_ == IBUS_KEY_ISO_Level3_Shift || sym_ == IBUS_KEY_ISO_Level5_Shift;
    }

    bool IBusKeyPress::isBareShift() const {
        return (sym_ == IBUS_KEY_Shift_L || sym_ == IBUS_KEY_Shift_R) && (state_ & SimpleModifiers & ~IBUS_SHIFT_MASK) == 0;
    }

    // fcitx5 reports X11 keycodes, which are evdev codes plus 8.
    uint32_t IBusKeyPress::code() const {
        return code_ + 8;
    }

    // IBus passes no timestamp to the engine.
    uint32_t IBusKeyPress::time() const {
        return 0;
    }

    bool IBusKeyPress::hasModifier() const {
        return (state_ & SimpleModifiers) != 0;
    }

    bool IBusKeyPress::isCursorMove() const {
        switch (appSym_) {
            case IBUS_KEY_Left:
            case IBUS_KEY_Right:
            case IBUS_KEY_Up:
            case IBUS_KEY_Down:
            case IBUS_KEY_Page_Up:
            case IBUS_KEY_Page_Down:
            case IBUS_KEY_Home:
            case IBUS_KEY_End: return (state_ & SimpleModifiers & ~(IBUS_CONTROL_MASK | IBUS_SHIFT_MASK)) == 0;
            default: return false;
        }
    }

    std::string IBusKeyPress::name() const {
        std::string out;
        if ((state_ & IBUS_CONTROL_MASK) != 0)
            out += "Control+";
        if ((state_ & IBUS_MOD1_MASK) != 0)
            out += "Alt+";
        if ((state_ & IBUS_SHIFT_MASK) != 0)
            out += "Shift+";
        if ((state_ & (IBUS_MOD4_MASK | IBUS_SUPER_MASK)) != 0)
            out += "Super+";
        const gchar* keyName = ibus_keyval_name(appSym_);
        return keyName != nullptr ? out + keyName : std::string();
    }

    void IBusKeyPress::replaceSym(uint32_t sym) {
        appSym_ = sym;
    }

    void IBusKeyPress::accept() {
        accepted_ = true;
    }

} // namespace ngosen
