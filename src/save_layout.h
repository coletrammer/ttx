#pragma once

#include "config.h"
#include "di/container/queue/queue.h"
#include "dius/condition_variable.h"
#include "layout_state.h"

namespace ttx {
struct SaveLayout {
    di::Optional<di::TransparentString> layout_name;
};

struct SaveLayoutExit {};

struct SaveLayoutUpdateConfig {
    SessionConfig config;
};

using SaveLayoutEvent = di::Variant<SaveLayout, SaveLayoutExit, SaveLayoutUpdateConfig>;

class SaveLayoutThread {
public:
    explicit SaveLayoutThread(di::Synchronized<LayoutState>& layout_state, di::Path save_dir, SessionConfig config);
    ~SaveLayoutThread();

    static auto create(di::Synchronized<LayoutState>& layout_state, di::Path save_dir, SessionConfig config)
        -> di::Result<di::Box<SaveLayoutThread>>;
    static auto create_mock(di::Synchronized<LayoutState>& layout_state) -> di::Box<SaveLayoutThread>;

    void push_event(SaveLayoutEvent event);
    void request_save_layout(di::Optional<di::TransparentString> layout_name = {}) {
        push_event(SaveLayout { di::move(layout_name) });
    }
    void set_config(SessionConfig config) { push_event(SaveLayoutUpdateConfig { di::move(config) }); }
    void request_exit() { push_event(SaveLayoutExit {}); }

private:
    void save_layout_thread();
    auto save_layout(di::TransparentStringView layout_name) -> di::Result<>;

    di::Synchronized<di::Queue<SaveLayoutEvent>> m_events;
    dius::ConditionVariable m_condition;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Path m_save_dir;
    SessionConfig m_config;
    dius::Thread m_thread;
};
}
