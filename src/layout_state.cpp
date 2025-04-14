#include "layout_state.h"

#include "render.h"
#include "session.h"
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
    set_active_session(&session);
    return session.set_active_tab(tab);
}

void LayoutState::remove_tab(Session& session, Tab& tab) {
    session.remove_tab(tab);
    if (session.empty()) {
        remove_session(session);
    }
}

auto LayoutState::remove_pane(Session& session, Tab& tab, Pane* pane) -> di::Box<Pane> {
    auto result = session.remove_pane(tab, pane);
    if (session.empty()) {
        remove_session(session);
    }
    return result;
}

void LayoutState::remove_session(Session& session) {
    // For now, ASSERT() there are no panes in the session. If there were, we'd
    // need to make sure not to destroy the panes while we hold the lock.
    ASSERT(session.empty());

    // Clear active session.
    if (m_active_session == &session) {
        auto* it = di::find(m_sessions, &session, [](Session const& session) {
            return &session;
        });
        if (it == m_sessions.end()) {
            set_active_session(m_sessions.at(0).data());
        } else if (m_sessions.size() == 1) {
            set_active_session(nullptr);
        } else {
            auto index = usize(it - m_sessions.begin());
            if (index == m_sessions.size() - 1) {
                set_active_session(&m_sessions[index - 1]);
            } else {
                set_active_session(&m_sessions[index + 1]);
            }
        }
    }

    // Delete session.
    di::erase_if(m_sessions, [&](Session const& item) {
        return &item == &session;
    });
}

auto LayoutState::add_pane(Session& session, Tab& tab, CreatePaneArgs args, Direction direction,
                           RenderThread& render_thread) -> di::Result<> {
    set_active_session(&session);
    return session.add_pane(tab, m_next_pane_id++, di::move(args), direction, render_thread);
}

auto LayoutState::popup_pane(Session& session, Tab& tab, PopupLayout const& popup_layout, CreatePaneArgs args,
                             RenderThread& render_thread) -> di::Result<> {
    set_active_session(&session);
    return session.popup_pane(tab, m_next_pane_id++, popup_layout, di::move(args), render_thread);
}

auto LayoutState::add_tab(Session& session, CreatePaneArgs args, RenderThread& render_thread) -> di::Result<> {
    set_active_session(&session);
    return session.add_tab(di::move(args), m_next_pane_id++, render_thread);
}

auto LayoutState::add_session(CreatePaneArgs args, RenderThread& render_thread) -> di::Result<> {
    auto name = di::to_string(m_next_session_id++);
    auto& session = m_sessions.emplace_back(di::move(name));
    return add_tab(session, di::move(args), render_thread);
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
}
