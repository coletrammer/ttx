#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/layout.h"
#include "ttx/pane.h"
#include "ttx/popup.h"

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

class Session;

// Corresponds to tmux window.
class Tab {
public:
    explicit Tab(Session* session, di::String name) : m_session(session), m_name(di::move(name)) {}

    void layout(Size const& size);
    void invalidate_all();

    // Returns the removed pane, if found.
    auto remove_pane(Pane* pane) -> di::Box<Pane>;

    auto add_pane(u64 pane_id, Size const& size, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
        -> di::Result<>;
    auto popup_pane(u64 pane_id, PopupLayout const& popup_layout, Size const& size, CreatePaneArgs args,
                    RenderThread& render_thread) -> di::Result<>;

    void navigate(NavigateDirection direction);

    // Returns true if active pane has changed.
    auto set_active(Pane* pane) -> bool;

    auto name() const -> di::StringView { return m_name; }
    auto empty() const -> bool { return m_layout_root.empty() && !m_popup; }

    void set_name(di::String name) { m_name = di::move(name); }

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

    auto panes() const -> di::Ring<Pane*> const& { return m_panes_ordered_by_recency; }

    auto set_is_active(bool b) -> bool;
    auto is_active() const -> bool { return m_is_active; }

    auto full_screen_pane() const -> di::Optional<Pane&> {
        if (!m_full_screen_pane) {
            return {};
        }
        return *m_full_screen_pane;
    }
    auto set_full_screen_pane(Pane* pane) -> bool;

    auto popup_layout() const -> di::Optional<LayoutEntry> { return m_popup_layout; }

private:
    auto make_pane(u64 pane_id, CreatePaneArgs args, Size const& size, RenderThread& render_thread)
        -> di::Result<di::Box<Pane>>;

    Session* m_session { nullptr };
    Size m_size;
    di::String m_name;
    LayoutGroup m_layout_root {};
    di::Box<LayoutNode> m_layout_tree {};
    di::Ring<Pane*> m_panes_ordered_by_recency {};
    bool m_is_active { false };
    Pane* m_active { nullptr };
    Pane* m_full_screen_pane { nullptr };
    di::Optional<Popup> m_popup;
    di::Optional<LayoutEntry> m_popup_layout;
};
}
