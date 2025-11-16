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
        auto prev_row_overflow = m_groups.back().value().group.rows().back().transform(&Row::overflow).value_or(false);
        while (rows_to_take < row_count &&
               to.cell_count + cells_to_take < get_target_cells_per_group(prev_row_overflow)) {
            // Strip empty cells from the end of the row.
            auto cells = from.strip_trailing_empty_cells(row_index + rows_to_take);
            cells_to_take += cells;
            rows_to_take++;

            prev_row_overflow = from.rows()[row_index + rows_to_take].overflow;
        }

        auto cells_taken = to.group.transfer_from(from, row_index, to.group.total_rows(), rows_to_take);

        // NOTE: row index remains unchanged because the "old" rows have now been deleted.
        row_count -= rows_to_take;
        m_total_rows += rows_to_take;
        to.cell_count += cells_taken;
        to.last_reflowed_to = {};
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

auto ScrollBack::take_rows_for_reflow(RowGroup& to) -> u64 {
    auto rows_to_take = 0_u64;

    // Only look in the last row group to prevent excessive computation.
    for (auto& group : m_groups.back()) {
        for (auto& row : group.group.rows() | di::reverse) {
            if (row.overflow) {
                rows_to_take++;
                continue;
            }
            break;
        }

        auto cells_taken = to.transfer_from(group.group, group.group.total_rows() - rows_to_take, 0, rows_to_take);
        m_total_rows -= rows_to_take;
        group.cell_count -= cells_taken;
    }

    return rows_to_take;
}

auto ScrollBack::reflow_visual_rows(u64 absolute_row_start, usize row_count, u32 desired_cols)
    -> di::Optional<ReflowResult> {
    auto result = di::Optional<ReflowResult> {};
    auto visible_rows = 0_usize;
    while (absolute_row_start < absolute_row_end() && visible_rows < row_count) {
        auto [row_offset, row_group_start, group] = find_row_group(absolute_row_start);
        if (group.last_reflowed_to != desired_cols) {
            group.last_reflowed_to = desired_cols;

            m_total_rows -= group.group.total_rows();
            auto reflow_result = group.group.reflow(row_group_start, desired_cols);
            m_total_rows += group.group.total_rows();

            absolute_row_start = reflow_result.map_position({ absolute_row_start, 0 }).row;
            row_offset = reflow_result.map_position({ row_group_start + row_offset, 0 }).row - row_group_start;

            if (result) {
                result.value().merge(di::move(reflow_result));
            } else {
                result = di::move(reflow_result);
            }
        }

        auto visible_rows_in_group = group.group.total_rows() - row_offset;
        absolute_row_start += visible_rows_in_group;
        visible_rows += visible_rows_in_group;
    }
    return result;
}

auto ScrollBack::find_row(u64 row) const -> di::Tuple<u32, RowGroup const&> {
    auto [row_offset, _, group] = const_cast<ScrollBack&>(*this).find_row_group(row);
    return { row_offset, group.group };
}

auto ScrollBack::find_row_group(u64 row) -> di::Tuple<u32, u64, Group&> {
    ASSERT_GT_EQ(row, absolute_row_start());
    ASSERT_LT(row, absolute_row_end());

    auto row_group_start = absolute_row_start();
    row -= absolute_row_start();

    // TODO: optimize!
    for (auto& group : m_groups) {
        if (row < group.group.total_rows()) {
            return { u32(row), row_group_start, group };
        }
        row -= group.group.total_rows();
        row_group_start += group.group.total_rows();
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

    auto const& group = m_groups.back().value();
    auto row_overflow = group.group.rows().back().transform(&Row::overflow).value_or(false);
    return group.cell_count >= get_target_cells_per_group(row_overflow);
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
