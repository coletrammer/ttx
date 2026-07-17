#include "workspace.h"

#include "layout_state.h"
#include "tab.h"
#include "ttx/layout_json.h"

namespace ttx {
void Workspace::layout(di::Optional<Size> size) {
    if (!size) {
        size = m_size;
    } else {
        m_size = size.value();
    }

    if (!m_active_tab) {
        return;
    }
    m_active_tab->layout(m_size);
}

auto Workspace::set_active_tab(Tab* tab) -> bool {
    if (m_active_tab == tab) {
        return false;
    }

    // Update tab with the new active status, only in cases
    // where this workspace is active.
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(false);
    }
    m_active_tab = tab;
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(true);
    }
    layout();
    return true;
}

void Workspace::remove_tab(Tab& tab) {
    // For now, ASSERT() there are no panes in the tab. If there were, we'd
    // need to make sure not to destroy the panes while we hold the lock.
    ASSERT(tab.empty());

    // Clear active tab.
    if (m_active_tab == &tab) {
        auto* it = di::find(m_tabs, &tab, &di::Box<Tab>::get);
        if (it == m_tabs.end()) {
            set_active_tab(m_tabs.at(0).transform(&di::Box<Tab>::get).value_or(nullptr));
        } else if (m_tabs.size() == 1) {
            set_active_tab(nullptr);
        } else {
            auto index = usize(it - m_tabs.begin());
            if (index == m_tabs.size() - 1) {
                set_active_tab(m_tabs[index - 1].get());
            } else {
                set_active_tab(m_tabs[index + 1].get());
            }
        }
    }

    // Delete tab.
    di::erase_if(m_tabs, [&](di::Box<Tab> const& pointer) {
        return pointer.get() == &tab;
    });
}

void Workspace::remove_pane(Tab& tab, PaneId pane_id) {
    tab.remove_pane(pane_id);
    if (tab.empty()) {
        remove_tab(tab);
    } else if (&tab == m_active_tab) {
        layout();
    }
}

void Workspace::add_pane(Tab& tab, PaneId pane, Direction direction) {
    tab.add_pane(pane, m_size, direction);
}

void Workspace::add_tab(TabId tab_id, PaneId initial_pane) {
    auto tab = di::make_box<Tab>(this, tab_id);
    add_pane(*tab, initial_pane, Direction::None);

    set_active_tab(tab.get());
    m_tabs.push_back(di::move(tab));
}

auto Workspace::active_tab() const -> di::Optional<Tab&> {
    if (!m_active_tab) {
        return {};
    }
    return *m_active_tab;
}

auto Workspace::active_pane() const -> di::Optional<PaneId> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->active();
}

auto Workspace::full_screen_pane() const -> di::Optional<PaneId> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->full_screen_pane();
}

auto Workspace::set_is_active(bool b) -> bool {
    if (m_is_active == b) {
        return false;
    }

    // Send focus in/out events appropriately.
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(false);
    }
    m_is_active = b;
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(true);
    }
    return true;
}

auto Workspace::as_json_v1() const -> json::v1::Workspace {
    auto json = json::v1::Workspace {};
    json.name = name().transform(di::to_owned);
    json.id = id();
    for (auto& tab : active_tab()) {
        json.active_tab_id = tab.id();
    }
    for (auto const& tab : m_tabs) {
        json.tabs.push_back(tab->as_json_v1());
    }
    return json;
}

auto Workspace::from_json_v1(json::v1::Workspace const& json, LayoutState* layout_state, Size size)
    -> di::Result<di::Box<Workspace>> {
    // This is needed because the Json parser will accept missing fields for default constructible types.
    if (json.id == WorkspaceId(0)) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto result = di::make_box<Workspace>(layout_state, json.id, json.name.clone());
    result->m_size = size;

    // Restore tabs
    for (auto const& tab_json : json.tabs) {
        result->m_tabs.push_back(TRY(Tab::from_json_v1(tab_json, result.get(), size)));
    }

    // Find the active tab by id
    for (auto id : json.active_tab_id) {
        auto* it = di::find(result->m_tabs, id, &Tab::id);
        if (it != result->m_tabs.end()) {
            result->set_active_tab(it->get());
        }
    }

    if (result->m_tabs.empty()) {
        return result;
    }

    // Fallback case: set the first tab as active
    if (!result->m_active_tab) {
        result->set_active_tab(result->m_tabs[0].get());
    }

    return result;
}

auto Workspace::max_tab_id() const -> TabId {
    if (m_tabs.empty()) {
        return TabId(1);
    }
    return di::max(m_tabs | di::transform(&Tab::id));
}
}
