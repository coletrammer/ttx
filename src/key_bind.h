#pragma once

#include "action.h"
#include "di/reflect/prelude.h"
#include "input_mode.h"
#include "ttx/key.h"
#include "ttx/modifiers.h"

namespace ttx {
struct KeyBind {
    constexpr auto is_default() const -> bool { return key == Key::None; }

    Key key { Key::None };
    Modifiers modifiers { Modifiers::None };
    InputMode mode { InputMode::Normal };      // Mode required for the bind to match.
    InputMode next_mode { InputMode::Insert }; // Mode key binding will transition to next.
    Action action;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<KeyBind>) {
        return di::make_fields<"KeyBind">(di::field<"key", &KeyBind::key>, di::field<"modifiers", &KeyBind::modifiers>,
                                          di::field<"mode", &KeyBind::mode>,
                                          di::field<"next_mode", &KeyBind::next_mode>,
                                          di::field<"action", &KeyBind::action>);
    }
};

auto make_key_binds(Key prefix, bool replay_mode) -> di::Vector<KeyBind>;
}
