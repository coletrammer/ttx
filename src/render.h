#pragma once

#include "di/container/queue/queue.h"
#include "dius/condition_variable.h"
#include "input.h"
#include "layout_state.h"
#include "tab.h"
#include "ttx/pane.h"
#include "ttx/renderer.h"

namespace ttx {
struct PaneExited {
    Tab* tab = nullptr;
    Pane* pane = nullptr;
};

struct DoRender {};

struct Exit {};

struct InputStatus {
    InputMode mode { InputMode::Insert };
};

using RenderEvent = di::Variant<dius::tty::WindowSize, PaneExited, InputStatus, DoRender, Exit>;

class RenderThread {
public:
    explicit RenderThread(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit);
    ~RenderThread();

    static auto create(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit)
        -> di::Result<di::Box<RenderThread>>;

    void push_event(RenderEvent event);
    void request_render() { push_event(DoRender {}); }
    void request_exit() { push_event(Exit {}); }

private:
    void render_thread();
    void do_render(Renderer& renderer);

    InputStatus m_input_status;
    di::Synchronized<di::Queue<RenderEvent>> m_events;
    dius::ConditionVariable m_condition;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Function<void()> m_did_exit;
    dius::Thread m_thread;
};
}
