#pragma once

#include "di/math/numeric_limits.h"
#include "di/types/integers.h"
#include "ttx/graphics_rendition.h"

namespace ttx::terminal {
/// @brief Represents a on-screen terminal cell
struct Cell {
    constexpr static auto max_text_size = (di::NumericLimits<u16>::max >> 5);

    struct Ids {
        u16 graphics_rendition_id { 0 }; ///< 0 means default
        u16 hyperlink_id { 0 };          ///< 0 means none
    };

    union {
        Ids ids {};
        Color background_color;
    };
    u16 multi_cell_id { 0 };                  ///< 0 means none (single cell)
    u16 text_size : 11 { 0 };                 ///< The size in bytes of the text in this cell (0 means no text)
    u16 background_only : 1 { 0 };            ///< 1 indicates this cell only stores a backgroun color
    u16 left_boundary_of_multicell : 1 { 0 }; ///< 1 indicates this cell is in the furthest left column of a multicell.
    u16 explicitly_sized : 1 { 0 };           ///< 1 indicates must be rendered using explicit sizing
    u16 complex_grapheme_cluster : 1 { 0 };   ///< 1 indicates this cell consists of multiple non-zero width code points
    mutable u16 stale : 1 { false };          ///< 1 indicates the cell has ben rendered

    auto is_multi_cell() const -> bool { return multi_cell_id != 0; }
    auto is_primary_in_multi_cell() const -> bool { return is_multi_cell() && left_boundary_of_multicell; }
    auto is_nonprimary_in_multi_cell() const -> bool { return is_multi_cell() && !is_primary_in_multi_cell(); }
    auto has_ids() const -> bool { return !background_only; }

    auto graphics_rendition_id() const -> u16 { return has_ids() ? ids.graphics_rendition_id : 0; }
    auto hyperlink_id() const -> u16 { return has_ids() ? ids.hyperlink_id : 0; }

    auto is_empty() const -> bool {
        return has_ids() && ids.graphics_rendition_id == 0 && ids.hyperlink_id == 0 && multi_cell_id == 0 &&
               text_size == 0;
    }
};
}
