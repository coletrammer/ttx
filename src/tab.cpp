#include "tab.h"

#include "di/container/algorithm/count_if.h"
#include "di/container/algorithm/replace.h"
#include "di/serialization/base64.h"
#include "ttx/clipboard.h"
#include "ttx/direction.h"
#include "ttx/focus_event.h"
#include "ttx/ipc/pane_id.h"
#include "ttx/layout.h"
#include "ttx/layout_json.h"
#include "ttx/terminal/escapes/osc_8671.h"
#include "ttx/terminal/navigation_direction.h"
#include "workspace.h"

namespace ttx {
void Tab::layout(Size const& size) {
    m_size = size;

    if (m_full_screen_pane) {
        // TODO: resize full screen pane
        // In full screen mode, circumvent ordinary layout.
        // m_full_screen_pane->resize(m_size);
        m_layout_tree =
            di::make_box<LayoutNode>(0, 0, size, di::Vector<di::Variant<di::Box<LayoutNode>, LayoutEntry>> {}, nullptr,
                                     &m_layout_root, Direction::None);
        m_layout_tree->children.emplace_back(LayoutEntry {
            0,
            0,
            size,
            m_layout_tree.get(),
            m_layout_root.find_layout_pane(m_full_screen_pane.value()),
            m_full_screen_pane.value(),
        });
    } else {
        m_layout_tree = m_layout_root.layout(size, 0, 0);
    }
    invalidate_all();
}

void Tab::invalidate_all() {
    // TODO: different mechanism?
    // for (auto* pane : m_panes_ordered_by_recency) {
    //     pane->invalidate_all();
    // }
}

void Tab::remove_pane(PaneId pane) {
    // Clear full screen pane. The caller makes sure to call layout() for us.
    if (m_full_screen_pane == pane) {
        m_full_screen_pane.reset();
    }

    di::erase(m_panes_ordered_by_recency, pane);

    // Clear active pane.
    if (m_active == pane) {
        set_active(m_panes_ordered_by_recency.front());
    }
}

void Tab::add_pane(PaneId pane, Size const& size, Direction direction) {
    auto new_layout = m_layout_root.split(size, 0, 0, m_active.value_or(PaneId(0)), direction, pane);
    m_layout_tree = di::move(new_layout);
    set_active(pane);
}

void Tab::replace_pane(PaneId original_pane, PaneId new_pane) {
    auto entry = m_layout_tree->find_pane(original_pane);
    ASSERT(entry);

    di::replace(m_panes_ordered_by_recency, original_pane, new_pane);
    if (m_active == original_pane) {
        m_active = new_pane;
        // TODO: focus events...
        // new_pane->event(FocusEvent::focus_in());
    }
    if (m_full_screen_pane == original_pane) {
        m_full_screen_pane = new_pane;
    }

    entry->pane_id = new_pane;
}

auto Tab::navigate(terminal::NavigateDirection direction, terminal::NavigateWrapMode wrap_mode,
                   di::Optional<di::String> id, di::Optional<di::Tuple<u32, u32>> override_range,
                   SeamlessNavigateMode seamless_navigate_mode, bool force_wrap) -> di::Optional<bool> {
    auto layout_entry = m_layout_tree->find_pane(m_active.value_or(PaneId(0)));
    if (!layout_entry) {
        return false;
    }

    auto override_start = override_range.transform([](auto x) {
        return di::get<0>(x);
    });
    auto override_end = override_range.transform([](auto x) {
        return di::get<1>(x);
    });
    auto [candidates, blocked] = [&] -> di::Tuple<di::TreeSet<PaneId>, bool> {
        using enum terminal::NavigateDirection;
        switch (direction) {
            case Left: {
                // Handle wrap.
                auto wraps = layout_entry->col <= 1 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<PaneId> {}, true };
                }
                auto col = wraps ? m_size.cols - 1 : layout_entry->col - 2;
                return { m_layout_tree->hit_test_vertical_line(
                             col, override_start.value_or(layout_entry->row),
                             override_end.value_or(layout_entry->row + layout_entry->size.rows)) |
                             di::transform(&LayoutEntry::pane_id) | di::to<di::TreeSet>(),
                         false };
            }
            case Right: {
                // Handle wrap.
                auto wraps =
                    m_size.cols < 2 || layout_entry->col + layout_entry->size.cols >= m_size.cols - 2 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<PaneId> {}, true };
                }
                auto col = wraps ? 0 : layout_entry->col + layout_entry->size.cols + 1;

