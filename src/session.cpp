#include "session.h"

#include "layout_state.h"
#include "tab.h"
#include "ttx/layout_json.h"
#include "ttx/pane.h"

namespace ttx {
void Session::layout(di::Optional<Size> size) {
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

auto Session::set_active_tab(Tab* tab) -> bool {
    if (m_active_tab == tab) {
        return false;
    }

    // Update tab with the new active status, only in cases
    // where this session is active.
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(false);
    }
    m_active_tab = tab;
    if (is_active() && m_active_tab) {
        m_active_tab->set_is_active(true);
        layout();
    }
    return true;
}

void Session::remove_tab(Tab& tab) {
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

auto Session::remove_pane(Tab& tab, Pane* pane) -> di::Box<Pane> {
    auto result = tab.remove_pane(pane);
    if (tab.empty()) {
        remove_tab(tab);
    } else if (result && &tab == m_active_tab) {
        layout();
    }
    return result;
}

auto Session::pane_by_id(u64 tab_id, u64 pane_id) -> di::Optional<Pane&> {
    auto* tab = di::find(m_tabs, tab_id, &Tab::id);
    if (tab == m_tabs.end()) {
        return {};
    }
    return (*tab)->pane_by_id(pane_id);
}

auto Session::add_pane(Tab& tab, u64 pane_id, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
    -> di::Result<> {
    return tab.add_pane(pane_id, m_size, di::move(args), direction, render_thread);
}

auto Session::popup_pane(Tab& tab, u64 pane_id, PopupLayout const& popup_layout, CreatePaneArgs args,
                         RenderThread& render_thread) -> di::Result<> {
    return tab.popup_pane(pane_id, popup_layout, m_size, di::move(args), render_thread);
}

auto Session::add_tab(CreatePaneArgs args, u64 tab_id, u64 pane_id, RenderThread& render_thread) -> di::Result<> {
    auto name = args.replay_path ? "capture"_s
                                 : di::back(di::PathView(args.command[0])).value_or(""_tsv) | di::transform([](char c) {
                                       return c32(c);
                                   }) | di::to<di::String>();
    auto tab = di::make_box<Tab>(this, tab_id, di::move(name));
    TRY(add_pane(*tab, pane_id, di::move(args), Direction::None, render_thread));

    set_active_tab(tab.get());
    m_tabs.push_back(di::move(tab));

    return {};
}

auto Session::active_tab() const -> di::Optional<Tab&> {
    if (!m_active_tab) {
        return {};
    }
    return *m_active_tab;
}

auto Session::active_pane() const -> di::Optional<Pane&> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->active();
}

auto Session::full_screen_pane() const -> di::Optional<Pane&> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->full_screen_pane();
}

auto Session::set_is_active(bool b) -> bool {
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

void Session::layout_did_update() {
    m_layout_state->layout_did_update();
}

auto Session::as_json_v1() const -> json::v1::Session {
    auto json = json::v1::Session {};
    json.name = name().to_owned();
    json.id = id();
    for (auto& tab : active_tab()) {
        json.active_tab_id = tab.id();
    }
    for (auto const& tab : m_tabs) {
        json.tabs.push_back(tab->as_json_v1());
    }
    return json;
}

auto Session::from_json_v1(json::v1::Session const& json, LayoutState* layout_state, Size size, CreatePaneArgs args,
                           RenderThread& render_thread) -> di::Result<di::Box<Session>> {
    // This is needed because the JSOn parser will accept missing fields for default constructible types.
    if (json.id == 0) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto result = di::make_box<Session>(layout_state, json.name.clone(), json.id);
    result->m_size = size;

    // Restore tabs
    for (auto const& tab_json : json.tabs) {
        result->m_tabs.push_back(TRY(Tab::from_json_v1(tab_json, result.get(), size, args.clone(), render_thread)));
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

auto Session::max_tab_id() const -> u64 {
    if (m_tabs.empty()) {
        return 1;
    }
    return di::max(m_tabs | di::transform(&Tab::id));
}

auto Session::max_pane_id() const -> u64 {
    return di::max(m_tabs | di::transform(&Tab::max_pane_id));
}
}
