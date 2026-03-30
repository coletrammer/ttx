#include "tab.h"

#include "di/container/algorithm/count_if.h"
#include "di/container/algorithm/replace.h"
#include "di/serialization/base64.h"
#include "input.h"
#include "render.h"
#include "ttx/clipboard.h"
#include "ttx/direction.h"
#include "ttx/focus_event.h"
#include "ttx/layout.h"
#include "ttx/layout_json.h"
#include "ttx/terminal/escapes/osc_8671.h"
#include "ttx/terminal/navigation_direction.h"

namespace ttx {
void Tab::layout(Size const& size) {
    m_size = size;

    if (m_full_screen_pane) {
        // In full screen mode, circumvent ordinary layout.
        m_full_screen_pane->resize(m_size);
        m_layout_tree =
            di::make_box<LayoutNode>(0, 0, size, di::Vector<di::Variant<di::Box<LayoutNode>, LayoutEntry>> {}, nullptr,
                                     &m_layout_root, Direction::None);
        m_layout_tree->children.emplace_back(
            LayoutEntry { 0, 0, size, m_layout_tree.get(), nullptr, m_full_screen_pane });
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

    return m_layout_root.remove_pane(pane);
}

auto Tab::add_pane(u64 pane_id, Size const& size, CreatePaneArgs args, Direction direction, RenderThread& render_thread,
                   InputThread& input_thread) -> di::Result<> {
    auto [new_layout, pane_layout, pane_out] = m_layout_root.split(size, 0, 0, m_active, direction);

    if (!pane_layout || !pane_out || pane_layout->size == Size {}) {
        // NOTE: this happens when the visible terminal size is too small.
        m_layout_root.remove_pane(nullptr);
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto maybe_pane = make_pane(pane_id, di::move(args), pane_layout->size, render_thread, input_thread);
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

auto Tab::replace_pane(Pane& pane, CreatePaneArgs args, RenderThread& render_thread, InputThread& input_thread)
    -> di::Result<> {
    auto entry = m_layout_tree->find_pane(&pane);
    if (!entry) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto new_size = m_full_screen_pane == &pane ? m_size : entry->size;
    auto new_pane = TRY(make_pane(pane.id(), di::move(args), new_size, render_thread, input_thread));

    di::replace(m_panes_ordered_by_recency, &pane, new_pane.get());
    if (m_active == &pane) {
        m_active = new_pane.get();
        new_pane->event(FocusEvent::focus_in());
    }
    if (m_full_screen_pane == &pane) {
        m_full_screen_pane = new_pane.get();
    }

    entry->pane = new_pane.get();

    // Now remove the old pane.
    pane.exit();
    // SAFETY: this const case is safe because we are holding the layout state lock. The
    // mutation is safe because we aren't changing any field other than the pane object,
    // and everything else remains unchanged.
    const_cast<LayoutPane*>(entry->ref)->pane = di::move(new_pane);
    return {};
}

auto Tab::pane_by_id(u64 pane_id) -> di::Optional<Pane&> {
    auto pane = di::find(m_panes_ordered_by_recency, pane_id, &Pane::id);
    if (pane == m_panes_ordered_by_recency.end()) {
        return {};
    }
    return **pane;
}

auto Tab::navigate(terminal::NavigateDirection direction, terminal::NavigateWrapMode wrap_mode,
                   di::Optional<di::String> id, di::Optional<di::Tuple<u32, u32>> override_range,
                   SeamlessNavigateMode seamless_navigate_mode, bool force_wrap) -> di::Optional<bool> {
    auto layout_entry = m_layout_tree->find_pane(m_active);
    if (!layout_entry) {
        return false;
    }

    auto override_start = override_range.transform([](auto x) {
        return di::get<0>(x);
    });
    auto override_end = override_range.transform([](auto x) {
        return di::get<1>(x);
    });
    auto [candidates, blocked] = [&] -> di::Tuple<di::TreeSet<Pane*>, bool> {
        using enum terminal::NavigateDirection;
        switch (direction) {
            case Left: {
                // Handle wrap.
                auto wraps = layout_entry->col <= 1 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<Pane*> {}, true };
                }
                auto col = wraps ? m_size.cols - 1 : layout_entry->col - 2;
                return { m_layout_tree->hit_test_vertical_line(
                             col, override_start.value_or(layout_entry->row),
                             override_end.value_or(layout_entry->row + layout_entry->size.rows)) |
                             di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>(),
                         false };
            }
            case Right: {
                // Handle wrap.
                auto wraps =
                    m_size.cols < 2 || layout_entry->col + layout_entry->size.cols >= m_size.cols - 2 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<Pane*> {}, true };
                }
                auto col = wraps ? 0 : layout_entry->col + layout_entry->size.cols + 1;

                return { m_layout_tree->hit_test_vertical_line(
                             col, override_start.value_or(layout_entry->row),
                             override_end.value_or(layout_entry->row + layout_entry->size.rows)) |
                             di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>(),
                         false };
            }
            case Up: {
                // Handle wrap.
                auto wraps = layout_entry->row <= 1 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<Pane*> {}, true };
                }
                auto row = wraps ? m_size.rows - 1 : layout_entry->row - 2;

                return { m_layout_tree->hit_test_horizontal_line(
                             row, override_start.value_or(layout_entry->col),
                             override_end.value_or(layout_entry->col + layout_entry->size.cols)) |
                             di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>(),
                         false };
            }
            case Down: {
                // Handle wrap.
                auto wraps =
                    m_size.rows < 2 || layout_entry->row + layout_entry->size.rows >= m_size.rows - 2 || force_wrap;
                if (wraps && wrap_mode == terminal::NavigateWrapMode::Disallow) {
                    return { di::TreeSet<Pane*> {}, true };
                }
                auto row = wraps ? 0 : layout_entry->row + layout_entry->size.rows + 1;

                return { m_layout_tree->hit_test_horizontal_line(
                             row, override_start.value_or(layout_entry->col),
                             override_end.value_or(layout_entry->col + layout_entry->size.cols)) |
                             di::transform(&LayoutEntry::pane) | di::to<di::TreeSet>(),
                         false };
            }
        }
        return {};
    }();

    // If the current active pane supports seamless navigation, it gets priority. In this case we return empty to
    // indicate navigation has not yet been completed.
    auto valid_candidates_count = di::count_if(candidates, [&](Pane* candidate) {
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
        if (m_active->seamless_navigate(di::move(osc_8671))) {
            if (is_async) {
                return {};
            }
            return true;
        }
    }

    if (blocked) {
        return false;
    }

    for (auto* candidate : m_panes_ordered_by_recency) {
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
            candidate->seamless_navigate(di::move(osc_8671));

            set_active(candidate);
            return true;
        }
    }
    return false;
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

    auto _ = di::ScopeExit(di::bind_front(&Tab::layout_did_update, this));

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
    if (m_active) {
        m_active->event(FocusEvent::focus_out());
    }
    m_is_active = b;
    if (is_active() && m_active) {
        m_active->event(FocusEvent::focus_in());
    }
    return true;
}

