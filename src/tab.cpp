#include "tab.h"

#include "di/serialization/base64.h"
#include "dius/print.h"
#include "render.h"

namespace ttx {
void Tab::layout(dius::tty::WindowSize const& size, u32 row, u32 col) {
    m_size = size;
    m_layout_tree = m_layout_root.layout(size, row, col);
    invalidate_all();
}

void Tab::invalidate_all() {
    for (auto* pane : m_panes_ordered_by_recency) {
        pane->invalidate_all();
    }
}

auto Tab::remove_pane(Pane* pane) -> di::Box<Pane> {
    // Clear active pane.
    if (m_active == pane) {
        if (pane) {
            di::erase(m_panes_ordered_by_recency, pane);
        }
        auto candidates = m_panes_ordered_by_recency | di::transform([](Pane* pane) {
                              return pane;
                          });
        set_active(candidates.front().value_or(nullptr));
    }

    return m_layout_root.remove_pane(pane);
}

auto Tab::add_pane(dius::tty::WindowSize const& size, u32 row, u32 col, di::Vector<di::TransparentStringView> command,
                   Direction direction, RenderThread& render_thread) -> di::Result<> {
    auto [new_layout, pane_layout, pane_out] = m_layout_root.split(size, row, col, m_active, direction);

    if (!pane_layout || !pane_out || pane_layout->size == dius::tty::WindowSize {}) {
        // NOTE: this happens when the visible terminal size is too small.
        m_layout_root.remove_pane(nullptr);
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto maybe_pane = Pane::create(
        di::move(command), pane_layout->size,
        [this, &render_thread](Pane& pane) {
            render_thread.push_event(PaneExited(this, &pane));
        },
        [&render_thread](Pane&) {
            render_thread.request_render();
        },
        [](di::Span<byte const> data) {
            auto base64 = di::Base64View(data);
            di::writer_println<di::String::Encoding>(dius::stdin, "\033]52;;{}\033\\"_sv, base64);
        },
        [](di::StringView apc_data) {
            // Pass-through APC commands to host terminal. This makes kitty graphics "work">
            (void) di::write_exactly(dius::stdin, di::as_bytes("\033_"_sv.span()));
            (void) di::write_exactly(dius::stdin, di::as_bytes(apc_data.span()));
            (void) di::write_exactly(dius::stdin, di::as_bytes("\033\\"_sv.span()));
        });
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

auto Tab::set_active(Pane* pane) -> bool {
    if (m_active == pane) {
        return false;
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
}
