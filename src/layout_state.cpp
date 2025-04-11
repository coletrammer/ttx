#include "layout_state.h"

namespace ttx {
LayoutState::LayoutState(Size const& size, bool hide_status_bar) : m_size(size), m_hide_status_bar(hide_status_bar) {}

void LayoutState::layout(di::Optional<Size> size) {
    if (!size) {
        size = m_size;
    } else {
        m_size = size.value();
    }

    if (!m_active_tab) {
        return;
    }
    if (hide_status_bar()) {
        // Now status bar when forcing a single pane.
        m_active_tab->layout(m_size, 0, 0);
    } else {
        m_active_tab->layout(m_size.rows_shrinked(1), 1, 0);
    }
}

auto LayoutState::set_active_tab(Tab* tab) -> bool {
    if (m_active_tab == tab) {
        return false;
    }

    // Update tab with the new active status, and force layout
    // when switching ot a new tab. This is needed because size
    // changes are only sent to the active tab, so the old layout
    // could be stale.
    if (m_active_tab) {
        m_active_tab->set_is_active(false);
    }
    m_active_tab = tab;
    if (m_active_tab) {
        m_active_tab->set_is_active(true);
        layout();
    }
    return true;
}

void LayoutState::remove_tab(Tab& tab) {
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

auto LayoutState::remove_pane(Tab& tab, Pane* pane) -> di::Box<Pane> {
    auto result = tab.remove_pane(pane);
    if (tab.empty()) {
        remove_tab(tab);
    } else if (result && &tab == m_active_tab) {
        layout();
    }
    return result;
}

auto LayoutState::add_pane(Tab& tab, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
    -> di::Result<> {
    if (hide_status_bar()) {
        return tab.add_pane(m_next_pane_id++, m_size, 0, 0, di::move(args), direction, render_thread);
    }
    return tab.add_pane(m_next_pane_id++, m_size.rows_shrinked(1), 1, 0, di::move(args), direction, render_thread);
}

auto LayoutState::add_tab(CreatePaneArgs args, RenderThread& render_thread) -> di::Result<> {
    auto name = args.replay_path ? "capture"_s
                                 : di::back(di::PathView(args.command[0])).value_or(""_tsv) | di::transform([](char c) {
                                       return c32(c);
                                   }) | di::to<di::String>();
    auto tab = di::make_box<Tab>(di::move(name));
    TRY(add_pane(*tab, di::move(args), Direction::None, render_thread));

    set_active_tab(tab.get());
    m_tabs.push_back(di::move(tab));

    return {};
}

auto LayoutState::active_pane() const -> di::Optional<Pane&> {
    if (!active_tab()) {
        return {};
    }
    return active_tab()->active();
}
}