                return { m_layout_tree->hit_test_vertical_line(
                             col, override_start.value_or(layout_entry->row),
                             override_end.value_or(layout_entry->row + layout_entry->size.rows)) |
                             di::transform(&LayoutEntry::pane_id) | di::to<di::TreeSet>(),
                         false };
            }
            case Up: {
                // Handle wrap.
                auto wraps = layout_entry->row <= 1 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<PaneId> {}, true };
                }
                auto row = wraps ? m_size.rows - 1 : layout_entry->row - 2;

                return { m_layout_tree->hit_test_horizontal_line(
                             row, override_start.value_or(layout_entry->col),
                             override_end.value_or(layout_entry->col + layout_entry->size.cols)) |
                             di::transform(&LayoutEntry::pane_id) | di::to<di::TreeSet>(),
                         false };
            }
            case Down: {
                // Handle wrap.
                auto wraps =
                    m_size.rows < 2 || layout_entry->row + layout_entry->size.rows >= m_size.rows - 2 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<PaneId> {}, true };
                }
                auto row = wraps ? 0 : layout_entry->row + layout_entry->size.rows + 1;

                return { m_layout_tree->hit_test_horizontal_line(
                             row, override_start.value_or(layout_entry->col),
                             override_end.value_or(layout_entry->col + layout_entry->size.cols)) |
                             di::transform(&LayoutEntry::pane_id) | di::to<di::TreeSet>(),
                         false };
            }
        }
        return {};
    }();

    // If the current active pane supports seamless navigation, it gets priority. In this case we return empty to
    // indicate navigation has not yet been completed.
    auto valid_candidates_count = di::count_if(candidates, [&](PaneId candidate) {
        return candidate != m_active;
    });
    if (seamless_navigate_mode == SeamlessNavigateMode::Enabled) {
        auto osc_8671 = terminal::OSC8671 {
            .type = terminal::SeamlessNavigationRequestType::Navigate,
            .direction = direction,
            .id = di::move(id),
            .wrap_mode = wrap_mode == terminal::NavigateWrapMode::Allow && valid_candidates_count == 0
                             ? terminal::NavigateWrapMode::Allow
                             : terminal::NavigateWrapMode::Disallow,
        };
        auto is_async = osc_8671.wrap_mode == terminal::NavigateWrapMode::Disallow;
        // TODO: send OSC 8671
        (void) is_async;
        // if (m_active->seamless_navigate(di::move(osc_8671))) {
        //     if (is_async) {
        //         return {};
        //     }
        //     return true;
        // }
    }

    if (blocked) {
        return false;
    }

    for (auto candidate : m_panes_ordered_by_recency) {
        // When forcing a wrap we should be stable if the active pane doesn't need to change.
        if (force_wrap && candidate == m_active) {
            return false;
        }
        if (candidate != m_active && candidates.contains(candidate)) {
            // Notify the new active pane we are switching to it.
            auto candidate_layout_entry = m_layout_tree->find_pane(candidate);
            ASSERT(candidate_layout_entry);

            auto range = [&] -> di::Tuple<u32, u32> {
                if (direction == terminal::NavigateDirection::Left || direction == terminal::NavigateDirection::Right) {
                    return { di::max(layout_entry->row, candidate_layout_entry->row) - candidate_layout_entry->row + 1,
                             di::min(layout_entry->row + layout_entry->size.rows,
                                     candidate_layout_entry->row + candidate_layout_entry->size.rows) -
                                 candidate_layout_entry->row };
                }
                return { di::max(layout_entry->col, candidate_layout_entry->col) - candidate_layout_entry->col + 1,
                         di::min(layout_entry->col + layout_entry->size.cols,
                                 candidate_layout_entry->col + candidate_layout_entry->size.cols) -
                             candidate_layout_entry->col };
            }();
            auto osc_8671 = terminal::OSC8671 {
                .type = terminal::SeamlessNavigationRequestType::Enter,
                .direction = direction,
                .range = range,
            };
            // TODO: send osc 8671
            // candidate->seamless_navigate(di::move(osc_8671));
            (void) osc_8671;

            set_active(candidate);
            return true;
        }
    }
    return false;
}

