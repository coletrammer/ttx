#pragma once

#include "config_json.h"
#include "di/sync/synchronized.h"
#include "layout_state.h"
#include "ttx/key_event.h"

namespace ttx {
struct ActionContext {
    KeyEvent const& key_event;
    di::Synchronized<LayoutState>& layout_state;
    Config const& config;
    config_json::v1::Config const& base_config;
    di::TransparentStringView profile;
    di::Atomic<bool>& done;
};

struct Action {
    di::String description;
    di::Function<void(ActionContext const&) const&> apply;

    auto to_string() const -> di::String { return di::clone(description); }
};
}
