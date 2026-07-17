#pragma once

#include "config.h"
#include "di/container/vector/vector.h"
#include "tab.h"
#include "ttx/ids.h"
#include "ttx/ipc/pane_id.h"
#include "ttx/layout.h"
#include "ttx/layout_json.h"
#include "ttx/popup.h"
#include "workspace.h"

namespace ttx {
class LayoutState {
public:
    explicit LayoutState(Size const& size, Config config);

    void set_config(Config config);
    void layout(di::Optional<Size> size = {});
    auto set_active_tab(Workspace& workspace, Tab* tab, bool focus = true) -> bool;
    void remove_tab(Workspace& workspace, Tab& tab);
    void remove_pane(Workspace& workspace, Tab& tab, PaneId pane);
    void remove_popup();

    void add_pane(Workspace& workspace, Tab& tab, PaneId pane, Direction direction);
    void add_tab(Workspace& workspace, PaneId initial_pane);
    void add_workspace(PaneId initial_pane);
    void popup_pane(PopupLayout const& popup_layout, PaneId pane);

    auto workspaces() -> di::Vector<di::Box<Workspace>>& { return m_workspaces; }
    auto workspaces() const -> di::Vector<di::Box<Workspace>> const& { return m_workspaces; }
    void remove_workspace(Workspace& workspace);
    auto set_active_workspace(Workspace* workspace, bool focus = true) -> bool;
    auto active_workspace() const -> di::Optional<Workspace&>;

    auto empty() const -> bool { return m_workspaces.empty(); }
    auto active_tab() const -> di::Optional<Tab&>;

    auto active_pane() const -> di::Optional<PaneId>;
    auto full_screen_pane() const -> di::Optional<PaneId>;
    auto size() const -> Size { return m_size; }
    auto hide_status_bar() const -> bool { return m_config.status_bar.hide; }

    auto popup_layout() const -> di::Optional<LayoutEntry> { return m_popup_layout; }
    auto active_popup() const -> di::Optional<PaneId>;
    auto status_bar_position() const -> di::Optional<u32> {
        if (hide_status_bar()) {
            return {};
        }
        return m_config.status_bar.position == StatusBarPosition::Top ? 0_u32 : u32(m_size.rows - 1);
    }

    auto as_json_v1() const -> json::v1::LayoutState;
    auto as_json() const -> json::Layout;
    auto restore_json_v1(json::v1::LayoutState const& json) -> di::Result<>;
    auto restore_json(json::Layout const& json) -> di::Result<>;

    auto available_size() const -> Size { return hide_status_bar() ? m_size : m_size.rows_shrinked(1); }

private:
    Size m_size;
    di::Vector<di::Box<Workspace>> m_workspaces;
    Workspace* m_active_workspace { nullptr };
    TabId m_next_tab_id { 1 };
    WorkspaceId m_next_workspace_id { 1 };
    Config m_config;
    di::Optional<Popup> m_popup;
    di::Optional<LayoutEntry> m_popup_layout;
};
}
