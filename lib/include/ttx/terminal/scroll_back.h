#pragma once

#include "di/container/ring/prelude.h"
#include "ttx/terminal/row_group.h"

namespace ttx::terminal {
/// @brief Represents the terminal scroll back
///
/// The scroll back is effectively immutable and optimized
/// to minimize the memory needed per cell. Blank cells are
/// not stored but still accessible when iterating over a row.
///
/// The scroll back is unaffected by resize operations (until rewrap
/// is supported), so rows can be over or under sized depending on
/// the actual screen size when the scroll back is being rendered.
///
/// For efficiency, the scroll back is divided into chunks which
/// target a particular number of cells, and represent a collection
/// of visual terminal lines. The memory limit for the scroll back
/// buffer is specified by the total number of cells allowed, which
/// is used to determine the number of chunks.
class ScrollBack {
    // TODO: optimize!
    constexpr static auto cells_per_group = usize(di::NumericLimits<u16>::max / 2);

    // TODO: make this configurable.
    constexpr static auto max_cells = usize(cells_per_group * 100);

    constexpr static auto max_groups = di::divide_round_up(max_cells, cells_per_group);

    struct Group {
        RowGroup group;
        usize cell_count { 0 };
    };

public:
    auto absolute_row_start() const -> u64 { return m_absolute_row_start; }
    auto absolute_row_end() const -> u64 { return m_absolute_row_start + total_rows(); }
    auto total_rows() const -> usize { return m_total_rows; }

    /// @brief Clear the scroll back history
    void clear();

    /// @brief Add rows to the scroll back buffer
    ///
    /// @param from Row group to take from
    /// @param row_index Row index within the group to take from
    /// @param row_count Number of rows to take
    ///
    /// The taken rows will be deleted after this call.
    void add_rows(RowGroup& from, usize row_index, usize row_count);

    /// @brief Remove rows from the scroll back buffer
    ///
    /// @param from Row group to tranfer to
    /// @param desired_cols Number of columns to force in the output
    /// @param row_index Row index within the group to insert
    /// @param row_count Number of rows to take
    ///
    /// This function requires: row_count >= total rows in scroll back.
    ///
    /// The desired_cols value ensures that the inserted rows will have the correct number
    /// of cells per row. The row from scrollback will either be truncated or padded with
    /// empty cells to meet this constraint.
    void take_rows(RowGroup& to, u32 desired_cols, usize row_index, usize row_count);

    auto iterate_row(u64 row) {
        ASSERT_GT_EQ(row, absolute_row_start());
        ASSERT_LT(row, absolute_row_end());

        row -= absolute_row_start();

        // TODO: optimize!
        for (auto& group : m_groups) {
            if (row < group.group.total_rows()) {
                return group.group.iterate_row(row);
            }
            row -= group.group.total_rows();
        }
        di::unreachable();
    }

private:
    auto is_last_group_full() const -> bool;
    auto add_group() -> Group&;

    di::Ring<Group> m_groups;
    usize m_total_rows { 0 };
    u64 m_absolute_row_start { 0 };
};
}
