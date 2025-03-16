#pragma once

#include "di/reflect/prelude.h"
#include "layout_state.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/paste_event.h"

namespace ttx {
enum class InputMode {
    Insert, // Default mode - sends keys to active pane.
    Normal, // Normal mode - waiting for key after pressing the prefix key.
    Switch, // Switch mode - only a subset of keys will be handled by ttx, for moving.
    Resize, // Resize mode - only a subset of keys will be handled by ttx, for resizing panes.
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<InputMode>) {
    using enum InputMode;
    return di::make_enumerators<"InputMode">(di::enumerator<"INSERT", Insert>, di::enumerator<"NORMAL", Normal>,
                                             di::enumerator<"SWITCH", Switch>, di::enumerator<"RESIZE", Resize>);
}

class RenderThread;

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

    void set_input_mode(InputMode mode);

    void handle_event(KeyEvent const& event);
    void handle_event(MouseEvent const& event);
    void handle_event(FocusEvent const& event);
    void handle_event(PasteEvent const& event);

    InputMode m_mode { InputMode::Insert };
    di::Vector<di::TransparentStringView> m_command;
    Key m_prefix { Key::B };
    di::Atomic<bool> m_done { false };
    di::Synchronized<LayoutState>& m_layout_state;
    RenderThread& m_render_thread;
    dius::Thread m_thread;
};
}
