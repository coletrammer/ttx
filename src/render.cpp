#include "render.h"

#include "dius/print.h"
#include "dius/system/process.h"
#include "input_mode.h"
#include "layout_state.h"
#include "theme.h"
#include "ttx/clipboard.h"
#include "ttx/layout.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"
#include "ttx/renderer.h"
#include "ttx/terminal/graphics_rendition.h"

namespace ttx {
auto RenderThread::create(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit, Config config,
                          Feature features) -> di::Result<di::Box<RenderThread>> {
    auto result = di::make_box<RenderThread>(layout_state, di::move(did_exit), di::move(config), features);
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.render_thread();
    }));
    return result;
}

auto RenderThread::create_mock(di::Synchronized<LayoutState>& layout_state) -> RenderThread {
    return RenderThread(layout_state, nullptr, {}, Feature::All);
}

RenderThread::RenderThread(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit, Config config,
                           Feature features)
    : m_layout_state(layout_state)
    , m_did_exit(di::move(did_exit))
    , m_clipboard(config.clipboard.mode, features)
    , m_config(di::move(config))
    , m_features(features) {}

RenderThread::~RenderThread() {
    (void) m_thread.join();
}

void RenderThread::push_event(RenderEvent event) {
    m_events.with_lock([&](auto& queue) {
        queue.push(di::move(event));
        m_condition.notify_one();
    });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RenderThread::render_thread() {
    auto _ = di::ScopeExit([&] {
        if (m_did_exit) {
            m_did_exit();
        }
    });

    auto renderer = Renderer();
    auto _ = di::ScopeExit([&] {
        (void) renderer.cleanup(dius::std_in);
    });

    auto deadline = dius::SteadyClock::now();
    auto do_setup = true;
    for (;;) {
        while (deadline < dius::SteadyClock::now()) {
            deadline += di::Milliseconds(25); // 50 FPS
        }
        dius::this_thread::sleep_until(deadline);

        // Fetch events all events from the queue.
        auto events = [&] {
            auto lock = di::UniqueLock(m_events.get_lock());
            m_condition.wait(lock, [&] {
                // SAFETY: we acquired the lock manually above.
                return !m_events.get_assuming_no_concurrent_accesses().empty();
            });

            // SAFETY: we acquired the lock manually above.
            auto result = di::Vector<RenderEvent> {};
            for (auto& event : m_events.get_assuming_no_concurrent_accesses()) {
                result.push_back(di::move(event));
            }
            return result;
        }();

        // Process any pending events.
        auto new_size = di::Optional<Size> {};
        for (auto& event : events) {
            // Pattern matching would be nice here...
            if (auto ev = di::get_if<Size>(event)) {
                new_size = ev.value();
            } else if (auto ev = di::get_if<PaneExited>(event)) {
                // Exit pane.
                auto [pane, should_exit] = m_layout_state.with_lock([&](LayoutState& state) {
                    auto pane = state.remove_pane(*ev->session, *ev->tab, ev->pane);
                    return di::Tuple { di::move(pane), state.empty() };
                });
                if (should_exit) {
                    return;
                }
            } else if (auto ev = di::get_if<StatusMessage>(event)) {
                m_pending_status_message = {
                    di::move(ev->message),
                    dius::SteadyClock::now() + ev->duration,
                };
            } else if (auto ev = di::get_if<InputStatus>(event)) {
                m_input_status = *ev;
            } else if (auto ev = di::get_if<WriteString>(event)) {
                (void) dius::std_in.write_exactly(di::as_bytes(ev->string.span()));
            } else if (auto ev = di::get_if<MouseEvent>(event)) {
                if (ev->type() == MouseEventType::Press && ev->button() == MouseButton::Left) {
                    auto* it = di::find_if(m_status_bar_layout, [&](StatusBarEntry const& entry) {
                        return ev->position().in_cells().x() >= entry.start &&
                               ev->position().in_cells().x() < entry.start + entry.width;
                    });
                    if (it != m_status_bar_layout.end()) {
                        auto index = usize(it - m_status_bar_layout.begin());
                        m_layout_state.with_lock([&](LayoutState& state) {
                            if (auto session = state.active_session()) {
                                if (auto tab = session.value().tabs().at(index)) {
                                    state.set_active_tab(session.value(),
                                                         tab.transform(&di::Box<Tab>::get).value_or(nullptr));
                                }
                            }
                        });
                    }
                }
            } else if (auto ev = di::get_if<ClipboardRequest>(event)) {
                // We only need to check the first selection, because the parser ensures there is
                // at least 1 selection and maps selections into ones we support.
                ASSERT(!ev->osc52.selections.empty());
                auto selection_type = ev->osc52.selections[0];

                if (ev->reply) {
                    if (!ev->osc52.query) {
                        m_clipboard.got_clipboard_response(selection_type, di::move(ev->osc52.data).container());
                    }
                } else {
                    ASSERT(ev->identifier.has_value());
                    if (ev->osc52.query) {
                        auto string = ev->osc52.serialize();
                        if (m_clipboard.request_clipboard(selection_type, ev->identifier.value())) {
                            // Forward the query.
                            (void) dius::std_in.write_exactly(di::as_bytes(string.span()));
                        }
                    } else {
                        auto string = ev->osc52.serialize();
                        if (m_clipboard.set_clipboard(selection_type, di::move(ev->osc52.data).container())) {
                            // Forward setting the clipboard.
                            (void) dius::std_in.write_exactly(di::as_bytes(string.span()));
                        }
                        if (ev->manual) {
                            m_pending_status_message = {
                                "Copied text"_s,
                                dius::SteadyClock::now() + di::chrono::Seconds(1),
                            };
                        }
                    }
                }
            } else if (auto ev = di::get_if<UpdateConfig>(event)) {
                auto palette_did_change = m_config.colors != ev->config.colors;
                m_layout_state.with_lock([&](LayoutState& state) {
                    state.set_config(di::clone(ev->config));
                    if (palette_did_change) {
                        for (auto& tab : state.active_tab()) {
                            tab.invalidate_all();
                        }
                    }
                });
                m_clipboard.set_mode(ev->config.clipboard.mode);
                m_config = di::move(ev->config);
            } else if (auto ev = di::get_if<DoRender>(event)) {
                // Do nothing. This was just to wake us up.
            } else if (auto ev = di::get_if<Exit>(event)) {
                // Exit.
                return;
            }
        }

        if (new_size) {
            // Do layout.
            m_layout_state.lock()->layout(new_size.value());

            // Force doing a resetting the terminal mode on SIGWINCH.
            // This is to enable make putting ttx in a "dumb" session
            // persistence program (like dtach) work correctly.
            do_setup = true;
        }

        // Handle any filled clipboard requests.
        auto clipboard_respones = m_clipboard.get_replies();
        if (!clipboard_respones.empty()) {
            m_layout_state.with_lock([&](LayoutState& state) {
                for (auto& response : clipboard_respones) {
                    if (auto pane = state.pane_by_id(response.identifier.session_id, response.identifier.tab_id,
                                                     response.identifier.pane_id)) {
                        pane.value().send_clipboard(response.type, di::move(response.data));
                    }
                }
            });
        }

        // Do terminal setup if requested.
        if (do_setup) {
            (void) renderer.setup(dius::std_in, m_features, m_clipboard.mode());
            do_setup = false;
        }

        // Maybe expire pending status message.
        if (m_pending_status_message && dius::SteadyClock::now() > m_pending_status_message.value().expiration) {
            m_pending_status_message.reset();
        }

        // Do render.
        do_render(renderer);
    }
}

struct PositionAndSize {
    static auto operator()(di::Box<LayoutNode> const& node) -> di::Tuple<u32, u32, Size> {
        return { node->row, node->col, node->size };
    }

    static auto operator()(LayoutEntry const& entry) -> di::Tuple<u32, u32, Size> {
        return { entry.row, entry.col, entry.size };
    }
};

struct Render {
    Renderer& renderer;
    di::Optional<RenderedCursor>& cursor;
    Tab& tab;
    LayoutState& state;
    bool have_top_status_bar { false };

    auto border_code_point(Direction direction, u32 pos, di::Span<u32 const>& interior_intersections,
                           di::Span<u32 const>& exterior_intersections) -> c32 {
        auto combined_intersections = (direction == Direction::Horizontal) << 2;
        if (interior_intersections.front().has_value() && interior_intersections.front().value() == pos) {
            combined_intersections |= 1 << 0;
            interior_intersections = interior_intersections.subspan(1).value();
        }
        if (exterior_intersections.front().has_value() && exterior_intersections.front().value() == pos) {
            combined_intersections |= 1 << 1;
            exterior_intersections = exterior_intersections.subspan(1).value();
        }

        constexpr static auto code_point_lookup = di::Array { U'─', U'┬', U'┴', U'┼', U'│', U'├', U'┤', U'┼' };
        return code_point_lookup[combined_intersections];
    }

    void operator()(di::Box<LayoutNode> const& node) { (*this)(*node); }

    void operator()(LayoutNode& node) {
        // Special case to visit the first child since no preceding border is drawn for it.
        if (!node.children.empty()) {
            di::visit(*this, node.children.front().value());
        }

        for (auto const&& [prev_child, child] : node.children | di::pairwise) {
            auto [_, exterior_intersections] = border_intersections(prev_child);
            auto [interior_intersections, _] = border_intersections(child);

            // Draw a border around the pane.
            auto [row, col, size] = di::visit(PositionAndSize {}, child);
            renderer.set_bound(0, 0, state.size().cols, state.size().rows);
            if (node.direction == Direction::Horizontal) {
                for (auto r : di::range(row + have_top_status_bar, row + have_top_status_bar + size.rows)) {
                    auto code_point = border_code_point(node.direction, r - have_top_status_bar, interior_intersections,
                                                        exterior_intersections);
                    renderer.put_text(code_point, r, col - 1);
                }
            } else if (node.direction == Direction::Vertical) {
                for (auto c : di::range(col, col + size.cols)) {
                    auto code_point =
                        border_code_point(node.direction, c, interior_intersections, exterior_intersections);
                    renderer.put_text(code_point, row + have_top_status_bar - 1, c);
                }
            }

            di::visit(*this, child);
        }
    }

    void operator()(LayoutEntry const& entry) {
        renderer.set_bound(entry.row + have_top_status_bar, entry.col, entry.size.cols, entry.size.rows);
        auto pane_cursor = entry.pane->draw(renderer);
        if (entry.pane == tab.active().data()) {
            pane_cursor.cursor_row += have_top_status_bar;
            pane_cursor.cursor_row += entry.row;
            pane_cursor.cursor_col += entry.col;
            cursor = pane_cursor;
        }
    }
};

void RenderThread::render_status_bar(LayoutState const& state, Renderer& renderer, StatusBarConfig const& config) {
    auto const& colors = config.colors;
    auto const dark_bg = colors.background_color;
    auto const label_bg = colors.label_background_color;
    auto const label_fg = colors.label_text_color;
    auto const badge_fg = colors.badge_text_color;
    auto const active_color = colors.active_tab_badge_color;
    auto const inactive_color = colors.inactive_tab_badge_color;
    auto const separator = U'█';

    auto make_badge_sgr = [&](terminal::Color bg, bool bold = false) -> terminal::GraphicsRendition {
        if (badge_fg.is_dynamic()) {
            return terminal::GraphicsRendition {
                .fg = bg,
                .bg = {},
                .font_weight = bold ? terminal::FontWeight::Bold : terminal::FontWeight::None,
                .inverted = true,
            };
        }
        return terminal::GraphicsRendition {
            .fg = badge_fg,
            .bg = bg,
            .font_weight = bold ? terminal::FontWeight::Bold : terminal::FontWeight::None,
        };
    };

    auto const status_bar_position = state.status_bar_position().value();
    for (auto& session : state.active_session()) {
        auto offset = 0u;
        renderer.clear_row(status_bar_position, terminal::GraphicsRendition { .bg = dark_bg });

        {
            auto color = [&] -> terminal::Color {
                switch (m_input_status.mode) {
                    case ttx::InputMode::Switch:
                        return colors.switch_mode_color;
                    case ttx::InputMode::Insert:
                        return colors.insert_mode_color;
                    case ttx::InputMode::Normal:
                        return colors.normal_mode_color;
                    case ttx::InputMode::Resize:
                        return colors.resize_mode_color;
                }
                return {};
            }();
            renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
            renderer.put_text(di::to_string(m_input_status.mode).view(), status_bar_position, offset,
                              make_badge_sgr(color, true));
            offset += 6;
            renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
            offset += 1;
        }

        m_status_bar_layout.clear();
        if (!m_pending_status_message) {
            for (auto [i, tab] : di::enumerate(session.tabs())) {
                auto color = inactive_color;
                auto sign = U' ';
                if (tab.get() == state.active_tab().data()) {
                    color = active_color;
                    if (tab->full_screen_pane()) {
                        sign = U'󰁌';
                    } else {
                        sign = U'󰖯';
                    }
                }

                auto status_bar_entry = StatusBarEntry { offset, status_bar_position };

                auto num_string = di::to_string(i + 1);
                renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
                renderer.put_text(num_string.view(), status_bar_position, offset, make_badge_sgr(color));
                offset += num_string.size_bytes();
                renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
                renderer.put_text(tab->name(), status_bar_position, offset, { .fg = label_fg, .bg = label_bg });
                offset += tab->name().size_bytes();
                if (sign != ' ') {
                    renderer.put_text(' ', status_bar_position, offset++, { .fg = label_fg, .bg = label_bg });
                    renderer.put_text(sign, status_bar_position, offset++, { .fg = label_fg, .bg = label_bg });
                    renderer.put_text(' ', status_bar_position, offset++, { .fg = label_fg, .bg = label_bg });
                }
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
                status_bar_entry.width = offset - status_bar_entry.start;

                m_status_bar_layout.push_back(status_bar_entry);
                offset++;
            }
        } else {
            renderer.put_text(m_pending_status_message->message.view(), status_bar_position, offset,
                              terminal::GraphicsRendition { .fg = label_fg, .bg = dark_bg });
        }

        // TODO: horizontal scrolling on overflow

        // TODO: this code isn't correct if the session name contains any multi-code point grapheme clusters.
        // TODO: handle case where session name is longer than the status bar width.
        {
            // 5 padding cols (including icon) per section.
            auto hostname = dius::system::get_hostname().value_or("unknown"_ts);
            auto rhs_size = 5_usize * 2 + session.name().size_bytes() + hostname.size();
            if (rhs_size >= state.size().cols || state.size().cols - rhs_size < offset) {
                return;
            }
            offset = state.size().cols - rhs_size;

            {
                auto color = colors.session_badge_background_color;
                renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
                renderer.put_text(U'', status_bar_position, offset++, make_badge_sgr(color));
                renderer.put_text(' ', status_bar_position, offset++, { .bg = color });
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
                renderer.put_text(session.name().view(), status_bar_position, offset,
                                  { .fg = label_fg, .bg = label_bg });
                offset += session.name().size_bytes();
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
            }

            {
                auto color = colors.host_badge_background_color;
                renderer.put_text(separator, status_bar_position, offset++, { .fg = color, .bg = color });
                renderer.put_text(U'󰒋', status_bar_position, offset++, make_badge_sgr(color));
                renderer.put_text(' ', status_bar_position, offset++, { .bg = color });
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
                renderer.put_text(di::to_string(hostname).view(), status_bar_position, offset,
                                  { .fg = label_fg, .bg = label_bg });
                offset += hostname.size_bytes();
                renderer.put_text(separator, status_bar_position, offset++, { .fg = label_bg, .bg = label_bg });
            }
        }
    }
}

void RenderThread::do_render(Renderer& renderer) {
    auto cursor = m_layout_state.with_lock([&](LayoutState& state) -> di::Optional<RenderedCursor> {
        // Ignore if there is no layout.
        auto active_tab = state.active_tab();
        if (!active_tab) {
            return {};
        }
        auto& tab = *active_tab;
        auto tree = tab.layout_tree();
        if (!tree) {
            return {};
        }

        // Set global palette.
        auto _ = renderer.set_global_palette(m_config.colors);

        // Do the render.
        renderer.start(state.size());

        // Status bar.
        if (!state.hide_status_bar()) {
            render_status_bar(state, renderer, m_config.status_bar);
        }

        auto cursor = di::Optional<RenderedCursor> {};

        // First render all panes in the layout tree.
        auto render_fn = Render(renderer, cursor, tab, state, state.status_bar_position() == 0_u32);
        render_fn(*tree);

        // If there is a popup, render it.
        for (auto popup_layout : tab.popup_layout()) {
            // For now, always invalidate the popup since we don't have proper damage tracking when
            // panes overlap.
            popup_layout.pane->invalidate_all();
            render_fn(popup_layout);
        }

        return cursor;
    });

    (void) renderer.finish(dius::std_in, cursor.value_or({ .hidden = true }));
}
}
