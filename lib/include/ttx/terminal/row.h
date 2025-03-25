#pragma once

#include "di/container/string/prelude.h"
#include "di/container/vector/prelude.h"
#include "ttx/terminal/cell.h"

namespace ttx::terminal {
/// @brief Represents a on-screen terminal row of cells
struct Row {
    di::Vector<Cell> cells;  ///< Fixed size vector of terminal cells for this row.
    di::String text;         ///< Text associated with the row.
    bool overflow { false }; ///< Use for rewrapping on resize. Set if the cursor overflowed when at this row.
};
}
