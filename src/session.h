#pragma once

#include "di/container/vector/vector.h"
#include "tab.h"
#include "ttx/layout_json.h"
#include "ttx/popup.h"

namespace ttx {
class LayoutState;

/// @brief Represents a "session" (like in tmux)
class Session {
public:
    explicit Session(LayoutState* layout_state, di::String name, u64 id)
        : m_layout_state(layout_state), m_name(di::move(name)), m_id(id) {}

    void layout(di::Optional<Size> size = {});
    auto set_active_tab(Tab* tab) -> bool;
    void remove_tab(Tab& tab);
    auto remove_pane(Tab& tab, Pane* pane) -> di::Box<Pane>;

    auto id() const { return m_id; }
    void set_name(di::String name) { m_name = di::move(name); }
    auto name() const -> di::StringView { return m_name; }

    auto add_pane(Tab& tab, u64 pane_id, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
        -> di::Result<>;
    auto popup_pane(Tab& tab, u64 pane_id, PopupLayout const& popup_layout, CreatePaneArgs args,
                    RenderThread& render_thread) -> di::Result<>;
    auto add_tab(CreatePaneArgs args, u64 tab_id, u64 pane_id, RenderThread& render_thread) -> di::Result<>;

    auto empty() const -> bool { return m_tabs.empty(); }
    auto tabs() -> di::Vector<di::Box<Tab>>& { return m_tabs; }
    auto tabs() const -> di::Vector<di::Box<Tab>> const& { return m_tabs; }
    auto active_tab() const -> di::Optional<Tab&>;

    auto active_pane() const -> di::Optional<Pane&>;
    auto full_screen_pane() const -> di::Optional<Pane&>;
    auto size() const -> Size { return m_size; }

    auto is_active() const -> bool { return m_is_active; }
    auto set_is_active(bool b) -> bool;

    void layout_did_update();
    auto as_json_v1() const -> json::v1::Session;

private:
    LayoutState* m_layout_state { nullptr };
    di::String m_name;
    Size m_size;
    u64 m_id { 0 };
    di::Vector<di::Box<Tab>> m_tabs {};
    Tab* m_active_tab { nullptr };
    bool m_is_active { false };
};
}
