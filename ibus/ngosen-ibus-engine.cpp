/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-ibus-engine.h"

#include "ngosen-globals.h"
#include "ngosen-ibus-host.h"
#include "ngosen-ibus-resources.h"
#include "ngosen-log.h"
#include "ngosen-state.h"

#include <memory>

namespace {

    struct Session {
        ngosen::IBusHost*                    host = nullptr;
        std::unique_ptr<ngosen::TypingState> state;
    };

    ngosen::IBusResources& resources() {
        static ngosen::IBusResources instance;
        return instance;
    }

    // Applies a changed settings file to the field being typed in.
    void syncSettings(Session& session) {
        realMode = resources().mode();
        if (resources().reload()) {
            realMode = resources().mode();
            session.state->setEngine();
            session.state->reset();
        }
    }

} // namespace

struct NgoSenEngine {
    IBusEngine parent;
    Session*   session;
};

struct NgoSenEngineClass {
    IBusEngineClass parent;
};

G_DEFINE_TYPE(NgoSenEngine, ngosen_engine, IBUS_TYPE_ENGINE)

namespace {

    Session* sessionOf(IBusEngine* engine) {
        return reinterpret_cast<NgoSenEngine*>(engine)->session;
    }

    gboolean processKeyEvent(IBusEngine* engine, guint keyval, guint keycode, guint state) {
        Session* session = sessionOf(engine);
        if (session == nullptr)
            return FALSE;
        session->host->refreshWindow();
        ngosen::IBusKeyPress key(keyval, keycode, state);
        session->state->keyEvent(key);
        if (key.accepted())
            return TRUE;
        // IBus cannot change a key it lets through, so the changed key goes out on its own.
        if (const uint32_t sym = key.replacedSym()) {
            ibus_engine_forward_key_event(engine, sym, keycode, state);
            return TRUE;
        }
        return FALSE;
    }

    void focusIn(IBusEngine* engine) {
        Session* session = sessionOf(engine);
        if (session == nullptr)
            return;
        session->host->setFocus(true);
        syncSettings(*session);
    }

    void focusOut(IBusEngine* engine) {
        Session* session = sessionOf(engine);
        if (session == nullptr)
            return;
        // Pending text belongs to the field being left.
        session->state->flushPendingReplacement();
        session->state->reset(true);
        session->host->setFocus(false);
    }

#if IBUS_CHECK_VERSION(1, 5, 27)
    void focusInId(IBusEngine* engine, const gchar* /*objectPath*/, const gchar* client) {
        if (Session* session = sessionOf(engine))
            session->host->setClient(client != nullptr ? client : "");
        NGOSEN_INFO("Focus in: " << (client != nullptr ? client : ""));
        focusIn(engine);
    }

    void focusOutId(IBusEngine* engine, const gchar* /*objectPath*/) {
        focusOut(engine);
    }
#endif

    void reset(IBusEngine* engine) {
        Session* session = sessionOf(engine);
        if (session != nullptr && session->state->isEmptyHistory())
            session->state->reset();
    }

    void disable(IBusEngine* engine) {
        focusOut(engine);
    }

    void setSurroundingText(IBusEngine* engine, IBusText* text, guint cursor, guint anchor) {
        IBUS_ENGINE_CLASS(ngosen_engine_parent_class)->set_surrounding_text(engine, text, cursor, anchor);
        if (Session* session = sessionOf(engine))
            session->state->surroundingUpdated();
    }

    void setContentType(IBusEngine* engine, guint purpose, guint hints) {
        IBUS_ENGINE_CLASS(ngosen_engine_parent_class)->set_content_type(engine, purpose, hints);
        NGOSEN_DEBUG("Content type: purpose " << purpose << ", hints " << hints);
        if (Session* session = sessionOf(engine))
            session->host->setPurpose(purpose);
    }

    void destroy(IBusObject* object) {
        auto* self = reinterpret_cast<NgoSenEngine*>(object);
        delete self->session;
        self->session = nullptr;
        IBUS_OBJECT_CLASS(ngosen_engine_parent_class)->destroy(object);
    }

#if IBUS_CHECK_VERSION(1, 5, 27)
    // The factory's own engines never get focus_in_id, which is what tells the app apart.
    IBusEngine* createEngine(IBusFactory* factory, const gchar* name, gpointer /*data*/) {
        static guint id     = 0;
        gchar*       path   = g_strdup_printf("/org/freedesktop/IBus/Engine/NgoSen/%u", ++id);
        auto*        engine = IBUS_ENGINE(g_object_new(ngosen_engine_get_type(), "engine-name", name, "object-path", path, "connection",
                                                       ibus_service_get_connection(IBUS_SERVICE(factory)), "has-focus-id", TRUE, nullptr));
        g_free(path);
        return engine;
    }
#endif

} // namespace

static void ngosen_engine_class_init(NgoSenEngineClass* klass) {
    auto* engineClass                 = IBUS_ENGINE_CLASS(klass);
    engineClass->process_key_event    = processKeyEvent;
    engineClass->focus_in             = focusIn;
    engineClass->focus_out            = focusOut;
    engineClass->reset                = reset;
    engineClass->disable              = disable;
    engineClass->set_surrounding_text = setSurroundingText;
    engineClass->set_content_type     = setContentType;
#if IBUS_CHECK_VERSION(1, 5, 27)
    engineClass->focus_in_id  = focusInId;
    engineClass->focus_out_id = focusOutId;
#endif
    IBUS_OBJECT_CLASS(klass)->destroy = destroy;
}

static void ngosen_engine_init(NgoSenEngine* self) {
    auto* engine   = IBUS_ENGINE(self);
    auto  host     = std::make_unique<ngosen::IBusHost>(engine);
    auto* session  = new Session;
    session->host  = host.get();
    realMode       = resources().mode();
    session->state = std::make_unique<ngosen::TypingState>(&resources(), std::move(host));
    session->host->setOnCommit([state = session->state.get()](const std::string& text) { state->noteCommit(text); });
    self->session = session;
}

namespace ngosen {

    void addEngine(IBusFactory* factory) {
        ibus_factory_add_engine(factory, "ngosen", ngosen_engine_get_type());
#if IBUS_CHECK_VERSION(1, 5, 27)
        g_signal_connect(factory, "create-engine", G_CALLBACK(createEngine), nullptr);
#endif
    }

} // namespace ngosen