auto Tab::set_full_screen_pane(di::Optional<PaneId> pane) -> bool {
    if (m_full_screen_pane == pane) {
        return false;
    }

    if (pane.has_value()) {
        m_full_screen_pane = {};
        layout(m_size);
        return true;
    }

    m_full_screen_pane = pane;
    set_active(pane);
    layout(m_size);
    return true;
}

auto Tab::set_active(di::Optional<PaneId> pane) -> bool {
    if (m_active == pane) {
        return false;
    }

    // Clear full screen pane, if said pane is no longer focused.
    if (m_full_screen_pane && m_full_screen_pane != pane) {
        m_full_screen_pane = {};
        layout(m_size);
    }

    // Unfocus the old pane, and focus the new pane.
    if (is_active() && m_active) {
        // TODO: focus event
        // m_active->event(FocusEvent::focus_out());
    }
    m_active = pane;
    if (pane) {
        di::erase(m_panes_ordered_by_recency, pane.value());
        m_panes_ordered_by_recency.push_front(pane.value());
    }
    if (is_active() && m_active) {
        // TODO: focus event
        // m_active->event(FocusEvent::focus_in());
    }
    return true;
}

auto Tab::set_is_active(bool b) -> bool {
    if (m_is_active == b) {
        return false;
    }

    // Send focus in/out events appropriately.
    if (m_active) {
        // TODO: focus event
        // m_active->event(FocusEvent::focus_out());
    }
    m_is_active = b;
    if (is_active() && m_active) {
        // TODO: focus event
        // m_active->event(FocusEvent::focus_in());
    }
    return true;
}

auto Tab::as_json_v1() const -> json::v1::Tab {
    auto json = json::v1::Tab {};
    json.name = m_name.clone();
    json.id = id();
    for (auto& pane : full_screen_pane()) {
        json.full_screen_pane_id = pane;
    }
    for (auto& pane : active()) {
        json.active_pane_id = pane;
    }
    for (auto pane : m_panes_ordered_by_recency) {
        json.pane_ids_by_recency.push_back(pane);
    }
    json.pane_layout = m_layout_root.as_json_v1();
    return json;
}

auto Tab::from_json_v1(json::v1::Tab const& json, Workspace* workspace, Size size) -> di::Result<di::Box<Tab>> {
    // This is needed because the JSON parser will accept missing fields for default constructible types.
    if (json.id == TabId(0)) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto result = di::make_box<Tab>(workspace, json.id, json.name.clone());
    result->m_size = size;

    // TODO: setup this vector
    auto panes = di::Vector<PaneId> {};
    result->m_layout_root = TRY(LayoutGroup::from_json_v1(json.pane_layout, size));

    // If there are any panes missing from the list, add them to the end.
    auto counted_panes = result->m_panes_ordered_by_recency | di::to<di::TreeSet>();
    for (auto pane : panes) {
        if (!counted_panes.contains(pane)) {
            result->m_panes_ordered_by_recency.push_back(pane);
        }
    }

    // Full screen pane should always be active.
    if (json.full_screen_pane_id) {
        auto* it = di::find(panes, json.full_screen_pane_id.value());
        if (it != panes.end()) {
            result->set_full_screen_pane(*it);
        }
    } else if (json.active_pane_id) {
        auto* it = di::find(panes, json.active_pane_id.value());
        if (it != panes.end()) {
            result->set_active(*it);
        }
    }

    if (result->m_panes_ordered_by_recency.empty()) {
        return result;
    }

    // Fallback case: set the first pane as active.
    if (!result->m_active) {
        result->set_active(result->m_panes_ordered_by_recency[0]);
    }

    return result;
}

auto Tab::layout_state() const -> LayoutState& {
    return m_workspace->layout_state();
}
}
