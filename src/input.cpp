#include "input.h"

#include "di/util/construct.h"
#include "layout_state.h"
#include "render.h"
#include "tab.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/layout.h"
#include "ttx/modifiers.h"
#include "ttx/mouse_event.h"
#include "ttx/paste_event.h"
#include "ttx/terminal_input.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
auto InputThread::create(di::Vector<di::TransparentStringView> command, Key prefix,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread)
    -> di::Result<di::Box<InputThread>> {
    auto result = di::make_box<InputThread>(di::move(command), prefix, layout_state, render_thread);
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.input_thread();
    }));
    return result;
}

InputThread::InputThread(di::Vector<di::TransparentStringView> command, Key prefix,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread)
    : m_command(di::move(command)), m_prefix(prefix), m_layout_state(layout_state), m_render_thread(render_thread) {}

InputThread::~InputThread() {
    request_exit();
    (void) m_thread.join();
}

void InputThread::request_exit() {
    if (!m_done.exchange(true, di::MemoryOrder::Release)) {
        // Ensure the input thread exits. (By requesting device attributes, thus waking up the input thread).
        // It would be better to use something else to cancel the input thread.
        (void) dius::stdin.write_exactly(di::as_bytes("\033[c"_sv.span()));
    }
}

void InputThread::set_input_mode(InputMode mode) {
    if (m_mode == InputMode::Resize && mode == InputMode::Switch) {
        return;
    }
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    m_render_thread.push_event(InputStatus { .mode = mode });
}

