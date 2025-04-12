#pragma once

#include "di/container/vector/vector.h"
#include "tab.h"
#include "ttx/layout.h"

namespace ttx {
class LayoutState {
public:
    explicit LayoutState(Size const& size, bool hide_status_bar);

    void layout(di::Optional<Size> size = {});
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
    auto full_screen_pane() const -> di::Optional<Pane&>;
    auto size() const -> Size { return m_size; }
    auto hide_status_bar() const -> bool { return m_hide_status_bar; }

private:
    Size m_size;
    di::Vector<di::Box<Tab>> m_tabs {};
    Tab* m_active_tab { nullptr };
    u64 m_next_pane_id { 1 };
    bool m_hide_status_bar { false };
};
}
