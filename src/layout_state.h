#pragma once

#include "config.h"
#include "di/container/vector/vector.h"
#include "di/serialization/json_value.h"
#include "session.h"
#include "tab.h"
#include "ttx/layout.h"
#include "ttx/layout_json.h"
#include "ttx/popup.h"
#include "ttx/terminal/palette.h"

namespace ttx {
class LayoutState {
public:
    explicit LayoutState(Size const& size, Config config);

    void set_config(Config config);
    void layout(di::Optional<Size> size = {});
    auto set_active_tab(Session& session, Tab* tab) -> bool;
    void remove_tab(Session& session, Tab& tab);
    auto remove_pane(Session& session, Tab& tab, Pane* pane) -> di::Box<Pane>;
    auto pane_by_id(u64 session_id, u64 tab_id, u64 pane_id) -> di::Optional<Pane&>;

    auto add_pane(Session& session, Tab& tab, CreatePaneArgs args, Direction direction, RenderThread& render_thread,
                  InputThread& input_thread) -> di::Result<>;
    auto popup_pane(Session& session, Tab& tab, PopupLayout const& popup_layout, CreatePaneArgs args,
                    RenderThread& render_thread, InputThread& input_thread) -> di::Result<>;
    auto add_tab(Session& session, CreatePaneArgs args, RenderThread& render_thread, InputThread& input_thread)
        -> di::Result<>;

    auto sessions() -> di::Vector<di::Box<Session>>& { return m_sessions; }
    auto add_session(CreatePaneArgs args, RenderThread& render_thread, InputThread& input_thread) -> di::Result<>;
    void remove_session(Session& session);
    auto set_active_session(Session* session) -> bool;
    auto active_session() const -> di::Optional<Session&>;

    auto empty() const -> bool { return m_sessions.empty(); }
    auto active_tab() const -> di::Optional<Tab&>;

    auto active_pane() const -> di::Optional<Pane&>;
    auto full_screen_pane() const -> di::Optional<Pane&>;
    auto size() const -> Size { return m_size; }
    auto hide_status_bar() const -> bool { return m_config.status_bar.hide; }

    auto active_popup() const -> di::Optional<Pane&>;
    auto status_bar_position() const -> di::Optional<u32> {
        if (hide_status_bar()) {
            return {};
        }
        return m_config.status_bar.position == StatusBarPosition::Top ? 0_u32 : u32(m_size.rows - 1);
    }

    void set_layout_did_update(di::Function<void()> layout_did_update);
    void layout_did_update();
    auto as_json_v1() const -> json::v1::LayoutState;
    auto as_json() const -> json::Layout;
    auto restore_json_v1(json::v1::LayoutState const& json, CreatePaneArgs args, RenderThread& render_thread,
                         InputThread& input_thread) -> di::Result<>;
    auto restore_json(json::Layout const& json, CreatePaneArgs args, RenderThread& render_thread,
                      InputThread& input_thread) -> di::Result<>;

    void for_each_pane(di::FunctionRef<void(Pane&)>);

private:
    di::Function<void()> m_layout_did_update;
    Size m_size;
    di::Vector<di::Box<Session>> m_sessions;
    Session* m_active_session { nullptr };
    u64 m_next_pane_id { 1 };
    u64 m_next_tab_id { 1 };
    u64 m_next_session_id { 1 };
    Config m_config;
};
}
