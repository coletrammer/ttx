#pragma once

#include "di/types/prelude.h"

namespace ttx::terminal {
/// @brief Represents the current cursor position of the terminal.
struct Cursor {
    u32 row { 0 };                   ///< Row (y coordinate)
    u32 col { 0 };                   ///< Column (x coordinate)
    usize text_offset { 0 };         ///< Cached text offset of the cell pointed to by the cursor.
    bool overflow_pending { false }; ///< Signals that the previous text outputted reached the end of a row.

    auto operator==(Cursor const&) const -> bool = default;
};
}
