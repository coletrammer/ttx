#pragma once

#include "di/reflect/prelude.h"
#include "input_mode.h"
#include "key_bind.h"
#include "layout_state.h"
#include "save_layout.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/mouse.h"
#include "ttx/pane.h"
#include "ttx/paste_event.h"
#include "ttx/terminal/escapes/device_attributes.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/mode.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace ttx {
class RenderThread;

class InputThread {
public:
    static auto create(CreatePaneArgs create_pane_args, di::Vector<KeyBind> key_binds,
                       di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread,
                       SaveLayoutThread& save_layout_thread) -> di::Result<di::Box<InputThread>>;

    explicit InputThread(CreatePaneArgs create_pane_args, di::Vector<KeyBind> key_binds,
                         di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread,
                         SaveLayoutThread& save_layout_thread);
    ~InputThread();

    void request_exit();

private:
    void input_thread();

    void set_input_mode(InputMode mode);

    void handle_event(KeyEvent const& event);
    void handle_event(MouseEvent const& event);
    void handle_event(FocusEvent const& event);
    void handle_event(PasteEvent const& event);
    void handle_event(terminal::PrimaryDeviceAttributes const&) {}
    void handle_event(terminal::ModeQueryReply const&) {}
    void handle_event(terminal::CursorPositionReport const&) {}
    void handle_event(terminal::KittyKeyReport const&) {}
    void handle_event(terminal::StatusStringResponse const&) {}
    void handle_event(terminal::TerminfoString const&) {}

    auto handle_drag(LayoutState& state, MouseCoordinate const& coordinate) -> bool;

    InputMode m_mode { InputMode::Insert };
    di::Vector<KeyBind> m_key_binds;
    CreatePaneArgs m_create_pane_args;
    di::Atomic<bool> m_done { false };
    di::Optional<MouseCoordinate> m_drag_origin;
    di::Synchronized<LayoutState>& m_layout_state;
    RenderThread& m_render_thread;
    SaveLayoutThread& m_save_layout_thread;
    dius::Thread m_thread;
};
}
