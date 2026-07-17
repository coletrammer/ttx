#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/ids.h"
#include "ttx/ipc/pane_id.h"
#include "ttx/layout.h"
#include "ttx/layout_json.h"
#include "ttx/pane.h"
#include "ttx/popup.h"
#include "ttx/terminal/navigation_direction.h"

namespace ttx {
class InputThread;
class RenderThread;

class LayoutState;
class Workspace;

/// @brief Represents a tab (like a "window" in tmux)
class Tab {
public:
    explicit Tab(Workspace* workspace, TabId id, di::Optional<di::String> name = {})
        : m_workspace(workspace), m_id(id), m_name(di::move(name)) {}

    static auto from_json_v1(json::v1::Tab const& json, Workspace* workspace, Size size) -> di::Result<di::Box<Tab>>;

    void layout(Size const& size);
    void invalidate_all();

    void remove_pane(PaneId pane);

    void add_pane(PaneId pane, Size const& size, Direction direction);
    void replace_pane(PaneId original_pane, PaneId new_pane);

    enum class SeamlessNavigateMode { Disabled, Enabled };
    auto navigate(terminal::NavigateDirection direction, terminal::NavigateWrapMode wrap_mode,
                  di::Optional<di::String> id, di::Optional<di::Tuple<u32, u32>> override_range,
                  SeamlessNavigateMode seamless_navigate_mode, bool force_wrap) -> di::Optional<bool>;

    // Returns true if active pane has changed.
    auto set_active(di::Optional<PaneId> pane) -> bool;

    auto id() const { return m_id; }
    auto name() const -> di::Optional<di::StringView> { return m_name.transform(&di::String::view); }
    auto empty() const -> bool { return m_layout_root.empty(); }

    void set_name(di::Optional<di::String> name) { m_name = di::move(name); }

    auto layout_group() -> LayoutGroup& { return m_layout_root; }
    auto layout_tree() const -> di::Optional<LayoutNode&> {
        if (!m_layout_tree) {
            return {};
        }
        return *m_layout_tree;
    }

    auto active() const -> di::Optional<PaneId> {
        if (!m_active) {
            return {};
        }
        return *m_active;
    }

    auto panes() const -> di::Ring<PaneId> const& { return m_panes_ordered_by_recency; }

    auto set_is_active(bool b) -> bool;
    auto is_active() const -> bool { return m_is_active; }

    auto full_screen_pane() const -> di::Optional<PaneId> { return m_full_screen_pane; }
    auto set_full_screen_pane(di::Optional<PaneId> pane) -> bool;

    auto as_json_v1() const -> json::v1::Tab;

    auto layout_state() const -> LayoutState&;

private:
    Workspace* m_workspace { nullptr };
    TabId m_id { 0 };
    Size m_size;
    di::Optional<di::String> m_name;
    LayoutGroup m_layout_root {};
    di::Box<LayoutNode> m_layout_tree {};
    di::Ring<PaneId> m_panes_ordered_by_recency {};
    bool m_is_active { false };
    di::Optional<PaneId> m_active {};
    di::Optional<PaneId> m_full_screen_pane {};
};
}
