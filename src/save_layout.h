#pragma once

#include "di/container/queue/queue.h"
#include "dius/condition_variable.h"
#include "layout_state.h"

namespace ttx {
struct SaveLayout {
    di::Optional<di::TransparentString> layout_name;
};

struct SaveLayoutExit {};

using SaveLayoutEvent = di::Variant<SaveLayout, SaveLayoutExit>;

class SaveLayoutThread {
public:
    explicit SaveLayoutThread(di::Synchronized<LayoutState>& layout_state, di::Path save_dir,
                              di::Optional<di::TransparentString> layout_name);
    ~SaveLayoutThread();

    static auto create(di::Synchronized<LayoutState>& layout_state, di::Path save_dir,
                       di::Optional<di::TransparentString> layout_name) -> di::Result<di::Box<SaveLayoutThread>>;

    void push_event(SaveLayoutEvent event);
    void request_save_layout(di::Optional<di::TransparentString> layout_name = {}) {
        push_event(SaveLayout { di::move(layout_name) });
    }
    void request_exit() { push_event(SaveLayoutExit {}); }

private:
    void save_layout_thread();
    auto save_layout(di::TransparentStringView layout_name) -> di::Result<>;

    di::Synchronized<di::Queue<SaveLayoutEvent>> m_events;
    dius::ConditionVariable m_condition;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Path m_save_dir;
    di::Optional<di::TransparentString> m_layout_name;
    dius::Thread m_thread;
};
}
