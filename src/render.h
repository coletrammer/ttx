#pragma once

#include "di/container/queue/queue.h"
#include "dius/condition_variable.h"
#include "input_mode.h"
#include "layout_state.h"
#include "tab.h"
#include "ttx/clipboard.h"
#include "ttx/features.h"
#include "ttx/pane.h"
#include "ttx/renderer.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace ttx {
struct PaneExited {
    Session* session = nullptr;
    Tab* tab = nullptr;
    Pane* pane = nullptr;
};

struct DoRender {};

struct Exit {};

struct InputStatus {
    InputMode mode { InputMode::Insert };
};

struct WriteString {
    di::String string;
};

struct StatusMessage {
    di::String message;
    dius::SteadyClock::Duration duration {};
};

struct ClipboardRequest {
    terminal::OSC52 osc52;
    di::Optional<Clipboard::Identifier> identifier;
    bool manual { false };
    bool reply { false };
};

using RenderEvent = di::Variant<Size, PaneExited, InputStatus, WriteString, StatusMessage, DoRender, MouseEvent,
                                ClipboardRequest, Exit>;

class RenderThread {
public:
    explicit RenderThread(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit,
                          ClipboardMode clipboard_mode, Feature features);
    ~RenderThread();

    static auto create(di::Synchronized<LayoutState>& layout_state, di::Function<void()> did_exit,
                       ClipboardMode clipboard_mode, Feature features) -> di::Result<di::Box<RenderThread>>;
    static auto create_mock(di::Synchronized<LayoutState>& layout_state) -> RenderThread;

    void push_event(RenderEvent event);
    void request_render() { push_event(DoRender {}); }
    void request_exit() { push_event(Exit {}); }
    void status_message(di::String message, dius::SteadyClock::Duration duration = di::Seconds(1)) {
        push_event(StatusMessage { di::move(message), duration });
    }

private:
    void render_thread();
    void do_render(Renderer& renderer);
    void render_status_bar(LayoutState const& state, Renderer& renderer);

    struct PendingStatusMessage {
        di::String message;
        dius::SteadyClock::TimePoint expiration {};
    };

    struct StatusBarEntry {
        u32 start { 0 };
        u32 width { 0 };
    };

    InputStatus m_input_status;
    di::Optional<PendingStatusMessage> m_pending_status_message;
    di::Vector<StatusBarEntry> m_status_bar_layout;
    di::Synchronized<di::Queue<RenderEvent>> m_events;
    dius::ConditionVariable m_condition;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Function<void()> m_did_exit;
    Clipboard m_clipboard;
    Feature m_features { Feature::None };
    dius::Thread m_thread;
};
}
