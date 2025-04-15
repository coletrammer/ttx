#include "ttx/terminal/scroll_back.h"

#include "ttx/terminal/row_group.h"

namespace ttx::terminal {
void ScrollBack::add_rows(RowGroup& from, usize row_index, usize row_count) {
    ASSERT_LT_EQ(row_index + row_count, from.total_rows());

    // Try to insert the last group until its full, and then add new groups. Most of the
    // work is done by the transfer function.
    while (row_count > 0) {
        if (is_last_group_full()) {
            add_group();
        }

        auto& to = m_groups.back().value();
        auto rows_to_take = 0_usize;
        auto cells_to_take = 0_usize;
        while (rows_to_take < row_count && to.cell_count + cells_to_take < cells_per_group) {
            // Strip empty cells from the end of the row.
            auto cells = from.strip_trailing_empty_cells(row_index + rows_to_take);
            cells_to_take += cells;
            rows_to_take++;
        }

        auto cells_taken = to.group.transfer_from(from, row_index, to.group.total_rows(), rows_to_take);

        row_index += rows_to_take;
        row_count -= rows_to_take;
        m_total_rows += rows_to_take;
        to.cell_count += cells_taken;
    }
}

void ScrollBack::take_rows(RowGroup& to, u32 desired_cols, usize row_index, usize row_count) {
    ASSERT_LT_EQ(row_count, total_rows());
    ASSERT_LT_EQ(row_index, to.total_rows());

    // Take from the last group until all rows have been added. The row group transfer function does
    // all the heavy lifting.
    while (row_count > 0) {
        auto& from = m_groups.back().value();
        auto rows_to_take = di::min(row_count, from.group.total_rows());
        auto from_index = from.group.total_rows() - rows_to_take;

        auto cells_taken = to.transfer_from(from.group, from_index, row_index, rows_to_take, desired_cols);

        row_count -= rows_to_take;
        m_total_rows -= rows_to_take;
        from.cell_count -= cells_taken;

        if (from.group.empty()) {
            m_groups.pop_back();
        }
    }
}

auto ScrollBack::find_row(u64 row) const -> di::Tuple<u32, RowGroup const&> {
    ASSERT_GT_EQ(row, absolute_row_start());
    ASSERT_LT(row, absolute_row_end());

    row -= absolute_row_start();

    // TODO: optimize!
    for (auto const& group : m_groups) {
        if (row < group.group.total_rows()) {
            return { u32(row), group.group };
        }
        row -= group.group.total_rows();
    }
    di::unreachable();
}

void ScrollBack::clear() {
    while (!m_groups.empty()) {
        m_absolute_row_start += m_groups.front().value().group.total_rows();
        m_groups.pop_front();
    }
    m_total_rows = 0;
}

auto ScrollBack::is_last_group_full() const -> bool {
    if (m_groups.empty()) {
        return true;
    }
    return m_groups.back().value().cell_count >= cells_per_group;
}

auto ScrollBack::add_group() -> Group& {
    if (m_groups.size() >= max_groups) {
        auto deleted_rows = m_groups.front().value().group.total_rows();
        m_absolute_row_start += deleted_rows;
        m_total_rows -= deleted_rows;
        m_groups.pop_front();
    }
    return m_groups.emplace_back();
}
}
