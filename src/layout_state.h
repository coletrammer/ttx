#pragma once

#include "di/container/vector/vector.h"
#include "tab.h"
#include "ttx/layout.h"

namespace ttx {
class LayoutState {
public:
    explicit LayoutState(dius::tty::WindowSize const& size, bool show_status_bar);

    void layout(di::Optional<dius::tty::WindowSize> size = {});
    auto set_active_tab(Tab* tab) -> bool;
    void remove_tab(Tab& tab);
    auto remove_pane(Tab& tab, Pane* pane) -> di::Box<Pane>;

    auto add_pane(Tab& tab, CreatePaneArgs args, Direction direction, RenderThread& render_thread) -> di::Result<>;
    auto add_tab(CreatePaneArgs args, RenderThread& render_thread) -> di::Result<>;

    auto empty() const -> bool { return m_tabs.empty(); }
    auto tabs() -> di::Vector<di::Box<Tab>>& { return m_tabs; }
    auto active_tab() const -> di::Optional<Tab&> {
        if (!m_active_tab) {
            return {};
        }
        return *m_active_tab;
    }

    auto active_pane() const -> di::Optional<Pane&>;
    auto size() const -> dius::tty::WindowSize { return m_size; }
    auto show_status_bar() const -> bool { return m_show_status_bar; }

private:
    dius::tty::WindowSize m_size;
    di::Vector<di::Box<Tab>> m_tabs {};
    Tab* m_active_tab { nullptr };
    bool m_show_status_bar { false };
};
}
