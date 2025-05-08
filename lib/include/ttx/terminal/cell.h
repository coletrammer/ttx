#pragma once

#include "di/math/numeric_limits.h"
#include "di/types/integers.h"

namespace ttx::terminal {
/// @brief Represents a on-screen terminal cell
struct Cell {
    constexpr static auto max_text_size = (di::NumericLimits<u16>::max >> 4);

    u16 graphics_rendition_id { 0 };          ///< 0 means default
    u16 hyperlink_id { 0 };                   ///< 0 means none
    u16 multi_cell_id { 0 };                  ///< 0 means none (single cell)
    u16 text_size : 12 { 0 };                 ///< The size in bytes of the text in this cell (0 means no text)
    u16 left_boundary_of_multicell : 1 { 0 }; ///< 1 indicates this cell is in the furthest left column of a multicell.
    u16 top_boundary_of_multicell : 1 { 0 };  ///< 1 indicates this cell is in the furthest top row of a multicell.
    mutable u16 stale : 1 { false };          ///< 1 indicates the cell has ben rendered

    auto is_multi_cell() const { return multi_cell_id != 0; }
    auto is_primary_in_multi_cell() const {
        return is_multi_cell() && left_boundary_of_multicell && top_boundary_of_multicell;
    }
    auto is_nonprimary_in_multi_cell() const { return is_multi_cell() && !is_primary_in_multi_cell(); }

    auto is_empty() const {
        return graphics_rendition_id == 0 && hyperlink_id == 0 && multi_cell_id == 0 && text_size == 0;
    }
};
}