auto Tab::make_pane(u64 pane_id, CreatePaneArgs args, Size const& size, RenderThread& render_thread,
                    InputThread& input_thread) -> di::Result<di::Box<Pane>> {
    if (!args.hooks.did_exit) {
        args.hooks.did_exit = [this, &render_thread](Pane& pane, di::Optional<dius::system::ProcessResult>) {
            render_thread.push_event(PaneExited(m_session, this, &pane));
        };
    }
    return layout_state().make_pane_with_default_hooks(di::move(args), size,
                                                       Clipboard::Identifier {
                                                           .session_id = m_session->id(),
                                                           .tab_id = id(),
                                                           .pane_id = pane_id,
                                                       },
                                                       render_thread, input_thread);
}

void Tab::layout_did_update() {
    m_session->layout_did_update();
}

void Tab::for_each_pane(di::FunctionRef<void(Pane&)> action) {
    for (auto* pane : m_panes_ordered_by_recency) {
        action(*pane);
    }
}

auto Tab::as_json_v1() const -> json::v1::Tab {
    auto json = json::v1::Tab {};
    json.name = m_name.clone();
    json.id = id();
    for (auto& pane : full_screen_pane()) {
        json.full_screen_pane_id = pane.id();
    }
    for (auto& pane : active()) {
        json.active_pane_id = pane.id();
    }
    for (auto* pane : m_panes_ordered_by_recency) {
        json.pane_ids_by_recency.push_back(pane->id());
    }
    json.pane_layout = m_layout_root.as_json_v1();
    return json;
}

auto Tab::from_json_v1(json::v1::Tab const& json, Session* session, Size size, CreatePaneArgs args,
                       RenderThread& render_thread, InputThread& input_thread) -> di::Result<di::Box<Tab>> {
    // This is needed because the JSON parser will accept missing fields for default constructible types.
    if (json.id == 0) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto result = di::make_box<Tab>(session, json.id, json.name.clone());
    result->m_size = size;

    auto panes = di::Vector<Pane*> {};
    result->m_layout_root = TRY(LayoutGroup::from_json_v1(
        json.pane_layout, size,
        [&](u64 pane_id, di::Optional<di::Path> cwd, Size const& pane_size) -> di::Result<di::Box<Pane>> {
            auto cloned_args = args.clone();
            cloned_args.cwd = di::move(cwd);
            auto pane = result->make_pane(pane_id, di::move(cloned_args), pane_size, render_thread, input_thread);
            if (pane) {
                panes.push_back(pane.value().get());
            }
            return pane;
        }));

    // If there are any panes missing from the list, add them to the end.
    auto counted_panes = result->m_panes_ordered_by_recency | di::to<di::TreeSet>();
    for (auto* pane : panes) {
        if (!counted_panes.contains(pane)) {
            result->m_panes_ordered_by_recency.push_back(pane);
        }
    }

    // Full screen pane should always be active.
    if (json.full_screen_pane_id) {
        auto* it = di::find(panes, json.full_screen_pane_id.value(), &Pane::id);
        if (it != panes.end()) {
            result->set_full_screen_pane(*it);
        }
    } else if (json.active_pane_id) {
        auto* it = di::find(panes, json.active_pane_id.value(), &Pane::id);
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

auto Tab::max_pane_id() const -> u64 {
    if (m_panes_ordered_by_recency.empty()) {
        return 1;
    }
    return di::max(m_panes_ordered_by_recency | di::transform(&Pane::id));
}

auto Tab::layout_state() const -> LayoutState& {
    return m_session->layout_state();
}
}
