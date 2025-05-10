#include "ttx/terminal/screen.h"

#include "di/container/algorithm/rotate.h"
#include "di/io/vector_writer.h"
#include "di/util/clamp.h"
#include "di/util/construct.h"
#include "di/util/scope_exit.h"
#include "dius/print.h"
#include "dius/unicode/general_category.h"
#include "dius/unicode/grapheme_cluster.h"
#include "dius/unicode/width.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/cell.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/escapes/osc_8.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/selection.h"

namespace ttx::terminal {
Screen::Screen(Size const& size, ScrollBackEnabled scroll_back_enabled)
    : m_scroll_back_enabled(scroll_back_enabled), m_scroll_region(0, size.rows) {
    resize(size);
}

void Screen::resize(Size const& size) {
    // TODO: rewrap - for now we're truncating

    ASSERT_GT(size.rows, 0);
    ASSERT_GT(size.cols, 0);

    if (size.cols == max_width() && size.rows == max_height()) {
        return;
    }

    // Always update the size and clamp the scroll region in bounds.
    auto _ = di::ScopeExit([&] {
        // If the scroll region is the default (the entire screen, expand it).
        if (m_scroll_region.end_row == m_size.rows) {
            m_scroll_region.end_row = size.rows;
        } else {
            m_scroll_region.end_row = di::min(m_scroll_region.end_row, size.rows);
        }

        m_size = size;
        clamp_selection();
    });

    // First the column size of the existing rows.
    if (size.cols > max_width()) {
        // When expanding, just add blank cells.
        for (auto& row : rows()) {
            row.cells.resize(size.cols);
        }
    } else {
        // When contracting, we need to free resources used by each cell we're deleting.
        for (auto& row : rows()) {
            auto text_bytes_to_delete = 0zu;
            while (row.cells.size() > size.cols) {
                auto cell = *row.cells.pop_back();
                m_active_rows.drop_cell(cell);

                text_bytes_to_delete += cell.text_size;
            }

            auto text_end = row.text.iterator_at_offset(row.text.size_bytes() - text_bytes_to_delete);
            ASSERT(text_end);
            row.text.erase(text_end.value(), row.text.end());
        }
    }

    // Now either add new rows or move existing rows to the scroll back.
    if (rows().size() > size.rows) {
        if (m_scroll_back_enabled == ScrollBackEnabled::Yes && !m_never_got_input) {
            auto was_at_bottom = visual_scroll_at_bottom();
            auto rows_to_delete = rows().size() - size.rows;
            m_scroll_back.add_rows(m_active_rows, 0, rows_to_delete);
            if (was_at_bottom) {
                // Automatically scroll to the bottom if the screen isn't already scrolled.
                m_visual_scroll_offset = absolute_row_screen_start();
            }

            // Adjust the cursor to account for scrolled lines.
            if (m_cursor.row > rows_to_delete) {
                m_cursor.row -= rows_to_delete;
            } else {
                m_cursor.row = 0;
            }
        } else {
            auto rows_to_delete = rows() | di::take(rows().size() - size.rows);
            for (auto& row : rows_to_delete) {
                for (auto& cell : row.cells) {
                    m_active_rows.drop_cell(cell);
                    cell.text_size = 0;
                }
                row.text.clear();
                row.overflow = false;
            }
            rows().erase(rows().begin(), rows().end() - size.rows);
        }
    } else if (rows().size() < size.rows) {
        if (m_scroll_back_enabled == ScrollBackEnabled::Yes && !m_never_got_input) {
            // When taking rows from the scroll back, we also need to move the cursor down to accomdate the new rows.
            auto rows_to_take = di::min(m_scroll_back.total_rows(), size.rows - rows().size());
            m_scroll_back.take_rows(m_active_rows, size.cols, 0, rows_to_take);
            m_cursor.row += rows_to_take;

            // In this case, we may need to adjust the visual scroll offset.
            m_visual_scroll_offset = di::min(m_visual_scroll_offset, absolute_row_screen_start());
        }
        for (auto _ : di::range(size.rows - rows().size())) {
            auto row = Row {};
            row.cells.resize(size.cols);
            rows().push_back(di::move(row));
        }
    }
    ASSERT_EQ(rows().size(), size.rows);

    // Clamp the cursor to the screen size.
    m_cursor.row = di::min(m_cursor.row, size.rows - 1);
    m_cursor.col = di::min(m_cursor.col, size.cols - 1);

    // Recompute the cursor text offset.
    auto& row_object = rows()[m_cursor.row];
    m_cursor.text_offset = 0;
    for (auto const& cell : row_object.cells | di::take(m_cursor.col)) {
        m_cursor.text_offset += cell.text_size;
    }

    // TODO: optimize!
    invalidate_all();
}

void Screen::set_scroll_region(ScrollRegion const& region) {
    ASSERT_LT(region.start_row, region.end_row);
    ASSERT_LT_EQ(region.end_row, max_height());
    m_scroll_region = region;
}

auto Screen::current_graphics_rendition() const -> GraphicsRendition const& {
    return m_active_rows.graphics_rendition(m_graphics_id);
}

auto Screen::current_hyperlink() const -> di::Optional<Hyperlink const&> {
    return m_active_rows.maybe_hyperlink(m_hyperlink_id);
}

void Screen::set_current_graphics_rendition(GraphicsRendition const& rendition) {
    if (rendition == GraphicsRendition {}) {
        m_active_rows.drop_graphics_id(m_graphics_id);
        m_graphics_id = 0;
        return;
    }

    auto existing_id = m_active_rows.graphics_id(rendition);
    if (existing_id) {
        if (*existing_id == m_graphics_id) {
            return;
        }
        m_active_rows.drop_graphics_id(m_graphics_id);
        m_graphics_id = m_active_rows.use_graphics_id(*existing_id);
        return;
    }

    m_active_rows.drop_graphics_id(m_graphics_id);

    auto new_id = m_active_rows.allocate_graphics_id(rendition);
    if (!new_id) {
        m_graphics_id = 0;
        return;
    }

    m_graphics_id = *new_id;
}

void Screen::set_current_hyperlink(di::Optional<Hyperlink const&> hyperlink) {
    if (!hyperlink) {
        m_active_rows.drop_hyperlink_id(m_hyperlink_id);
        m_hyperlink_id = 0;
        return;
    }

    auto existing_id = m_active_rows.hyperlink_id(hyperlink.value().id);
    if (existing_id) {
        if (*existing_id == m_hyperlink_id) {
            return;
        }
        m_active_rows.drop_hyperlink_id(m_hyperlink_id);
        m_hyperlink_id = m_active_rows.use_hyperlink_id(*existing_id);
        return;
    }

    m_active_rows.drop_hyperlink_id(m_hyperlink_id);

    auto new_id = m_active_rows.allocate_hyperlink_id(hyperlink.value().clone());
    if (!new_id) {
        m_hyperlink_id = 0;
        return;
    }

    m_hyperlink_id = *new_id;
}

auto Screen::save_cursor() const -> SavedCursor {
    return {
        .row = m_cursor.row,
        .col = m_cursor.col,
        .overflow_pending = m_cursor.overflow_pending,
        .graphics_rendition = current_graphics_rendition(),
        .origin_mode = m_origin_mode,
    };
}

void Screen::restore_cursor(SavedCursor const& cursor) {
    // Restore origin mode first to ensure we clamp the cursor if necessary.
    m_origin_mode = cursor.origin_mode;
    set_cursor(cursor.row, cursor.col);
    set_current_graphics_rendition(cursor.graphics_rendition);

    // This is restored even if the terminal has been resized such that
    // the cursor is no longer at the end of the row.
    m_cursor.overflow_pending = cursor.overflow_pending;
}

void Screen::set_origin_mode(OriginMode origin_mode) {
    if (m_origin_mode == origin_mode) {
        return;
    }
    m_origin_mode = origin_mode;

    // If origin mode is enabled, this puts the cursor at the
    // top-left of the scroll region.
    set_cursor(0, 0);
}

void Screen::set_cursor_relative(u32 row, u32 col) {
    set_cursor(translate_row(row), translate_col(col));
}

void Screen::set_cursor(u32 row, u32 col, bool overflow_pending) {
    set_cursor(row, col);
    m_cursor.overflow_pending = overflow_pending;
}

void Screen::set_cursor(u32 row, u32 col) {
    // Setting the cursor always clears the overflow pending flag.
    m_cursor.overflow_pending = false;

    row = di::clamp(row, min_row(), max_row_inclusive());
    col = di::clamp(col, min_col(), max_col_inclusive());

    // Special case when moving the cursor on the same row.
    if (m_cursor.row == row) {
        set_cursor_col(col);
        return;
    }

    m_cursor.row = row;
    m_cursor.col = col;
    m_cursor.text_offset = 0;

    // Compute the text offset from the beginning of the row.
    auto& row_object = rows()[row];
    for (auto const& cell : row_object.cells | di::take(col)) {
        m_cursor.text_offset += cell.text_size;
    }
}

void Screen::set_cursor_row_relative(u32 row) {
    set_cursor_row(translate_row(row));
}

void Screen::set_cursor_row(u32 row) {
    set_cursor(row, m_cursor.col);
}

void Screen::set_cursor_col_relative(u32 col) {
    set_cursor_col(translate_col(col));
}

void Screen::set_cursor_col(u32 col) {
    ASSERT_LT(m_cursor.row, max_height());

    // Setting the cursor always clears the overflow pending flag.
    m_cursor.overflow_pending = false;

    col = di::clamp(translate_col(col), min_col(), max_col_inclusive());
    if (m_cursor.col == col) {
        return;
    }

    // Special case when moving to column 0.
    if (col == 0) {
        m_cursor.text_offset = 0;
        m_cursor.col = 0;
        return;
    }

    // Compute the text offset relative the current cursor.
    auto& row = rows()[m_cursor.row];
    if (m_cursor.col < col) {
        for (auto const& cell : row.cells | di::drop(m_cursor.col) | di::take(col - m_cursor.col)) {
            m_cursor.text_offset += cell.text_size;
        }
    } else {
        for (auto const& cell : row.cells | di::drop(col) | di::take(m_cursor.col - col)) {
            m_cursor.text_offset -= cell.text_size;
        }
    }
    m_cursor.col = col;
}

void Screen::insert_blank_characters(u32 count) {
    m_cursor.overflow_pending = false;

    // TODO: horizontal scrolling region

    // Check if we're inserting into a multicell boundary. If so, we need to clear the
    // multicell.
    auto& row = rows()[m_cursor.row];
    if (row.cells[m_cursor.col].is_nonprimary_in_multi_cell()) {
        auto erase_position = m_cursor.col;
        while (erase_position > 0 && row.cells[erase_position].is_nonprimary_in_multi_cell()) {
            m_active_rows.drop_cell(row.cells[erase_position]);
            erase_position--;
        }

        auto& primary_cell = row.cells[erase_position];
        m_active_rows.drop_cell(primary_cell);

        auto erase_text_start = row.text.iterator_at_offset(m_cursor.text_offset - primary_cell.text_size);
        ASSERT(erase_text_start);
        auto erase_text_end = row.text.iterator_at_offset(m_cursor.text_offset);
        ASSERT(erase_text_end);
        row.text.erase(erase_text_start.value(), erase_text_end.value());

        m_cursor.text_offset -= primary_cell.text_size;
        primary_cell.text_size = 0;
    }

    // Drop the correct number of cells, based on how many new ones we need
    // to insert.
    auto max_to_insert = di::min(count, max_width() - m_cursor.col);
    auto deletion_point = max_width() - max_to_insert;
    if (deletion_point < row.cells.size() && row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (deletion_point > 0 && !row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = row.text.size_bytes();
    for (auto& cell : auto(*row.cells.subspan(deletion_point))) {
        m_active_rows.drop_cell(cell);
        text_start_position -= cell.text_size;
        cell.text_size = 0;
    }
    row.cells.erase(row.cells.end() - max_to_insert, row.cells.end());

    // Erase the text of the deleted cells.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    ASSERT(text_start);
    row.text.erase(text_start.value(), row.text.end());

    // Mark any cells which have moved as dirty.
    for (auto& cell : row.cells | di::drop(m_cursor.col)) {
        cell.stale = false;
    }

    // Finally, insert the blank cells. Note that to implement bce this would need to
    // preserve the background color. The cursor position is unchanged, as is
    // the cursor byte offset.
    row.cells.insert_container(row.cells.begin() + m_cursor.col, di::repeat(Cell(), max_to_insert));
    row.overflow = false;
}

void Screen::insert_blank_lines(u32 count) {
    m_cursor.overflow_pending = false;
    if (!cursor_in_scroll_region()) {
        return;
    }

    // Start by dropping the correct number of rows. We're clearing rows at the end
    // of the screen. To implement bce we'd need to copy the background color when clearing.
    auto max_to_insert = di::min(count, m_scroll_region.end_row - m_cursor.row);
    auto delete_row_it = end_row_iterator() - max_to_insert;
    for (auto it = delete_row_it; it != end_row_iterator(); ++it) {
        auto& row = *it;
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }

    // Now rotate the blank lines into place.
    di::rotate(rows().begin() + m_cursor.row, delete_row_it, end_row_iterator());

    // Now set the cursor to the left margin.
    m_cursor.text_offset = 0;
    m_cursor.col = 0;

    // TODO: optimize!
    invalidate_all();
}

void Screen::delete_characters(u32 count) {
    m_cursor.overflow_pending = false;

    // Start by dropping the correct number of cells, based on how many new ones we need
    // to insert.
    auto& row = rows()[m_cursor.row];
    auto max_to_delete = di::min(count, max_width() - m_cursor.col);
    auto deletion_point = m_cursor.col;
    if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = m_cursor.text_offset;
    if (deletion_point < m_cursor.col) {
        for (auto& cell : auto(*row.cells.subspan(deletion_point, m_cursor.col - deletion_point))) {
            text_start_position -= cell.text_size;
        }
    }
    auto deletion_end = m_cursor.col + max_to_delete;
    while (deletion_end < max_width() && row.cells[deletion_end].is_nonprimary_in_multi_cell()) {
        deletion_end++;
    }
    auto text_end_position = text_start_position;
    for (auto& cell : auto(*row.cells.subspan(deletion_point, deletion_end - deletion_point))) {
        m_active_rows.drop_cell(cell);
        text_end_position += cell.text_size;
        cell.text_size = 0;
    }
    row.cells.erase(row.cells.begin() + m_cursor.col, row.cells.begin() + m_cursor.col + max_to_delete);

    // Erase the text of the deleted cells.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    auto text_end = row.text.iterator_at_offset(text_end_position);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());

    // Mark any cells which have moved as dirty.
    for (auto& cell : row.cells | di::drop(m_cursor.col)) {
        cell.stale = false;
    }

    // Insert blank cells at the end of the row. Note that to implement bce this would need to
    // preserve the background color. The cursor position is unchanged, as is the cursor byte offset.
    row.cells.resize(max_width());
    row.overflow = false;

    m_cursor.text_offset = text_start_position;
}

void Screen::delete_lines(u32 count) {
    m_cursor.overflow_pending = false;
    if (!cursor_in_scroll_region()) {
        return;
    }

    // Start by dropping the correct number of rows. We're clearing starting from
    // the cursor row. If we supported bce we'd need to copy the
    // background color.

    auto max_to_delete = di::min(count, m_scroll_region.end_row - m_cursor.row);
    auto delete_row_it = rows().begin() + m_cursor.row;
    auto delete_row_end = di::next(delete_row_it, max_to_delete, rows().end());
    for (auto it = delete_row_it; it != delete_row_end; ++it) {
        auto& row = *it;
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }

    // Rotate the blank lines into place.
    di::rotate(delete_row_it, delete_row_end, end_row_iterator());

    // Now set the cursor to the left margin.
    m_cursor.text_offset = 0;
    m_cursor.col = 0;

    // TODO: optimize!
    invalidate_all();
}

void Screen::clear() {
    m_cursor.overflow_pending = false;

    // NOTE: to properly implement "bce", we'd have to actually
    // apply the current background color to all cells. For now, just
    // delete everything. We may choose to never support bce properly
    // like kitty, since it allows us to optimize clearing by fully
    // deleting all cells.

    for (auto& row : rows()) {
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }

    // After clearing, there is no text.
    m_cursor.text_offset = 0;
}

void Screen::clear_after_cursor() {
    // First, clear the current cursor row.
    clear_row_after_cursor();
    ASSERT(!m_cursor.overflow_pending);

    // Now we just need to delete all lines below the cursor. As above, implementing
    // "bce" would require more work.
    for (auto& row : rows() | di::drop(m_cursor.row + 1)) {
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }
}

void Screen::clear_before_cursor() {
    // First, clear the current cursor row.
    clear_row_before_cursor();
    ASSERT(!m_cursor.overflow_pending);

    if (m_cursor.row == 0) {
        return;
    }

    // Now delete all lines before the cursor. As above, implementing
    // "bce" would require more work. However, we don't actually delete
    // the rows here because rows are always assumed to start at the top
    // of the screen.
    for (auto& row : rows() | di::take(m_cursor.row)) {
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }
}

void Screen::clear_row() {
    m_cursor.overflow_pending = false;

    auto& row = rows()[m_cursor.row];
    for (auto& cell : row.cells) {
        m_active_rows.drop_cell(cell);
        cell.text_size = 0;
    }
    row.text.clear();
    row.overflow = false;

    // We deleted all the text on the cursor's row.
    m_cursor.text_offset = 0;
}

void Screen::clear_row_after_cursor() {
    m_cursor.overflow_pending = false;

    auto& row = rows()[m_cursor.row];
    auto text_size_to_delete = 0zu;
    auto deletion_point = m_cursor.col;
    if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = m_cursor.text_offset;
    if (deletion_point < m_cursor.col) {
        for (auto& cell : auto(*row.cells.subspan(deletion_point, m_cursor.col - deletion_point))) {
            text_start_position -= cell.text_size;
        }
    }
    for (auto& cell : row.cells | di::drop(deletion_point)) {
        m_active_rows.drop_cell(cell);
        text_size_to_delete += cell.text_size;
        cell.text_size = 0;
    }
    row.overflow = false;

    // Delete text after the cursor.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    auto text_end = row.text.iterator_at_offset(text_start_position + text_size_to_delete);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());

    m_cursor.text_offset = text_start_position;
}

void Screen::clear_row_before_cursor() {
    m_cursor.overflow_pending = false;

    auto& row = rows()[m_cursor.row];
    auto deletion_end = m_cursor.col + 1;
    while (deletion_end < row.cells.size() && row.cells[deletion_end].is_nonprimary_in_multi_cell()) {
        deletion_end++;
    }
    auto text_size_to_delete = 0zu;
    for (auto& cell : row.cells | di::take(deletion_end)) {
        m_active_rows.drop_cell(cell);
        text_size_to_delete += cell.text_size;
        cell.text_size = 0;
    }

    // Delete all text before the cursor.
    auto text_end = row.text.iterator_at_offset(text_size_to_delete);
    ASSERT(text_end);
    row.text.erase(row.text.begin(), text_end.value());

    // We deleted all the text before the cursor.
    m_cursor.text_offset = 0;
}

void Screen::erase_characters(u32 n) {
    m_cursor.overflow_pending = false;

    auto& row = rows()[m_cursor.row];
    auto insertion_point = m_cursor.col;
    auto deletion_point = insertion_point;
    if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = m_cursor.text_offset;
    if (deletion_point < m_cursor.col) {
        for (auto& cell : auto(*row.cells.subspan(deletion_point, m_cursor.col - deletion_point))) {
            text_start_position -= cell.text_size;
        }
    }
    auto deletion_end = di::min(insertion_point + n, u32(row.cells.size()));
    while (deletion_end < max_width() && row.cells[deletion_end].is_nonprimary_in_multi_cell()) {
        deletion_end++;
    }
    auto text_end_position = text_start_position;
    for (auto& cell : auto(*row.cells.subspan(deletion_point, deletion_end - deletion_point))) {
        m_active_rows.drop_cell(cell);
        text_end_position += cell.text_size;
        cell.text_size = 0;
    }

    // Delete text after the cursor.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    auto text_end = row.text.iterator_at_offset(text_end_position);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());

    // Clear row overflow flag if text after the cursor is fully deleted.
    if (text_end.value() == row.text.end()) {
        row.overflow = false;
    }

    m_cursor.text_offset = text_start_position;
}

void Screen::scroll_down() {
    ASSERT_EQ(m_cursor.row + 1, m_scroll_region.end_row);

    // Clear the first row, optionally putting it into the scroll back buffer.
    if (m_scroll_back_enabled == ScrollBackEnabled::Yes) {
        auto was_at_bottom = visual_scroll_at_bottom();
        m_scroll_back.add_rows(m_active_rows, m_scroll_region.start_row, 1);

        // Insert the new row.
        auto row = m_active_rows.rows().emplace(m_active_rows.rows().iterator(m_scroll_region.end_row - 1));
        row->cells.resize(max_width());

        if (was_at_bottom) {
            // Automatically scroll to the bottom if the screen isn't already scrolled.
            m_visual_scroll_offset = absolute_row_screen_start();
        } else if (m_visual_scroll_offset < absolute_row_start()) {
            // Adjust the visual scroll offset to be in bounds, if needed.
            m_visual_scroll_offset = absolute_row_start();
        }
        clamp_selection();
    } else {
        auto& row = *begin_row_iterator();
        for (auto& cell : row.cells) {
            m_active_rows.drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;

        // Rotate rows into place.
        di::rotate(begin_row_iterator(), begin_row_iterator() + 1, end_row_iterator());
    }

    m_cursor.text_offset = 0;
    m_cursor.overflow_pending = false;
    invalidate_all();
}

void Screen::put_code_point(c32 code_point, AutoWrapMode auto_wrap_mode) {
    // 0. Update flag, which is is used when resizing to prevent committing blank lines to the scroll back buffer
    //    in cases where the terminal's initial size is changed before we get any input.
    m_never_got_input = false;

    // 1. Measure the width of the code point.
    auto width = dius::unicode::code_point_width(code_point).value_or(0);

    // 2. Determine the previous cell.
    auto prev_cell = di::Optional<di::Tuple<Row&, Cell&, usize>> {};
    if (m_cursor.row > 0 && m_cursor.col == 0) {
        auto& row = rows()[m_cursor.row - 1];
        auto candidate_position = max_width() - 1;
        while (candidate_position > 0 && row.cells[candidate_position].is_nonprimary_in_multi_cell()) {
            candidate_position--;
        }
        auto& cell = row.cells[candidate_position];
        prev_cell = { row, cell, row.text.size_bytes() - cell.text_size };
    } else if (m_cursor.col > 0) {
        auto& row = rows()[m_cursor.row];
        auto candidate_position = m_cursor.col - 1;
        while (candidate_position > 0 && row.cells[candidate_position].is_nonprimary_in_multi_cell()) {
            candidate_position--;
        }
        auto& cell = row.cells[candidate_position];
        prev_cell = { row, cell, m_cursor.text_offset - cell.text_size };
    }

    // 3. Combining path - width is 0, so potentially add the
    //    code point to the previous cell. In certain cases,
    //    the combining character will be ignored.
    if (width == 0) {
        // Ignore code point if there is no previous cell or the
        // previous cell has no text.
        if (!prev_cell) {
            return;
        }

        // Now, we're safe to add the 0 width character to the cell's text.
        auto [row, primary_cell, text_offset] = prev_cell.value();
        auto it = row.text.iterator_at_offset(text_offset + primary_cell.text_size);
        ASSERT(it);
        auto [s, e] = row.text.insert(it.value(), code_point);
        auto byte_size = e.data() - s.data();
        if (m_cursor.col > 0) {
            m_cursor.text_offset += byte_size;
        }
        primary_cell.text_size += byte_size;
        primary_cell.stale = false;
        return;
    }

    // 4. Perform grapheme clustering w.r.t to the previous cell. If this character is not a grapheme boundary,
    //    add to the previous cell.
    if (prev_cell) {
        auto clusterer = dius::unicode::GraphemeClusterer {};
        auto [row, primary_cell, text_offset] = prev_cell.value();
        auto text_start = row.text.iterator_at_offset(text_offset);
        auto text_end = row.text.iterator_at_offset(text_offset + primary_cell.text_size);
        ASSERT(text_start);
        ASSERT(text_end);
        auto text = row.text.substr(text_start.value(), text_end.value());
        for (auto ch : text) {
            clusterer.is_boundary(ch);
        }
        // Check for the combining case.
        if (!clusterer.is_boundary(code_point)) {
            auto [s, e] = row.text.insert(text_end.value(), code_point);
            auto byte_size = e.data() - s.data();
            if (m_cursor.col > 0) {
                m_cursor.text_offset += byte_size;
            }
            primary_cell.text_size += byte_size;
            primary_cell.stale = false;
            return;
        }
    }

    // 5. Happy path - width is 1, so add a single cell.
    if (width == 1) {
        auto code_units = di::encoding::convert_to_code_units(di::String::Encoding(), code_point);
        auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
        put_single_cell(view, auto_wrap_mode);
        return;
    }

    // 6. Multi-cell case - width is 2.
    ASSERT_EQ(width, 2);
    auto code_units = di::encoding::convert_to_code_units(di::String::Encoding(), code_point);
    auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
    put_wide_cell(view, 2, auto_wrap_mode);
}

void Screen::put_single_cell(di::StringView text, AutoWrapMode auto_wrap_mode) {
    // Sanity check - if the text size is too large, ignore.
    if (text.size_bytes() > Cell::max_text_size) {
        return;
    }

    // Fetch and validate the row from the cursor position.
    auto& row = rows()[m_cursor.row];
    ASSERT_LT(m_cursor.col, max_width());
    ASSERT_EQ(row.cells.size(), max_width());

    // Check for the overflow condition, which will evenually induce scrolling.
    if (auto_wrap_mode == AutoWrapMode::Enabled && m_cursor.overflow_pending) {
        // Mark the current row as having overflowed, and then advance the cursor.
        row.overflow = true;

        auto new_cursor = Cursor { m_cursor.row + 1, 0, 0, false };
        if (m_cursor.row + 1 == m_scroll_region.end_row) {
            // This was the last line - so induce scrolling by adding a new row.
            scroll_down();
            m_cursor.col = 0;
        } else {
            m_cursor = new_cursor;
        }
        put_single_cell(text, auto_wrap_mode);
        return;
    }

    // We have to clear text starting from the insertion point. However, we have to extend the
    // deletion of cells to account for a potential multi cell already partially occuppying the
    // new multi cell.
    auto insertion_point = m_cursor.col;
    auto deletion_point = insertion_point;
    if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = m_cursor.text_offset;
    if (deletion_point < m_cursor.col) {
        for (auto& cell : auto(*row.cells.subspan(deletion_point, m_cursor.col - deletion_point))) {
            text_start_position -= cell.text_size;
        }
    }
    auto deletion_end = insertion_point + 1;
    while (deletion_end < max_width() && row.cells[deletion_end].is_nonprimary_in_multi_cell()) {
        deletion_end++;
    }
    auto text_end_position = text_start_position;
    for (auto& cell : auto(*row.cells.subspan(deletion_point, deletion_end - deletion_point))) {
        m_active_rows.drop_cell(cell);
        text_end_position += cell.text_size;
        cell.text_size = 0;
    }

    // Modify the cell with the new attributes, starting by clearing the old attributes.
    auto& cell = row.cells[m_cursor.col];
    if (m_graphics_id) {
        cell.graphics_rendition_id = m_active_rows.use_graphics_id(m_graphics_id);
    }
    if (m_hyperlink_id) {
        cell.hyperlink_id = m_active_rows.use_hyperlink_id(m_hyperlink_id);
    }

    // Insert the text. We can safely assert the iterator is valid because we compute the text offset in all cases
    // and it should be correct. Additionally, because each cell contains valid UTF-8 text, the cell boundaries
    // will always be valid insertion points.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    auto text_end = row.text.iterator_at_offset(text_end_position);
    ASSERT(text_start.has_value());
    ASSERT(text_end.has_value());
    row.text.replace(text_start.value(), text_end.value(), text);
    cell.text_size = text.size_bytes();

    // Advance the cursor 1 cell.
    auto new_cursor = m_cursor;
    if (new_cursor.col + 1 == max_width()) {
        new_cursor.overflow_pending = true;
    } else {
        new_cursor.col++;
        new_cursor.text_offset += text.size_bytes();
    }
    m_cursor = new_cursor;
}

void Screen::put_wide_cell(di::StringView text, u8 width, AutoWrapMode auto_wrap_mode) {
    ASSERT_GT_EQ(width, 2u);

    // Sanity check - if the text size is too large, ignore.
    if (text.size_bytes() > Cell::max_text_size) {
        return;
    }

    // Sanity check - if the width is too large, ignore.
    if (width > max_width()) {
        return;
    }

    // Fetch and validate the row from the cursor position.
    auto& row = rows()[m_cursor.row];
    ASSERT_LT(m_cursor.col, max_width());
    ASSERT_EQ(row.cells.size(), max_width());

    // Check for the overflow condition, which will evenually induce scrolling.
    if (auto_wrap_mode == AutoWrapMode::Enabled && m_cursor.col + width > max_width()) {
        // Mark the current row as having overflowed, and then advance the cursor.
        row.overflow = true;

        auto new_cursor = Cursor { m_cursor.row + 1, 0, 0, false };
        if (m_cursor.row + 1 == m_scroll_region.end_row) {
            // This was the last line - so induce scrolling by adding a new row.
            scroll_down();
            m_cursor.col = 0;
        } else {
            m_cursor = new_cursor;
        }
        put_wide_cell(text, width, auto_wrap_mode);
        return;
    }

    // Try to the allocate the multi cell info. This is required, and so we bail if
    // this fails.
    auto multi_cell_info = MultiCellInfo { .width = width };
    auto multi_cell_id = m_active_rows.maybe_allocate_multi_cell_id(multi_cell_info);
    if (!multi_cell_id.has_value()) {
        return;
    }

    // Determine the insertion point. This may not be the cursor column when we're at the
    // end of the line.
    auto insertion_point = di::min(m_cursor.col, max_width() - width);

    // We have to clear text starting from the insertion point. However, we have to extend the
    // deletion of cells to account for a potential multi cell already partially occuppying the
    // new multi cell.
    auto deletion_point = insertion_point;
    if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
        while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
            deletion_point--;
        }
    }
    auto text_start_position = m_cursor.text_offset;
    if (deletion_point < m_cursor.col) {
        for (auto& cell : auto(*row.cells.subspan(deletion_point, m_cursor.col - deletion_point))) {
            text_start_position -= cell.text_size;
        }
    }
    auto deletion_end = insertion_point + width;
    while (deletion_end < max_width() && row.cells[deletion_end].is_nonprimary_in_multi_cell()) {
        deletion_end++;
    }
    auto text_end_position = text_start_position;
    for (auto& cell : auto(*row.cells.subspan(deletion_point, deletion_end - deletion_point))) {
        m_active_rows.drop_cell(cell);
        text_end_position += cell.text_size;
        cell.text_size = 0;
    }

    // Now that we've cleared any old text, set the attributes appropriately on the new cells.
    auto& primary_cell = row.cells[insertion_point];
    primary_cell.left_boundary_of_multicell = true;
    primary_cell.top_boundary_of_multicell = true;
    primary_cell.multi_cell_id = multi_cell_id.value();

    for (auto& cell : auto(*row.cells.subspan(insertion_point, width))) {
        if (m_graphics_id) {
            cell.graphics_rendition_id = m_active_rows.use_graphics_id(m_graphics_id);
        }
        if (m_hyperlink_id) {
            cell.hyperlink_id = m_active_rows.use_hyperlink_id(m_hyperlink_id);
        }
        // Avoid adding an additional reference on the primary cell. Allocating the id
        // already takes a reference, so we transfer that one directly to the primary
        // cell.
        if (!cell.multi_cell_id) {
            cell.multi_cell_id = m_active_rows.use_multi_cell_id(multi_cell_id.value());
        }
    }

    // Insert the text. We can safely assert the iterator is valid because we compute the text offset in all cases
    // and it should be correct. Additionally, because each cell contains valid UTF-8 text, the cell boundaries
    // will always be valid insertion points.
    auto text_start = row.text.iterator_at_offset(text_start_position);
    auto text_end = row.text.iterator_at_offset(text_end_position);
    ASSERT(text_start.has_value());
    ASSERT(text_end.has_value());
    row.text.replace(text_start.value(), text_end.value(), text);
    primary_cell.text_size = text.size_bytes();

    // Advance the cursor 1 cell.
    auto new_cursor = m_cursor;
    new_cursor.col += width;
    new_cursor.text_offset = text_start_position + text.size_bytes();
    if (new_cursor.col >= max_width()) {
        new_cursor.col = max_col_inclusive();
        new_cursor.overflow_pending = true;
    }
    m_cursor = new_cursor;
}

auto Screen::translate_row(u32 row) const -> u32 {
    if (m_origin_mode == OriginMode::Enabled) {
        return row + m_scroll_region.start_row;
    }
    return row;
}

auto Screen::translate_col(u32 col) const -> u32 {
    return col;
}

auto Screen::min_row() const -> u32 {
    if (m_origin_mode == OriginMode::Enabled) {
        return m_scroll_region.start_row;
    }
    return 0;
}

auto Screen::max_row_inclusive() const -> u32 {
    if (m_origin_mode == OriginMode::Enabled) {
        return m_scroll_region.end_row - 1;
    }
    return max_height() - 1;
}

auto Screen::min_col() const -> u32 {
    return 0;
}

auto Screen::max_col_inclusive() const -> u32 {
    return max_width() - 1;
}

auto Screen::cursor_in_scroll_region() const -> bool {
    return m_cursor.row >= m_scroll_region.start_row && m_cursor.row < m_scroll_region.end_row;
}

void Screen::clear_scroll_back() {
    visual_scroll_to_bottom();
    m_scroll_back.clear();
}

void Screen::visual_scroll_up() {
    if (visual_scroll_offset() > absolute_row_start()) {
        m_visual_scroll_offset--;
        invalidate_all();
    }
}

void Screen::visual_scroll_down() {
    if (visual_scroll_offset() < absolute_row_screen_start()) {
        m_visual_scroll_offset++;
        invalidate_all();
    }
}

void Screen::visual_scroll_to_bottom() {
    if (m_visual_scroll_offset != absolute_row_screen_start()) {
        m_visual_scroll_offset = absolute_row_screen_start();
        invalidate_all();
    }
}

void Screen::clear_selection() {
    if (!m_selection.value_or({}).empty()) {
        invalidate_all();
    }
    m_selection = {};
}

void Screen::begin_selection(SelectionPoint const& point, BeginSelectionMode mode) {
    switch (mode) {
        case BeginSelectionMode::Empty:
            m_selection = { point, point };
            return;
        case BeginSelectionMode::Word: {
            // For word selection, consider all non whitespace characters
            // as part of a word. This algorithm should work well enough
            // while still being simple to implement.
            auto text_per_cell = di::Vector<di::StringView> {};
            for (auto row : iterate_row(point.row)) {
                auto [_, _, text, _, _] = row;
                text_per_cell.push_back(text);
            }
            auto start = point.col;
            auto end = point.col;
            while (start > 0) {
                auto text = text_per_cell[start - 1];
                if (text.empty() ||
                    dius::unicode::general_category(*text.front()) == dius::unicode::GeneralCategory::SpaceSeparator) {
                    break;
                }
                start--;
            }
            while (end < max_col_inclusive()) {
                auto text = text_per_cell[end + 1];
                if (text.empty() ||
                    dius::unicode::general_category(*text.front()) == dius::unicode::GeneralCategory::SpaceSeparator) {
                    break;
                }
                end++;
            }
            m_selection = { SelectionPoint { point.row, start }, SelectionPoint { point.row, end } };
            invalidate_all();
            return;
        }
        case BeginSelectionMode::Line:
            m_selection = { SelectionPoint { point.row, 0 }, SelectionPoint { point.row, max_col_inclusive() } };
            invalidate_all();
            return;
    }
    di::unreachable();
}

void Screen::update_selection(SelectionPoint const& point) {
    if (!m_selection) {
        begin_selection(point, BeginSelectionMode::Empty);
    }
    ASSERT(m_selection);
    if (point != m_selection.value().end) {
        m_selection.value().end = point;
        invalidate_all();
    }
}

auto Screen::in_selection(SelectionPoint const& point) const -> bool {
    if (m_selection.value_or({}).empty()) {
        return false;
    }
    auto [start, end] = m_selection.value().normalize();
    return point >= start && point <= end;
}

auto Screen::selected_text() const -> di::String {
    if (m_selection.value_or({}).empty()) {
        return {};
    }

    auto [start, end] = m_selection.value().normalize();
    ASSERT_GT_EQ(start.row, absolute_row_start());
    ASSERT_LT_EQ(start.row, absolute_row_end());

    auto text = ""_s;
    for (auto r = start.row; r <= end.row; r++) {
        // Fast path: the entire row is contained the selection.
        auto [row, group] = find_row(r);
        auto const& row_object = group.rows()[row];
        if (r > start.row && r < end.row) {
            text.append(row_object.text);
            if (!row_object.overflow) {
                text.push_back('\n');
            }
            continue;
        }

        // Slow path: iterate over the whole row and add the relevant cells.
        auto iter_start_col = r == start.row ? start.col : 0_usize;
        auto iter_end_col = r == end.row ? end.col : row_object.cells.size();
        for (auto values : group.iterate_row(row)) {
            auto [c, _, cell_text, _, _] = values;
            if (c < iter_start_col || c > iter_end_col) {
                continue;
            }
            text.append(cell_text);
        }
        if (r != end.row && !row_object.overflow) {
            text.push_back('\n');
        }
    }
    return text;
}

void Screen::clamp_selection() {
    for (auto& selection : m_selection) {
        selection.start.row = di::clamp(selection.start.row, absolute_row_start(), absolute_row_end() - 1);
        selection.end.row = di::clamp(selection.end.row, absolute_row_start(), absolute_row_end() - 1);
    }
}

auto Screen::find_row(u64 row) const -> di::Tuple<u32, RowGroup const&> {
    ASSERT_GT_EQ(row, absolute_row_start());
    ASSERT_LT(row, absolute_row_end());

    if (row >= m_scroll_back.absolute_row_end()) {
        return { u32(row - m_scroll_back.absolute_row_end()), m_active_rows };
    }
    return m_scroll_back.find_row(row);
}

auto Screen::state_as_escape_sequences() const -> di::String {
    auto writer = di::VectorWriter<> {};

    auto write_hyperlink = [&](di::Optional<Hyperlink const&> hyperlink) {
        if (!hyperlink) {
            di::writer_print<di::String::Encoding>(writer, "{}"_sv, OSC8::from_hyperlink(hyperlink).serialize());
            return;
        }

        // To ensure the escape sequence representation is a fixed point, we need to strip our fixed prefix from the
        // hyperlink id.
        auto prefix = hyperlink.value().id.find(U'-');
        ASSERT(prefix);
        auto new_id = hyperlink.value().id.substr(prefix.end());
        auto new_hyperlink = Hyperlink { .uri = di::clone(hyperlink.value().uri), .id = new_id.to_owned() };
        di::writer_print<di::String::Encoding>(writer, "{}"_sv, OSC8::from_hyperlink(new_hyperlink).serialize());
    };

    // 1. Set default margins
    di::writer_print<di::String::Encoding>(writer, "\033[r"_sv);

    // 2. Set origin mode
    if (m_origin_mode == OriginMode::Enabled) {
        di::writer_print<di::String::Encoding>(writer, "\033[?6h"_sv);
    }

    // 3. Screen contents
    auto prev_sgr = GraphicsRendition();
    auto prev_hyperlink = di::Optional<Hyperlink const&> {};
    for (auto r : di::range(absolute_row_start(), absolute_row_end())) {
        // Once we get to the screen buffer, force the screen to fully scroll.
        if (r == absolute_row_screen_start()) {
            for (auto _ : di::range(max_height() - 1)) {
                di::writer_print<di::String::Encoding>(writer, "\n"_sv);
            }
            di::writer_print<di::String::Encoding>(writer, "\033[H"_sv);
        }

        auto [row, group] = find_row(r);
        for (auto row : group.iterate_row(row)) {
            auto [c, _, text, gfx, hyperlink] = row;

            // If we're at the cursor, and overflow isn't pending, then save the cursor now.
            auto at_cursor = r >= absolute_row_screen_start() && r - absolute_row_screen_start() == m_cursor.row &&
                             c == m_cursor.col;
            if (at_cursor && (text.empty() || !m_cursor.overflow_pending)) {
                di::writer_print<di::String::Encoding>(writer, "\0337"_sv);
            }

            // For now, we assume no text means no graphics. If we support https://sw.kovidgoyal.net/kitty/deccara/,
            // we would use that to handle cells with no text.
            if (text.empty()) {
                di::writer_print<di::String::Encoding>(writer, "\033[C"_sv);
                continue;
            }

            // Write out the cell
            if (gfx != prev_sgr) {
                for (auto& params : gfx.as_csi_params()) {
                    di::writer_print<di::String::Encoding>(writer, "\033[{}m"_sv, params);
                }
                prev_sgr = gfx;
            }
            if (hyperlink != prev_hyperlink) {
                write_hyperlink(hyperlink);
                prev_hyperlink = hyperlink;
            }
            di::writer_print<di::String::Encoding>(writer, "{}"_sv, text);

            // Since overflow was pending, save the cursor position after writing the cell.
            if (at_cursor && m_cursor.overflow_pending) {
                di::writer_print<di::String::Encoding>(writer, "\0337"_sv);
            }
        }

        // If the row hasn't overflowed, go to the next line. This doesn't apply for the last line.
        auto const& row_object = group.rows()[row];
        if (!row_object.overflow && r != absolute_row_end() - 1) {
            di::writer_print<di::String::Encoding>(writer, "\r\n"_sv);
        }
    }

    // 4. Scroll region
    // This is saved before the cursor because setting the scroll region also moves the cursor.
    di::writer_print<di::String::Encoding>(writer, "\033[{};{}r"_sv, m_scroll_region.start_row + 1,
                                           m_scroll_region.end_row + 2);

    // 5. Cursor
    di::writer_print<di::String::Encoding>(writer, "\0338"_sv);

    // 6. Current sgr
    for (auto& params : current_graphics_rendition().as_csi_params()) {
        di::writer_print<di::String::Encoding>(writer, "\033[{}m"_sv, params);
    }

    // 7. Current hyperlink
    if (auto hyperlink = current_hyperlink(); hyperlink != prev_hyperlink) {
        write_hyperlink(current_hyperlink());
    }

    // Return the resulting string.
    return writer.vector() | di::transform(di::construct<c8>) | di::to<di::String>(di::encoding::assume_valid);
}
}
