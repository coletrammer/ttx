#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/layout.h"
#include "ttx/pane.h"

namespace ttx {
class RenderThread;

enum class NavigateDirection {
    Left,
    Right,
    Up,
    Down,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<NavigateDirection>) {
    using enum NavigateDirection;
    return di::make_enumerators<"NavigateDirection">(di::enumerator<"Left", Left>, di::enumerator<"Right", Right>,
                                                     di::enumerator<"Up", Up>, di::enumerator<"Down", Down>);
}

// Corresponds to tmux window.
struct Tab {
public:
    explicit Tab(di::String name) : m_name(di::move(name)) {}

    void layout(dius::tty::WindowSize const& size, u32 row, u32 col);
    void invalidate_all();

    // Returns the removed pane, if found.
    auto remove_pane(Pane* pane) -> di::Box<Pane>;

    auto add_pane(dius::tty::WindowSize const& size, u32 row, u32 col, CreatePaneArgs args, Direction direction,
                  RenderThread& render_thread) -> di::Result<>;

    void navigate(NavigateDirection direction);

    // Returns true if active pane has changed.
    auto set_active(Pane* pane) -> bool;

    auto name() const -> di::StringView { return m_name; }
    auto empty() const -> bool { return m_layout_root.empty(); }

    auto layout_group() -> LayoutGroup& { return m_layout_root; }
    auto layout_tree() const -> di::Optional<LayoutNode&> {
        if (!m_layout_tree) {
            return {};
        }
        return *m_layout_tree;
    }

    auto active() const -> di::Optional<Pane&> {
        if (!m_active) {
            return {};
        }
        return *m_active;
    }

    auto set_is_active(bool b) -> bool;
    auto is_active() -> bool { return m_is_active; }

private:
    dius::tty::WindowSize m_size;
    di::String m_name;
    LayoutGroup m_layout_root {};
    di::Box<LayoutNode> m_layout_tree {};
    di::Ring<Pane*> m_panes_ordered_by_recency {};
    bool m_is_active { false };
    Pane* m_active { nullptr };
};
}
