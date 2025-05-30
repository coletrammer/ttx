#include "layout_state.h"

#include "render.h"
#include "session.h"
#include "ttx/layout_json.h"
#include "ttx/pane.h"

namespace ttx {
LayoutState::LayoutState(Size const& size, bool hide_status_bar) : m_size(size), m_hide_status_bar(hide_status_bar) {}

void LayoutState::layout(di::Optional<Size> size) {
    if (!size) {
        size = m_size;
    } else {
        m_size = size.value();
    }

    if (!m_active_session) {
        return;
    }
    if (hide_status_bar()) {
        // Now status bar when forcing a single pane.
        m_active_session->layout(m_size);
    } else {
        m_active_session->layout(m_size.rows_shrinked(1));
    }
}

auto LayoutState::set_active_session(Session* session) -> bool {
    if (m_active_session == session) {
        return false;
    }

    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));
    if (m_active_session) {
        m_active_session->set_is_active(false);
    }
    m_active_session = session;
    if (m_active_session) {
        m_active_session->set_is_active(true);
        // Force a layout() when switching which session is rendered,
        // since we resize things only when rendered.
        layout();
    }
    return true;
}

auto LayoutState::set_active_tab(Session& session, Tab* tab) -> bool {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    set_active_session(&session);
    return session.set_active_tab(tab);
}

void LayoutState::remove_tab(Session& session, Tab& tab) {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    session.remove_tab(tab);
    if (session.empty()) {
        remove_session(session);
    }
}

auto LayoutState::remove_pane(Session& session, Tab& tab, Pane* pane) -> di::Box<Pane> {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    auto result = session.remove_pane(tab, pane);
    if (session.empty()) {
        remove_session(session);
    }
    return result;
}

void LayoutState::remove_session(Session& session) {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    // For now, ASSERT() there are no panes in the session. If there were, we'd
    // need to make sure not to destroy the panes while we hold the lock.
    ASSERT(session.empty());

    // Clear active session.
    if (m_active_session == &session) {
        auto* it = di::find(m_sessions, &session, [](di::Box<Session> const& session) {
            return session.get();
        });
        if (it == m_sessions.end()) {
            set_active_session(m_sessions.at(0).transform(&di::Box<Session>::get).value_or(nullptr));
        } else if (m_sessions.size() == 1) {
            set_active_session(nullptr);
        } else {
            auto index = usize(it - m_sessions.begin());
            if (index == m_sessions.size() - 1) {
                set_active_session(m_sessions[index - 1].get());
            } else {
                set_active_session(m_sessions[index + 1].get());
            }
        }
    }

    // Delete session.
    di::erase_if(m_sessions, [&](di::Box<Session> const& item) {
        return item.get() == &session;
    });
}

auto LayoutState::add_pane(Session& session, Tab& tab, CreatePaneArgs args, Direction direction,
                           RenderThread& render_thread) -> di::Result<> {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    set_active_session(&session);
    return session.add_pane(tab, m_next_pane_id++, di::move(args), direction, render_thread);
}

auto LayoutState::popup_pane(Session& session, Tab& tab, PopupLayout const& popup_layout, CreatePaneArgs args,
                             RenderThread& render_thread) -> di::Result<> {
    set_active_session(&session);
    return session.popup_pane(tab, m_next_pane_id++, popup_layout, di::move(args), render_thread);
}

auto LayoutState::add_tab(Session& session, CreatePaneArgs args, RenderThread& render_thread) -> di::Result<> {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    set_active_session(&session);
    return session.add_tab(di::move(args), m_next_tab_id++, m_next_pane_id++, render_thread);
}

auto LayoutState::add_session(CreatePaneArgs args, RenderThread& render_thread) -> di::Result<> {
    auto _ = di::ScopeExit(di::bind_front(&LayoutState::layout_did_update, this));

    auto id = m_next_session_id++;
    auto name = di::to_string(id);
    auto& session = m_sessions.push_back(di::make_box<Session>(this, di::move(name), id));
    return add_tab(*session, di::move(args), render_thread);
}

auto LayoutState::pane_by_id(u64 session_id, u64 tab_id, u64 pane_id) -> di::Optional<Pane&> {
    auto* session = di::find(m_sessions, session_id, &Session::id);
    if (session == m_sessions.end()) {
        return {};
    }
    return (*session)->pane_by_id(tab_id, pane_id);
}

auto LayoutState::active_session() const -> di::Optional<Session&> {
    if (!m_active_session) {
        return {};
    }
    return *m_active_session;
}

auto LayoutState::active_tab() const -> di::Optional<Tab&> {
    return active_session().and_then(&Session::active_tab);
}

auto LayoutState::active_pane() const -> di::Optional<Pane&> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->active();
}

auto LayoutState::full_screen_pane() const -> di::Optional<Pane&> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->full_screen_pane();
}

void LayoutState::set_layout_did_update(di::Function<void()> layout_did_update) {
    m_layout_did_update = di::move(layout_did_update);
}

void LayoutState::layout_did_update() {
    if (m_layout_did_update) {
        m_layout_did_update();
    }
}

auto LayoutState::as_json_v1() const -> json::v1::LayoutState {
    auto json = json::v1::LayoutState {};
    if (m_active_session) {
        json.active_session_id = m_active_session->id();
    }
    for (auto const& session : m_sessions) {
        json.sessions.push_back(session->as_json_v1());
    }
    return json;
}

auto LayoutState::as_json() const -> json::Layout {
    return as_json_v1();
}

auto LayoutState::restore_json_v1(json::v1::LayoutState const& json, CreatePaneArgs args, RenderThread& render_thread)
    -> di::Result<> {
    auto size = m_hide_status_bar ? m_size : m_size.rows_shrinked(1);
    for (auto const& session_json : json.sessions) {
        m_sessions.push_back(TRY(Session::from_json_v1(session_json, this, size, args.clone(), render_thread)));
    }

    // Find the active session by id
    for (auto id : json.active_session_id) {
        auto* it = di::find(m_sessions, id, &Session::id);
        if (it != m_sessions.end()) {
            set_active_session(it->get());
        }
    }

    if (m_sessions.empty()) {
        return {};
    }

    // Fallback case: set the first session as active
    if (!m_active_session) {
        set_active_session(m_sessions[0].get());
    }

    // Update next ids
    m_next_session_id = di::max(m_sessions | di::transform(&Session::id)) + 1;
    m_next_tab_id = di::max(m_sessions | di::transform(&Session::max_tab_id)) + 1;
    m_next_pane_id = di::max(m_sessions | di::transform(&Session::max_pane_id)) + 1;

    // TODO: validate all ids are unique

    return {};
}

auto LayoutState::restore_json(json::Layout const& json, CreatePaneArgs args, RenderThread& render_thread)
    -> di::Result<> {
    return di::visit(di::overload([&](json::v1::LayoutState const& state) {
                         return restore_json_v1(state, di::move(args), render_thread);
                     }),
                     json);
}
}
