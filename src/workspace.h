#pragma once

#include "di/container/vector/vector.h"
#include "tab.h"
#include "ttx/direction.h"
#include "ttx/ids.h"
#include "ttx/ipc/pane_id.h"
#include "ttx/layout_json.h"

namespace ttx {
class LayoutState;

/// @brief Represents a workspace (like a "session" in tmux)
class Workspace {
public:
    explicit Workspace(LayoutState* layout_state, WorkspaceId id, di::Optional<di::String> name = {})
        : m_layout_state(layout_state), m_name(di::move(name)), m_id(id) {}

    static auto from_json_v1(json::v1::Workspace const& json, LayoutState* layout_state, Size size)
        -> di::Result<di::Box<Workspace>>;

    void layout(di::Optional<Size> size = {});
    auto set_active_tab(Tab* tab) -> bool;
    void remove_tab(Tab& tab);
    void remove_pane(Tab& tab, PaneId pane_id);

    auto max_tab_id() const -> TabId;

    auto id() const { return m_id; }
    void set_name(di::Optional<di::String> name) { m_name = di::move(name); }
    auto name() const -> di::Optional<di::StringView> { return m_name.transform(&di::String::view); }

    void add_pane(Tab& tab, PaneId pane, Direction direction);
    void add_tab(TabId tab_id, PaneId initial_pane);

    auto empty() const -> bool { return m_tabs.empty(); }
    auto tabs() -> di::Vector<di::Box<Tab>>& { return m_tabs; }
    auto tabs() const -> di::Vector<di::Box<Tab>> const& { return m_tabs; }
    auto active_tab() const -> di::Optional<Tab&>;

    auto active_pane() const -> di::Optional<PaneId>;
    auto full_screen_pane() const -> di::Optional<PaneId>;
    auto size() const -> Size { return m_size; }

    auto is_active() const -> bool { return m_is_active; }
    auto set_is_active(bool b) -> bool;

    auto as_json_v1() const -> json::v1::Workspace;

    auto layout_state() const -> LayoutState& { return *m_layout_state; }

private:
    LayoutState* m_layout_state { nullptr };
    di::Optional<di::String> m_name;
    Size m_size;
    WorkspaceId m_id { 0 };
    di::Vector<di::Box<Tab>> m_tabs {};
    Tab* m_active_tab { nullptr };
    bool m_is_active { false };
};
}
