#pragma once

#include "di/reflect/prelude.h"
#include "di/types/prelude.h"

namespace ttx::terminal {
/// @brief Represents the current cursor position of the terminal.
struct Cursor {
    u32 row { 0 };                   ///< Row (y coordinate)
    u32 col { 0 };                   ///< Column (x coordinate)
    usize text_offset { 0 };         ///< Cached text offset of the cell pointed to by the cursor.
    bool overflow_pending { false }; ///< Signals that the previous text outputted reached the end of a row.

    auto operator==(Cursor const&) const -> bool = default;
    auto operator<=>(Cursor const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Cursor>) {
        return di::make_fields<"Cursor">(di::field<"row", &Cursor::row>, di::field<"col", &Cursor::col>,
                                         di::field<"text_offset", &Cursor::text_offset>,
                                         di::field<"overflow_pending", &Cursor::overflow_pending>);
    }
};
}
