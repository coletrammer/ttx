#pragma once

#include "layout_state.h"
#include "render.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/paste_event.h"

namespace ttx {
class InputThread {
public:
    static auto create(di::Vector<di::TransparentStringView> command, Key prefix,
                       di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread)
        -> di::Result<di::Box<InputThread>>;

    explicit InputThread(di::Vector<di::TransparentStringView> command, Key prefix,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread);
    ~InputThread();

    void request_exit();

private:
    void input_thread();

    void handle_event(KeyEvent const& event);
    void handle_event(MouseEvent const& event);
    void handle_event(FocusEvent const& event);
    void handle_event(PasteEvent const& event);

    di::Vector<di::TransparentStringView> m_command;
    Key m_prefix { Key::B };
    bool m_got_prefix { false };
    di::Atomic<bool> m_done { false };
    di::Synchronized<LayoutState>& m_layout_state;
    RenderThread& m_render_thread;
    dius::Thread m_thread;
};
}
