#pragma once

#include "layout_state.h"
#include "render.h"
#include "save_layout.h"
#include "ttx/key_event.h"
#include "ttx/pane.h"

namespace ttx {
struct ActionContext {
    KeyEvent const& key_event;
    di::Synchronized<LayoutState>& layout_state;
    RenderThread& render_thread;
    SaveLayoutThread& save_layout_thread;
    CreatePaneArgs const& create_pane_args;
    di::Atomic<bool>& done;
};

struct Action {
    di::String description;
    di::Function<void(ActionContext const&) const&> apply;

    auto to_string() const -> di::String { return di::clone(description); }
};
}