void InputThread::input_thread() {
    auto _ = di::ScopeExit([&] {
        m_render_thread.request_exit();
        m_done.exchange(true, di::MemoryOrder::Release);
    });

    auto buffer = di::Vector<byte> {};
    buffer.resize(4096);

    auto parser = TerminalInputParser {};
    auto utf8_decoder = Utf8StreamDecoder {};
    while (!m_done.load(di::MemoryOrder::Acquire)) {
        auto nread = dius::stdin.read_some(buffer.span());
        if (!nread.has_value() || m_done.load(di::MemoryOrder::Acquire)) {
            return;
        }

        auto utf8_string = utf8_decoder.decode(buffer | di::take(*nread));
        auto events = parser.parse(utf8_string);
        for (auto const& event : events) {
            if (m_done.load(di::MemoryOrder::Acquire)) {
                return;
            }

            di::visit(
                [&](auto const& ev) {
                    this->handle_event(ev);
                },
                event);
        }
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void InputThread::handle_event(KeyEvent const& event) {
    if (event.type() == KeyEventType::Press &&
        (event.key() <= Key::ModifiersBegin || event.key() >= Key::ModifiersEnd)) {
        auto reset_mode = di::ScopeExit([&] {
            set_input_mode(InputMode::Insert);
        });

        if (m_mode != InputMode::Normal && event.key() == m_prefix && !!(event.modifiers() & Modifiers::Control)) {
            set_input_mode(InputMode::Normal);
            reset_mode.release();
            return;
        }

        if (m_mode != InputMode::Insert && event.key() == Key::Escape) {
            return;
        }

        auto can_navigate = m_mode != InputMode::Insert;
        if (can_navigate && event.key() == Key::H && !!(event.modifiers() & Modifiers::Control)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                state.active_tab()->navigate(NavigateDirection::Left);
            });
            m_render_thread.request_render();
            set_input_mode(InputMode::Switch);
            reset_mode.release();
            return;
        }

        if (can_navigate && event.key() == Key::L && !!(event.modifiers() & Modifiers::Control)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                state.active_tab()->navigate(NavigateDirection::Right);
            });
            m_render_thread.request_render();
            set_input_mode(InputMode::Switch);
            reset_mode.release();
            return;
        }

        if (can_navigate && event.key() == Key::K && !!(event.modifiers() & Modifiers::Control)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                state.active_tab()->navigate(NavigateDirection::Up);
            });
            m_render_thread.request_render();
            set_input_mode(InputMode::Switch);
            reset_mode.release();
            return;
        }

        if (can_navigate && event.key() == Key::J && !!(event.modifiers() & Modifiers::Control)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                state.active_tab()->navigate(NavigateDirection::Down);
            });
            m_render_thread.request_render();
            set_input_mode(InputMode::Switch);
            reset_mode.release();
            return;
        }

        auto can_resize = m_mode == InputMode::Normal || m_mode == InputMode::Resize;
        if (can_resize &&
            (event.key() == Key::J || event.key() == Key::H || event.key() == Key::L || event.key() == Key::K)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                auto tab = state.active_tab();
                if (!tab) {
                    return;
                }
                auto layout = tab.value().layout_tree();
                if (!layout) {
                    return;
                }
                auto pane = tab.value().active();
                if (!pane) {
                    return;
                }
                auto amount = !!(event.modifiers() & Modifiers::Shift) ? -2 : 2;
                auto direction = [&] {
                    switch (event.key()) {
                        case Key::J:
                            return ResizeDirection::Bottom;
                        case Key::K:
                            return ResizeDirection::Top;
                        case Key::L:
                            return ResizeDirection::Right;
                        case Key::H:
                            return ResizeDirection::Left;
                        default:
                            di::unreachable();
                    }
                }();
                auto need_relayout = tab.value().layout_group().resize(*layout, pane.data(), direction, amount);
                if (need_relayout) {
                    state.layout();
                }
            });
            m_render_thread.request_render();
            set_input_mode(InputMode::Resize);
            reset_mode.release();
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::C) {
            m_layout_state.with_lock([&](LayoutState& state) {
                (void) state.add_tab({ .command = di::clone(m_command) }, m_render_thread);
            });
            m_render_thread.request_render();
            return;
        }

        auto window_nav_keys = di::range(i32(Key::_1), i32(Key::_9) + 1) | di::transform(di::construct<Key>);
        if (m_mode == InputMode::Normal && di::contains(window_nav_keys, event.key())) {
            m_layout_state.with_lock([&](LayoutState& state) {
                auto index = usize(event.key()) - usize(Key::_1);
                if (auto tab = state.tabs().at(index)) {
                    state.set_active_tab(tab.value().get());
                }
            });
            m_render_thread.request_render();
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::D) {
            m_done.store(true, di::MemoryOrder::Release);
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::I && !!(event.modifiers() & Modifiers::Shift)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (auto pane = state.active_pane()) {
                    pane->stop_capture();
                }
            });
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::X) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (auto pane = state.active_pane()) {
                    pane->exit();
                }
            });
            m_render_thread.request_render();
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::BackSlash && !!(Modifiers::Shift)) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                (void) state.add_pane(*state.active_tab(), { .command = di::clone(m_command) }, Direction::Horizontal,
                                      m_render_thread);
            });
            m_render_thread.request_render();
            return;
        }

        if (m_mode == InputMode::Normal && event.key() == Key::Minus) {
            m_layout_state.with_lock([&](LayoutState& state) {
                if (!state.active_tab()) {
                    return;
                }
                (void) state.add_pane(*state.active_tab(), { .command = di::clone(m_command) }, Direction::Vertical,
                                      m_render_thread);
            });
            m_render_thread.request_render();
            return;
        }
    }

    // NOTE: we need to hold the layout state lock the entire time
    // to prevent the Pane object from being prematurely destroyed.
    m_layout_state.with_lock([&](LayoutState& state) {
        if (auto pane = state.active_pane()) {
            pane->event(event);
        }
    });
}

void InputThread::handle_event(MouseEvent const& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (!state.active_tab()) {
            return;
        }
        auto& tab = *state.active_tab();

        // Check if the event interests with any pane.
        for (auto const& entry :
             tab.layout_tree()->hit_test(event.position().in_cells().y(), event.position().in_cells().x())) {
            if (event.type() != MouseEventType::Move) {
                tab.set_active(entry.pane);
            }
            if (entry.pane == tab.active().data()) {
                if (entry.pane->event(event.translate({ -entry.col, -entry.row }, state.size()))) {
                    m_render_thread.request_render();
                }
            }
        }
    });
}

void InputThread::handle_event(FocusEvent const& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (auto pane = state.active_pane()) {
            pane->event(event);
        }
    });
}

void InputThread::handle_event(PasteEvent const& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (auto pane = state.active_pane()) {
            pane->event(event);
        }
    });
}
}
