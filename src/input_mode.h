#pragma once

#include "di/reflect/prelude.h"

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
}
