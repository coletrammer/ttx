#include "tab.h"

#include "di/serialization/base64.h"
#include "dius/print.h"
#include "render.h"
#include "ttx/direction.h"
#include "ttx/popup.h"

namespace ttx {
void Tab::layout(Size const& size) {
    m_size = size;

    if (m_popup) {
        m_popup_layout = m_popup.value().layout(size);
    }

    if (m_full_screen_pane) {
        // In full screen mode, circumvent ordinary layout.
        m_full_screen_pane->resize(m_size);
        m_layout_tree =
            di::make_box<LayoutNode>(0, 0, size, di::Vector<di::Variant<di::Box<LayoutNode>, LayoutEntry>> {}, nullptr,
                                     &m_layout_root, Direction::None);
        m_layout_tree->children.emplace_back(LayoutEntry { 0, 0, size, m_layout_tree.get(), m_full_screen_pane });
    } else {
        m_layout_tree = m_layout_root.layout(size, 0, 0);
    }
    invalidate_all();
}

void Tab::invalidate_all() {
    for (auto* pane : m_panes_ordered_by_recency) {
        pane->invalidate_all();
    }
}

auto Tab::remove_pane(Pane* pane) -> di::Box<Pane> {
    // Clear full screen pane. The caller makes sure to call layout() for us.
    if (m_full_screen_pane == pane) {
        m_full_screen_pane = nullptr;
    }

    if (pane) {
        di::erase(m_panes_ordered_by_recency, pane);
    }

    // Clear active pane.
    if (m_active == pane) {
        auto candidates = m_panes_ordered_by_recency | di::transform([](Pane* pane) {
                              return pane;
                          });
        set_active(candidates.front().value_or(nullptr));
    }

    // Clean the popup information if this pane was a popup. In this case,
    // we don't return try to remove the pane from the layout tree.
    if (m_popup && m_popup.value().pane.get() == pane) {
        auto result = di::move(m_popup).value().pane;
        m_popup_layout = {};
        m_popup = {};
        return result;
    }

    return m_layout_root.remove_pane(pane);
}

auto Tab::add_pane(u64 pane_id, Size const& size, CreatePaneArgs args, Direction direction, RenderThread& render_thread)
    -> di::Result<> {
    auto [new_layout, pane_layout, pane_out] = m_layout_root.split(size, 0, 0, m_active, direction);

    if (!pane_layout || !pane_out || pane_layout->size == Size {}) {
        // NOTE: this happens when the visible terminal size is too small.
        m_layout_root.remove_pane(nullptr);
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto maybe_pane = make_pane(pane_id, di::move(args), size, render_thread);
    if (!maybe_pane) {
        m_layout_root.remove_pane(nullptr);
        return di::Unexpected(di::move(maybe_pane).error());
    }

    auto& pane = *pane_out = di::move(maybe_pane).value();
    pane_layout->pane = pane.get();
    m_layout_tree = di::move(new_layout);

    set_active(pane.get());
    return {};
}

auto Tab::popup_pane(u64 pane_id, PopupLayout const& popup_layout, Size const& size, CreatePaneArgs args,
                     RenderThread& render_thread) -> di::Result<> {
    m_popup = Popup {
        .pane = nullptr,
        .layout_config = popup_layout,
    };
    m_popup_layout = m_popup.value().layout(size);

    auto maybe_pane = make_pane(pane_id, di::move(args), m_popup_layout.value().size, render_thread);
    if (!maybe_pane) {
        m_popup = {};
        m_popup_layout = {};
        return di::Unexpected(di::move(maybe_pane).error());
    }
    m_popup.value().pane = di::move(maybe_pane).value();
    m_popup_layout.value().pane = m_popup.value().pane.get();

    set_active(m_popup.value().pane.get());
    invalidate_all();
    return {};
}

void Tab::navigate(NavigateDirection direction) {
    auto layout_entry = m_layout_tree->find_pane(m_active);
    if (!layout_entry) {
        return;
    }

    auto candidates = [&] -> di::TreeSet<Pane*> {
        switch (direction) {
            case NavigateDirection::Left: {
                // Handle wrap.
                auto col = layout_entry->col <= 1 ? m_size.cols - 1 : layout_entry->col - 2;
                return m_layout_tree->hit_test_vertical_line(col, layout_entry->row,
                                                             layout_entry->row + layout_entry->size.rows) |
                       di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>();
            }
            case NavigateDirection::Right: {
                // Handle wrap.
                auto col = (m_size.cols < 2 || layout_entry->col + layout_entry->size.cols >= m_size.cols - 2)
                               ? 0
                               : layout_entry->col + layout_entry->size.cols + 1;

                return m_layout_tree->hit_test_vertical_line(col, layout_entry->row,
                                                             layout_entry->row + layout_entry->size.rows) |
                       di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>();
            }
            case NavigateDirection::Up: {
                // Handle wrap.
                auto row = layout_entry->row <= 1 ? m_size.rows - 1 : layout_entry->row - 2;

                return m_layout_tree->hit_test_horizontal_line(row, layout_entry->col,
                                                               layout_entry->col + layout_entry->size.cols) |
                       di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>();
            }
            case NavigateDirection::Down: {
                // Handle wrap.
                auto row = (m_size.rows < 2 || layout_entry->row + layout_entry->size.rows >= m_size.rows - 2)
                               ? 0
                               : layout_entry->row + layout_entry->size.rows + 1;

                return m_layout_tree->hit_test_horizontal_line(row, layout_entry->col,
                                                               layout_entry->col + layout_entry->size.cols) |
                       di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>();
            }
        }
        return {};
    }();

    for (auto* candidate : m_panes_ordered_by_recency) {
        if (candidate != m_active && candidates.contains(candidate)) {
            set_active(candidate);
            break;
        }
    }
}

auto Tab::set_full_screen_pane(Pane* pane) -> bool {
    if (m_full_screen_pane == pane) {
        return false;
    }

    if (pane == nullptr) {
        m_full_screen_pane = nullptr;
        layout(m_size);
        return true;
    }

    m_full_screen_pane = pane;
    set_active(pane);
    layout(m_size);
    return true;
}

auto Tab::set_active(Pane* pane) -> bool {
    if (m_active == pane) {
        return false;
    }

    // Clear full screen pane, if said pane is no longer focused.
    if (m_full_screen_pane && m_full_screen_pane != pane) {
        m_full_screen_pane = nullptr;
        layout(m_size);
    }

    // Unfocus the old pane, and focus the new pane.
    if (is_active() && m_active) {
        m_active->event(FocusEvent::focus_out());
    }
    m_active = pane;
    if (pane) {
        di::erase(m_panes_ordered_by_recency, pane);
        m_panes_ordered_by_recency.push_front(pane);
    }
    if (is_active() && m_active) {
        m_active->event(FocusEvent::focus_in());
    }
    return true;
}

auto Tab::set_is_active(bool b) -> bool {
    if (m_is_active == b) {
        return false;
    }

    // Send focus in/out events appropriately.
    if (is_active() && m_active) {
        m_active->event(FocusEvent::focus_out());
    }
    m_is_active = b;
    if (is_active() && m_active) {
        m_active->event(FocusEvent::focus_in());
    }
    return true;
}

auto Tab::make_pane(u64 pane_id, CreatePaneArgs args, Size const& size, RenderThread& render_thread)
    -> di::Result<di::Box<Pane>> {
    auto hooks = PaneHooks {
        [this, &render_thread](Pane& pane) {
            render_thread.push_event(PaneExited(this, &pane));
        },
        [&render_thread](Pane&) {
            render_thread.request_render();
        },
        [&render_thread](di::Span<byte const> data) {
            auto base64 = di::Base64View(data);
            auto string = *di::present("\033]52;;{}\033\\"_sv, base64);
            render_thread.push_event(WriteString(di::move(string)));
        },
        [&render_thread](di::StringView apc_data) {
            // Pass-through APC commands to host terminal. This makes kitty graphics "work".
            auto string = *di::present("\033_{}\033\\"_sv, apc_data);
            render_thread.push_event(WriteString(di::move(string)));
        },
    };
    return Pane::create(pane_id, di::move(args), size, di::move(hooks));
}
}
