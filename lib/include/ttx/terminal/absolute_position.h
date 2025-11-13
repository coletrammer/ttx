#pragma once

#include "di/reflect/prelude.h"
#include "di/types/integers.h"

namespace ttx::terminal {
/// @brief Represents a coordinate anywhere in a screen, including scroll back
///
/// The row coordinate if measured in absolute units, and so
/// can refer to rows in the scroll back buffer.
struct AbsolutePosition {
    u64 row { 0 }; ///< The absolute row in a screen.
    u32 col { 0 }; ///< The column in the specified row.

    auto operator==(AbsolutePosition const&) const -> bool = default;
    auto operator<=>(AbsolutePosition const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<AbsolutePosition>) {
        return di::make_fields<"AbsolutePosition">(di::field<"row", &AbsolutePosition::row>,
                                                   di::field<"col", &AbsolutePosition::col>);
    }
};
}
