#pragma once

#include "di/types/integers.h"

namespace ttx::terminal {
/// @brief Represents a on-screen terminal cell
struct Cell {
    u16 graphics_rendition_id; ///< 0 means default
    u16 hyperlink_id;          ///< 0 means none
    u16 multicell_id;          ///< 0 means none (single cell)
    u16 text_size : 15;        ///< The size in bytes of the text in this cell (0 means no text)
    u16 dirty : 1;             ///< Bit flag for damage tracking
};
}
