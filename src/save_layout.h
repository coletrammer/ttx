#pragma once

#include "di/container/queue/queue.h"
#include "dius/condition_variable.h"
#include "layout_state.h"

namespace ttx {
struct SaveLayout {};

struct SaveLayoutExit {};

using SaveLayoutEvent = di::Variant<SaveLayout, SaveLayoutExit>;

class SaveLayoutThread {
public:
    explicit SaveLayoutThread(di::Synchronized<LayoutState>& layout_state, di::Path save_path);
    ~SaveLayoutThread();

    static auto create(di::Synchronized<LayoutState>& layout_state, di::Path save_path)
        -> di::Result<di::Box<SaveLayoutThread>>;

    void push_event(SaveLayoutEvent event);
    void request_save_layout() { push_event(SaveLayout {}); }
    void request_exit() { push_event(SaveLayoutExit {}); }

private:
    void save_layout_thread();
    auto save_layout() -> di::Result<>;

    di::Synchronized<di::Queue<SaveLayoutEvent>> m_events;
    dius::ConditionVariable m_condition;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Path m_save_path;
    dius::Thread m_thread;
};
}
