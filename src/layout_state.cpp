#include "layout_state.h"

#include "input.h"
#include "render.h"
#include "ttx/layout_json.h"
#include "ttx/pane.h"
#include "workspace.h"

namespace ttx {
LayoutState::LayoutState(Size const& size, Config config) : m_size(size), m_config(di::move(config)) {}

void LayoutState::set_config(Config config) {
    if (m_config == config) {
        return;
    }
    m_config = di::move(config);
    layout({});
}

void LayoutState::layout(di::Optional<Size> size) {
    if (!size) {
        size = m_size;
    } else {
        m_size = size.value();
    }

    if (!m_active_workspace) {
        return;
    }
    if (m_popup) {
        m_popup_layout = m_popup.value().layout(available_size());
    }
    m_active_workspace->layout(available_size());
}

auto LayoutState::set_active_workspace(Workspace* workspace, bool focus) -> bool {
    if (m_active_workspace == workspace) {
        return false;
    }

    if (m_active_workspace) {
        m_active_workspace->set_is_active(false);
    }
    m_active_workspace = workspace;
    if (focus && m_active_workspace) {
        m_active_workspace->set_is_active(true);
    }
    layout();
    return true;
}

auto LayoutState::set_active_tab(Workspace& workspace, Tab* tab, bool focus) -> bool {
    if (focus) {
        set_active_workspace(&workspace);
    }
    return workspace.set_active_tab(tab);
}

void LayoutState::remove_tab(Workspace& workspace, Tab& tab) {
    workspace.remove_tab(tab);
    if (workspace.empty()) {
        remove_workspace(workspace);
    }
}

void LayoutState::remove_pane(Workspace& workspace, Tab& tab, PaneId pane) {
    workspace.remove_pane(tab, pane);
    if (workspace.empty()) {
        remove_workspace(workspace);
    }
}

void LayoutState::remove_popup() {
    if (!m_popup) {
        return;
    }

    m_popup = {};
    m_popup_layout = {};
    for (auto& workspace : active_workspace()) {
        workspace.set_is_active(true);
    }
    for (auto& tab : active_tab()) {
        tab.invalidate_all();
    }
}

void LayoutState::remove_workspace(Workspace& workspace) {
    // For now, ASSERT() there are no panes in the workspace. If there were, we'd
    // need to make sure not to destroy the panes while we hold the lock.
    ASSERT(workspace.empty());

    // Clear active workspace.
    if (m_active_workspace == &workspace) {
        auto* it = di::find(m_workspaces, &workspace, [](di::Box<Workspace> const& workspace) {
            return workspace.get();
        });
        if (it == m_workspaces.end()) {
            set_active_workspace(m_workspaces.at(0).transform(&di::Box<Workspace>::get).value_or(nullptr));
        } else if (m_workspaces.size() == 1) {
            set_active_workspace(nullptr);
        } else {
            auto index = usize(it - m_workspaces.begin());
            if (index == m_workspaces.size() - 1) {
                set_active_workspace(m_workspaces[index - 1].get());
            } else {
                set_active_workspace(m_workspaces[index + 1].get());
            }
        }
    }

    // Delete workspace.
    di::erase_if(m_workspaces, [&](di::Box<Workspace> const& item) {
        return item.get() == &workspace;
    });
}

void LayoutState::add_pane(Workspace& workspace, Tab& tab, PaneId pane, Direction direction) {
    set_active_workspace(&workspace);
    workspace.add_pane(tab, pane, direction);
}

void LayoutState::add_tab(Workspace& workspace, PaneId initial_pane) {
    set_active_workspace(&workspace);
    workspace.add_tab(m_next_tab_id++, initial_pane);
}

void LayoutState::add_workspace(PaneId initial_pane) {
    auto id = m_next_workspace_id++;
    auto& workspace = m_workspaces.push_back(di::make_box<Workspace>(this, id));
    add_tab(*workspace, initial_pane);
}

void LayoutState::popup_pane(PopupLayout const& popup_layout, PaneId pane) {
    // Prevent creating more than 1 popup.
    if (m_popup) {
        return;
    }
    m_popup = Popup {
        .pane_id = pane,
        .layout_config = popup_layout,
    };
    m_popup_layout = m_popup.value().layout(available_size());

    for (auto& workspace : active_workspace()) {
        workspace.set_is_active(false);
    }
    for (auto& tab : active_tab()) {
        tab.invalidate_all();
    }
}

auto LayoutState::active_workspace() const -> di::Optional<Workspace&> {
    if (!m_active_workspace) {
        return {};
    }
    return *m_active_workspace;
}

auto LayoutState::active_tab() const -> di::Optional<Tab&> {
    return active_workspace().and_then(&Workspace::active_tab);
}

auto LayoutState::active_pane() const -> di::Optional<PaneId> {
    if (auto popup = active_popup()) {
        return popup.value();
    }
    if (!active_tab()) {
        return {};
    }
    return active_tab()->active();
}

auto LayoutState::full_screen_pane() const -> di::Optional<PaneId> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->full_screen_pane();
}

auto LayoutState::active_popup() const -> di::Optional<PaneId> {
    if (!active_tab()) {
        return {};
    }
    return popup_layout().transform([&](LayoutEntry const& entry) {
        return entry.pane_id;
    });
}

auto LayoutState::as_json_v1() const -> json::v1::LayoutState {
    auto json = json::v1::LayoutState {};
    if (m_active_workspace) {
        json.active_workspace_id = m_active_workspace->id();
    }
    for (auto const& workspace : m_workspaces) {
        json.workspaces.push_back(workspace->as_json_v1());
    }
    return json;
}

auto LayoutState::as_json() const -> json::Layout {
    return as_json_v1();
}

auto LayoutState::restore_json_v1(json::v1::LayoutState const& json) -> di::Result<> {
    auto size = hide_status_bar() ? m_size : m_size.rows_shrinked(1);
    for (auto const& workspace_json : json.workspaces) {
        m_workspaces.push_back(TRY(Workspace::from_json_v1(workspace_json, this, size)));
    }

    // Find the active workspace by id
    for (auto id : json.active_workspace_id) {
        auto* it = di::find(m_workspaces, id, &Workspace::id);
        if (it != m_workspaces.end()) {
            set_active_workspace(it->get());
        }
    }

    if (m_workspaces.empty()) {
        return {};
    }

    // Fallback case: set the first workspace as active
    if (!m_active_workspace) {
        set_active_workspace(m_workspaces[0].get());
    }

    // Update next ids
    m_next_workspace_id = di::max(m_workspaces | di::transform(&Workspace::id));
    m_next_workspace_id++;
    m_next_tab_id = di::max(m_workspaces | di::transform(&Workspace::max_tab_id));
    m_next_tab_id++;

    // TODO: validate all ids are unique

    return {};
}

auto LayoutState::restore_json(json::Layout const& json) -> di::Result<> {
    return di::visit(di::overload([&](json::v1::LayoutState const& state) {
                         return restore_json_v1(state);
                     }),
                     json);
}
}
