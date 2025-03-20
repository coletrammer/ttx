#pragma once

#include "layout_state.h"
#include "render.h"
#include "ttx/key_event.h"

namespace ttx {
struct ActionContext {
    KeyEvent const& key_event;
    di::Synchronized<LayoutState>& layout_state;
    RenderThread& render_thread;
    di::Vector<di::TransparentStringView> const& command;
    di::Atomic<bool>& done;
};

struct Action {
    di::String description;
    di::Function<void(ActionContext const&) const&> apply;

    auto to_string() const -> di::String { return di::clone(description); }
};
}
