#include "render.h"

#include "dius/system/process.h"
#include "input_mode.h"
#include "layout_state.h"
#include "ttx/graphics_rendition.h"
#include "ttx/layout.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"
#include "ttx/renderer.h"

namespace ttx {
auto RenderThread::create(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit, Feature features)
    -> di::Result<di::Box<RenderThread>> {
    auto result = di::make_box<RenderThread>(layout_state, di::move(did_exit), features);
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.render_thread();
    }));
    return result;
}

auto RenderThread::create_mock(di::Synchronized<LayoutState>& layout_state) -> RenderThread {
    return RenderThread(layout_state, nullptr, Feature::All);
}

RenderThread::RenderThread(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit, Feature features)
    : m_layout_state(layout_state), m_did_exit(di::move(did_exit)), m_features(features) {}

RenderThread::~RenderThread() {
    (void) m_thread.join();
}

void RenderThread::push_event(RenderEvent event) {
    m_events.with_lock([&](auto& queue) {
        queue.push(di::move(event));
        m_condition.notify_one();
    });
}

void RenderThread::render_thread() {
    auto _ = di::ScopeExit([&] {
        if (m_did_exit) {
            m_did_exit();
        }
    });

    auto renderer = Renderer();
    auto _ = di::ScopeExit([&] {
        (void) renderer.cleanup(dius::stdin);
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
        for (auto& event : events) {
            // Pattern matching would be nice here...
            if (auto ev = di::get_if<Size>(event)) {
                // Do layout.
                m_layout_state.lock()->layout(ev.value());

                // Force doing a resetting the terminal mode on SIGWINCH.
                // This is to enable make putting ttx in a "dumb" session
                // persistence program (like dtach) work correctly.
                do_setup = true;
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
                (void) dius::stdin.write_exactly(di::as_bytes(ev->string.span()));
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
            } else if (auto ev = di::get_if<DoRender>(event)) {
                // Do nothing. This was just to wake us up.
            } else if (auto ev = di::get_if<Exit>(event)) {
                // Exit.
                return;
            }
        }

        // Do terminal setup if requested.
        if (do_setup) {
            (void) renderer.setup(dius::stdin, m_features);
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
    bool have_status_bar { false };

    void operator()(di::Box<LayoutNode> const& node) { (*this)(*node); }

    void operator()(LayoutNode& node) {
        auto first = true;
        for (auto const& child : node.children) {
            if (!first) {
                // Draw a border around the pane.
                auto [row, col, size] = di::visit(PositionAndSize {}, child);
                renderer.set_bound(0, 0, state.size().cols, state.size().rows);
                if (node.direction == Direction::Horizontal) {
                    for (auto r : di::range(row + have_status_bar, row + have_status_bar + size.rows)) {
                        auto code_point = U'│';
                        renderer.put_text(code_point, r, col - 1);
                    }
                } else if (node.direction == Direction::Vertical) {
                    for (auto c : di::range(col, col + size.cols)) {
                        auto code_point = U'─';
                        renderer.put_text(code_point, row + have_status_bar - 1, c);
                    }
                }
            }
            first = false;

            di::visit(*this, child);
        }
    }

    void operator()(LayoutEntry const& entry) {
        renderer.set_bound(entry.row + have_status_bar, entry.col, entry.size.cols, entry.size.rows);
        auto pane_cursor = entry.pane->draw(renderer);
        if (entry.pane == tab.active().data()) {
            pane_cursor.cursor_row += have_status_bar;
            pane_cursor.cursor_row += entry.row;
            pane_cursor.cursor_col += entry.col;
            cursor = pane_cursor;
        }
    }
};

void RenderThread::render_status_bar(LayoutState const& state, Renderer& renderer) {
    auto const dark_bg = Color(0x11, 0x11, 0x1b);
    auto const light_bg = Color(0x31, 0x32, 0x44);
    auto const dark_fg = Color(0x1e, 0x1e, 0x2e);
    auto const active_color = Color(Color::Palette::Yellow);
    auto const inactive_color = Color(Color::Palette::Blue);
    auto const separator = U'█';
    for (auto& session : state.active_session()) {
        auto offset = 0u;
        renderer.clear_row(0, GraphicsRendition { .bg = dark_bg });

        {
            auto color = [&] {
                switch (m_input_status.mode) {
                    case ttx::InputMode::Switch:
                        return Color(Color::Palette::Yellow);
                    case ttx::InputMode::Insert:
                        return Color(Color::Palette::Blue);
                    case ttx::InputMode::Normal:
                        return Color(Color::Palette::Green);
                    case ttx::InputMode::Resize:
                        return Color(Color::Palette::Red);
                }
                return Color(Color::Palette::Blue);
            }();
            renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
            renderer.put_text(di::to_string(m_input_status.mode).view(), 0, offset,
                              GraphicsRendition {
                                  .fg = dark_fg,
                                  .bg = color,
                                  .font_weight = FontWeight::Bold,
                              });
            offset += 6;
            renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
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

                auto status_bar_entry = StatusBarEntry { offset, 0 };

                auto num_string = di::to_string(i + 1);
                renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
                renderer.put_text(num_string.view(), 0, offset, { .fg = dark_fg, .bg = color });
                offset += num_string.size_bytes();
                renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
                renderer.put_text(tab->name(), 0, offset, { .bg = light_bg });
                offset += tab->name().size_bytes();
                if (sign != ' ') {
                    renderer.put_text(' ', 0, offset++, { .bg = light_bg });
                    renderer.put_text(sign, 0, offset++, { .bg = light_bg });
                    renderer.put_text(' ', 0, offset++, { .bg = light_bg });
                }
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
                status_bar_entry.width = offset - status_bar_entry.start;

                m_status_bar_layout.push_back(status_bar_entry);
                offset++;
            }
        } else {
            renderer.put_text(m_pending_status_message->message.view(), 0, offset, GraphicsRendition { .bg = dark_bg });
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
                auto color = Color(Color::Palette::Green);
                renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
                renderer.put_text(U'', 0, offset++, { .fg = dark_fg, .bg = color });
                renderer.put_text(' ', 0, offset++, { .bg = color });
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
                renderer.put_text(session.name().view(), 0, offset, { .bg = light_bg });
                offset += session.name().size_bytes();
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
            }

            {
                auto color = Color(Color::Palette::Magenta);
                renderer.put_text(separator, 0, offset++, { .fg = color, .bg = color });
                renderer.put_text(U'󰒋', 0, offset++, { .fg = dark_fg, .bg = color });
                renderer.put_text(' ', 0, offset++, { .bg = color });
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
                renderer.put_text(di::to_string(hostname).view(), 0, offset, { .bg = light_bg });
                offset += hostname.size_bytes();
                renderer.put_text(separator, 0, offset++, { .fg = light_bg, .bg = light_bg });
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

        // Do the render.
        renderer.start(state.size());

        // Status bar.
        if (!state.hide_status_bar()) {
            render_status_bar(state, renderer);
        }

        auto cursor = di::Optional<RenderedCursor> {};

        // First render all panes in the layout tree.
        auto render_fn = Render(renderer, cursor, tab, state, !state.hide_status_bar());
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

    (void) renderer.finish(dius::stdin, cursor.value_or({ .hidden = true }));
}
}
