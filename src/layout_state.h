#pragma once

#include "di/container/vector/vector.h"
#include "session.h"
#include "tab.h"
#include "ttx/layout.h"
#include "ttx/popup.h"

namespace ttx {
class LayoutState {
public:
    explicit LayoutState(Size const& size, bool hide_status_bar);

    void layout(di::Optional<Size> size = {});
    auto set_active_tab(Session& session, Tab* tab) -> bool;
    void remove_tab(Session& session, Tab& tab);
    auto remove_pane(Session& session, Tab& tab, Pane* pane) -> di::Box<Pane>;

    auto add_pane(Session& session, Tab& tab, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
        -> di::Result<>;
    auto popup_pane(Session& session, Tab& tab, PopupLayout const& popup_layout, CreatePaneArgs args,
                    RenderThread& render_thread) -> di::Result<>;
    auto add_tab(Session& session, CreatePaneArgs args, RenderThread& render_thread) -> di::Result<>;

    auto sessions() -> di::Vector<Session>& { return m_sessions; }
    auto add_session(CreatePaneArgs args, RenderThread& render_thread) -> di::Result<>;
    void remove_session(Session& session);
    auto set_active_session(Session* session) -> bool;
    auto active_session() const -> di::Optional<Session&>;

    auto empty() const -> bool { return m_sessions.empty(); }
    auto active_tab() const -> di::Optional<Tab&>;

    auto active_pane() const -> di::Optional<Pane&>;
    auto full_screen_pane() const -> di::Optional<Pane&>;
    auto size() const -> Size { return m_size; }
    auto hide_status_bar() const -> bool { return m_hide_status_bar; }

    auto active_popup() const -> di::Optional<Popup&>;

private:
    Size m_size;
    di::Vector<Session> m_sessions;
    Session* m_active_session { nullptr };
    u64 m_next_pane_id { 1 };
    u64 m_next_session_id { 1 };
    bool m_hide_status_bar { false };
};
}
