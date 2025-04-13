#include "input.h"

#include "input_mode.h"
#include "key_bind.h"
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
auto InputThread::create(di::Vector<di::TransparentString> command, di::Vector<KeyBind> key_binds,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread)
    -> di::Result<di::Box<InputThread>> {
    auto result = di::make_box<InputThread>(di::move(command), di::move(key_binds), layout_state, render_thread);
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.input_thread();
    }));
    return result;
}

InputThread::InputThread(di::Vector<di::TransparentString> command, di::Vector<KeyBind> key_binds,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread)
    : m_key_binds(di::move(key_binds))
    , m_command(di::move(command))
    , m_layout_state(layout_state)
    , m_render_thread(render_thread) {}

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

void InputThread::handle_event(KeyEvent const& event) {
    for (auto const& bind : m_key_binds) {
        // Ignore key up events and modifier keys when not in insert mode.
        if (m_mode != InputMode::Insert && (event.type() == KeyEventType::Release ||
                                            (event.key() > Key::ModifiersBegin && event.key() < Key::ModifiersEnd))) {
            continue;
        }

        auto modifiers = event.modifiers() & ~(Modifiers::LockModifiers);
        auto key_matches = bind.key == Key::None || (event.type() != KeyEventType::Release && event.key() == bind.key &&
                                                     modifiers == bind.modifiers);
        if (m_mode == bind.mode && key_matches) {
            bind.action.apply({
                .key_event = event,
                .layout_state = m_layout_state,
                .render_thread = m_render_thread,
                .command = m_command,
                .done = m_done,
            });
            set_input_mode(bind.next_mode);
            return;
        }
    }
}

void InputThread::handle_event(MouseEvent const& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (!state.active_tab()) {
            return;
        }
        auto& tab = *state.active_tab();

        auto ev = event.translate({ 0, u32(-!state.hide_status_bar()) }, state.size());

        // Check if we're hitting any popup with the mouse.
        auto row = event.position().in_cells().y();
        auto col = event.position().in_cells().x();
        for (auto entry : tab.popup_layout()) {
            if (row >= entry.row && row < entry.row + entry.size.rows && col >= entry.col &&
                col < entry.col + entry.size.cols) {
                if (ev.type() != MouseEventType::Move) {
                    tab.set_active(entry.pane);
                }
                if (entry.pane->event(ev.translate({ -entry.col, -entry.row }, state.size()))) {
                    m_render_thread.request_render();
                }
                return;
            }
        }

        // Check if the event interests with any pane.
        for (auto const& entry :
             tab.layout_tree()->hit_test(ev.position().in_cells().y(), ev.position().in_cells().x())) {
            if (ev.type() != MouseEventType::Move) {
                // Set the pane the user just clicked on as active.
                tab.set_active(entry.pane);
                // If we had a popup, exit it as the user clicked out.
                for (auto popup_entry : tab.popup_layout()) {
                    popup_entry.pane->exit();
                }
            }
            if (entry.pane == tab.active().data()) {
                if (entry.pane->event(ev.translate({ -entry.col, -entry.row }, state.size()))) {
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
