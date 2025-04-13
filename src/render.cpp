#include "render.h"

#include "layout_state.h"
#include "ttx/layout.h"

namespace ttx {
auto RenderThread::create(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit)
    -> di::Result<di::Box<RenderThread>> {
    auto result = di::make_box<RenderThread>(layout_state, di::move(did_exit));
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.render_thread();
    }));
    return result;
}

RenderThread::RenderThread(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit)
    : m_layout_state(layout_state), m_did_exit(di::move(did_exit)) {}

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

    auto deadline = dius::SteadyClock::now();
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
            } else if (auto ev = di::get_if<PaneExited>(event)) {
                // Exit pane.
                auto [pane, should_exit] = m_layout_state.with_lock([&](LayoutState& state) {
                    auto pane = state.remove_pane(*ev->tab, ev->pane);
                    return di::Tuple { di::move(pane), state.empty() };
                });
                if (should_exit) {
                    return;
                }
            } else if (auto ev = di::get_if<InputStatus>(event)) {
                m_input_status = *ev;
            } else if (auto ev = di::get_if<WriteString>(event)) {
                (void) dius::stdin.write_exactly(di::as_bytes(ev->string.span()));
            } else if (auto ev = di::get_if<DoRender>(event)) {
                // Do nothing. This was just to wake us up.
            } else if (auto ev = di::get_if<Exit>(event)) {
                // Exit.
                return;
            }
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

void RenderThread::do_render(Renderer& renderer) {
    m_layout_state.with_lock([&](LayoutState& state) {
        // Ignore if there is no layout.
        auto active_tab = state.active_tab();
        if (!active_tab) {
            return;
        }
        auto& tab = *active_tab;
        auto tree = tab.layout_tree();
        if (!tree) {
            return;
        }

        // Do the render.
        renderer.start(state.size());

        // Status bar.
        if (!state.hide_status_bar()) {
            auto text = di::enumerate(state.tabs()) | di::transform(di::uncurry([&](usize i, di::Box<Tab> const& tab) {
                            auto sign = U' ';
                            if (tab.get() == active_tab.data()) {
                                if (tab->full_screen_pane()) {
                                    sign = U'+';
                                } else {
                                    sign = U'*';
                                }
                            }
                            return *di::present("[{}{} {}]"_sv, sign, i + 1, tab->name());
                        })) |
                        di::join_with(U' ') | di::to<di::String>();
            renderer.clear_row(0);
            renderer.put_text(di::to_string(m_input_status.mode).view(), 0, 0,
                              GraphicsRendition {
                                  .font_weight = FontWeight::Bold,
                              });
            renderer.put_text(text.view(), 0, 7);
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

        (void) renderer.finish(dius::stdin, cursor.value_or({ .hidden = true }));
    });
}
}
