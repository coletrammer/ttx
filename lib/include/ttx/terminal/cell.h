#pragma once

#include "di/types/integers.h"

namespace ttx::terminal {
/// @brief Represents a on-screen terminal cell
struct Cell {
    u16 graphics_rendition_id { 0 }; ///< 0 means default
    u16 hyperlink_id { 0 };          ///< 0 means none
    u16 multicell_id { 0 };          ///< 0 means none (single cell)
    u16 text_size : 15 { 0 };        ///< The size in bytes of the text in this cell (0 means no text)
    mutable u16 stale : 1 { false }; ///< 1 indicates the cell has ben rendered

    auto is_empty() const {
        return graphics_rendition_id == 0 && hyperlink_id == 0 && multicell_id == 0 && text_size == 0;
    }
};
}
