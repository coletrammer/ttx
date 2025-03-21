#pragma once

#include "di/reflect/prelude.h"

namespace ttx {
// Represents the direction of splits.
enum class Direction {
    None, // Case when is 0 or 1 children.
    Horizontal,
    Vertical,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Direction>) {
    using enum Direction;
    return di::make_enumerators<"Direction">(di::enumerator<"None", None>, di::enumerator<"Horitzontal", Horizontal>,
                                             di::enumerator<"Vertical", Vertical>);
}
}
