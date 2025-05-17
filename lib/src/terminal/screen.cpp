#include "ttx/terminal/screen.h"

#include "di/container/algorithm/rotate.h"
#include "di/io/vector_writer.h"
#include "di/util/clamp.h"
#include "di/util/construct.h"
#include "di/util/scope_exit.h"
#include "dius/print.h"
#include "dius/unicode/emoji.h"
#include "dius/unicode/general_category.h"
#include "dius/unicode/grapheme_cluster.h"
#include "dius/unicode/name.h"
#include "dius/unicode/width.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/cell.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/escapes/osc_66.h"
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

    // When resizing, just invalidate everything. Resize happens when
    // the layout changes and the caller expects us to redraw everything.
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

    // Invalidate all moved rows as necessary.
    if (m_cursor.row == 0) {
        invalidate_all();
    } else {
        for (auto& row : di::View(rows().begin() + m_cursor.row, end_row_iterator())) {
            row.stale = false;
        }
    }
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

    // Invalidate all moved rows as necessary.
    if (delete_row_it == rows().begin()) {
        invalidate_all();
    } else {
        for (auto& row : di::View(delete_row_it, end_row_iterator())) {
            row.stale = false;
        }
    }
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
    auto prev_cell = di::Optional<di::Tuple<Row&, Cell&, usize, u8, u32, u32>> {};
    if (m_cursor.row > 0 && m_cursor.col == 0) {
        auto& row = rows()[m_cursor.row - 1];
        auto width = 1_u8;
        auto candidate_position = max_width() - 1;
        while (candidate_position > 0 && row.cells[candidate_position].is_nonprimary_in_multi_cell()) {
            candidate_position--;
            width++;
        }
        auto& cell = row.cells[candidate_position];
        prev_cell = { row, cell, row.text.size_bytes() - cell.text_size, width, m_cursor.row - 1, candidate_position };
    } else if (m_cursor.col > 0) {
        auto& row = rows()[m_cursor.row];
        auto width = 1_u8;
        auto candidate_position = m_cursor.col - !m_cursor.overflow_pending;
        while (candidate_position > 0 && row.cells[candidate_position].is_nonprimary_in_multi_cell()) {
            candidate_position--;
            width++;
        }
        auto& cell = row.cells[candidate_position];
        if (candidate_position == m_cursor.col) {
            prev_cell = { row, cell, m_cursor.text_offset, width, m_cursor.row, candidate_position };
        } else {
            prev_cell = { row, cell, m_cursor.text_offset - cell.text_size, width, m_cursor.row, candidate_position };
        }
    }

    // 3. Combining path - width is 0, so potentially add the
    //    code point to the previous cell. In certain cases,
    //    the combining character will be ignored. This is the
    //    behavior implemented in kitty 0.42.0. This does
    //    result in problematic cases because it effectively
    //    disallows using a code point in the Prepend class
    //    from existing in the start of the cell (without using
    //    explicit sizing via OSC 66). On the other hand, the
    //    alternate behavior is less backwards compatible with
    //    old implementations, which will treat any width 0
    //    code point as combining. Let's follow kitty as there
    //    is an open spec documenting its behavior.
    auto code_units = di::encoding::convert_to_code_units(di::String::Encoding(), code_point);
    if (width == 0) {
        // Ignore code point if there is no previous cell or the
        // previous cell has no text.
        if (!prev_cell) {
            return;
        }

        // Now, we're safe to add the 0 width character to the cell's text.
        auto [row, primary_cell, text_offset, width, r, c] = prev_cell.value();
        if (primary_cell.text_size + code_units.size_bytes() > Cell::max_text_size) {
            return;
        }

        // Check for variation selector 15 and 16. For now, variation selector 15
        // results in explicitly sizing the text, but may cause cells to
        // shrink if terminals to implement that part of the kitty text sizing
        // spec.
        auto it = row.text.iterator_at_offset(text_offset + primary_cell.text_size);
        ASSERT(it);
        if (code_point == dius::unicode::VariationSelector_15) {
            primary_cell.explicitly_sized = true;
        }
        if (code_point == dius::unicode::VariationSelector_16) {
            // For variation selector 16, if the previous character
            // was an emoji and the cell isn't already a wide cell,
            // the cell's width is promoted to have width 2.
            auto prev = *di::prev(it.value());
            if (width == 1 && !primary_cell.explicitly_sized &&
                dius::unicode::emoji(prev) == dius::unicode::Emoji::Yes) {
                // In this case, we need to increase the cell width to 2. This
                // is especially annoying when this would cause the current cell
                // to wrap. To implement this cleanly, we fetch the full attributes
                // and call put_cell() directly, after first clearing the current
                // cell.
                auto text_start = row.text.iterator_at_offset(text_offset);
                ASSERT(text_start);
                auto new_text = di::StringView(text_start.value(), it.value()).to_owned();
                new_text.push_back(code_point);
                row.text.erase(text_start.value(), it.value());
                primary_cell.text_size = 0;

                // We know our goal is to call put_cell(), but that function requires
                // the current graphics attributes and hyperlink value are correct. So
                // we save our current ids and then take the other ids directly from
                // the primary cell.
                auto gfx_save = m_graphics_id;
                auto hyperlink_save = m_hyperlink_id;
                auto _ = di::ScopeExit([&] {
                    m_active_rows.drop_graphics_id(m_graphics_id);
                    m_graphics_id = gfx_save;
                    m_active_rows.drop_hyperlink_id(m_hyperlink_id);
                    m_hyperlink_id = hyperlink_save;
                });
                m_graphics_id = di::exchange(primary_cell.graphics_rendition_id, 0);
                m_hyperlink_id = di::exchange(primary_cell.hyperlink_id, 0);

                auto info = m_active_rows.multi_cell_info(primary_cell.multi_cell_id);
                info.width = 2;

                // Drop the old cell and add the new one, first moving the cursor to the previous cell.
                m_active_rows.drop_cell(primary_cell);
                m_cursor.row = r;
                m_cursor.col = c;
                m_cursor.text_offset = text_offset;
                put_cell(new_text.view(), info, auto_wrap_mode, true, false);
                return;
            }
        }

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
    //    add to the previous cell. Some terminals (not kitty) seem to guard this condition on the character
    //    being non-ASCII. This is a useful optimization, but is incorrect because UAX 29 specifies "Prepend"
    //    characters, which do not break with ASCII. Implementing the aforementioned optimization would result
    //    in failing the kitty width tests.
    //
    //    In the future, we can optimize this routine by caching the grapheme clustering state across calls
    //    the put_code_point(), which get invalidated whenever any other function is called. This is a significant
    //    improvement over the algorithm below, which is quadratic when inserting N characters into the same cell.
    //    This state could also be used to enable the ASCII optimization for 2 subsequent ASCII characters.
    if (prev_cell) {
        auto clusterer = dius::unicode::GraphemeClusterer {};
        auto [row, primary_cell, text_offset, _, _, _] = prev_cell.value();
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
            if (text.size_bytes() + code_units.size_bytes() > Cell::max_text_size) {
                return;
            }
            auto [s, e] = row.text.insert(text_end.value(), code_point);
            auto byte_size = e.data() - s.data();
            if (m_cursor.col > 0) {
                m_cursor.text_offset += byte_size;
            }
            primary_cell.text_size += byte_size;
            primary_cell.complex_grapheme_cluster = true;
            primary_cell.stale = false;
            return;
        }
    }

    // 5. Happy path - width is 1, so add a single cell.
    if (width == 1) {
        auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
        put_single_cell(view, narrow_multi_cell_info, auto_wrap_mode, false, false);
        return;
    }

    // 6. Multi-cell case - width is 2.
    ASSERT_EQ(width, 2);
    auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
    put_wide_cell(view, terminal::wide_multi_cell_info, auto_wrap_mode, false, false);
}

void Screen::put_osc66(OSC66 const& sized_text, AutoWrapMode auto_wrap_mode) {
    // 0. Update flag, which is is used when resizing to prevent committing blank lines to the scroll back buffer
    //    in cases where the terminal's initial size is changed before we get any input.
    m_never_got_input = false;

    // 1. Scale>0 (multi-height cell). For now we don't support this.
    if (sized_text.info.scale > 1) {
        return;
    }

    // 2. Explicit width case. This maps directly to put_wide_cell().
    if (sized_text.info.width > 0) {
        put_cell(sized_text.text, sized_text.info, auto_wrap_mode, true, false);
        return;
    }

    // 3. Auto width mode. We need to break up the text into cells according to our normal algorithm.
    //    Splitting cells text into cell should have the exact same logic from put_code_point() when
    //    grapheme clustering is enabled. Note there isn't legacy behavior for this escape sequence
    //    because kitty fully specified this behavior.
    auto cells = di::Vector<di::Tuple<di::StringView, u8, bool, bool>> {};
    auto clusterer = dius::unicode::GraphemeClusterer {};
    for (auto it = sized_text.text.begin(); it != sized_text.text.end(); ++it) {
        auto code_point = *it;
        auto width = dius::unicode::code_point_width(code_point).value_or(0);
        auto is_break = clusterer.is_boundary(code_point);
        if (width == 0) {
            if (cells.empty()) {
                // If the grapheme width is 0, the text must be appended to the previous cell. Since
                // we're using a previous cell, the multi-cell info is ignored.
                put_code_point(code_point, auto_wrap_mode);
                continue;
            }

            auto& [text, width, explicitly_sized, complex_grapheme_cluster] = cells.back().value();
            text = { text.begin(), di::next(it) };

            // Variation selector 16 may promote a cell to have width 2. If we do
            // increase the cell width, force the explicitly sized flag.
            if (code_point == dius::unicode::VariationSelector_16) {
                if (dius::unicode::emoji(text.back().value()) == dius::unicode::Emoji::Yes && width < 2) {
                    width = 2;
                    explicitly_sized = true;
                }
            }
            if (code_point == dius::unicode::VariationSelector_15) {
                // Kitty will down-sizes some code points with variation selector 15. Until other terminals
                // adopt this behavior, we will explicitly size any text with variation selector to override
                // kitty's default behavior.
                explicitly_sized = true;
            }
            continue;
        }

        if (is_break || cells.empty()) {
            cells.push_back({ di::StringView(it, di::next(it)), width, false, false });
            continue;
        }

        // This is the combining case. Because this cell's width is non-zero, it is a complex grapheme.
        auto& [text, _, explicitly_sized, complex_grapheme_cluster] = cells.back().value();
        text = { text.begin(), di::next(it) };
        complex_grapheme_cluster = true;
    }

    for (auto [text, width, explicitly_sized, complex_grapheme_cluster] : cells) {
        auto info = sized_text.info;
        info.width = width;
        put_cell(text, info, auto_wrap_mode, explicitly_sized, complex_grapheme_cluster);
    }
}

void Screen::put_cell(di::StringView text, MultiCellInfo const& multi_cell_info, AutoWrapMode auto_wrap_mode,
                      bool explicitly_sized, bool complex_grapheme_cluster) {
    ASSERT_NOT_EQ(multi_cell_info.compute_width(), 0);

    if (text.size_bytes() > Cell::max_text_size) {
        return;
    }
    if (multi_cell_info.compute_width() == 1) {
        put_single_cell(text, multi_cell_info, auto_wrap_mode, explicitly_sized, complex_grapheme_cluster);
    } else {
        put_wide_cell(text, multi_cell_info, auto_wrap_mode, explicitly_sized, complex_grapheme_cluster);
    }
}

void Screen::put_single_cell(di::StringView text, MultiCellInfo const& multi_cell_info, AutoWrapMode auto_wrap_mode,
                             bool explicitly_sized, bool complex_grapheme_cluster) {
    ASSERT_EQ(multi_cell_info.compute_width(), 1);

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
        put_single_cell(text, multi_cell_info, auto_wrap_mode, explicitly_sized, complex_grapheme_cluster);
        return;
    }

    // Try to the allocate the multi cell info. This is required, and so we bail if
    // this fails.
    auto multi_cell_id = m_active_rows.maybe_allocate_multi_cell_id(multi_cell_info);
    if (!multi_cell_id.has_value()) {
        return;
    }

    // We have to clear text starting from the insertion point. However, we have to extend the
    // deletion of cells to account for a potential multi cell already partially occuppying the
    // new multi cell.
    auto insertion_point = m_cursor.col;
    auto text_start_position = m_cursor.text_offset;
    auto& cell = row.cells[insertion_point];
    if (cell.graphics_rendition_id == m_graphics_id && cell.hyperlink_id == m_hyperlink_id &&
        cell.multi_cell_id == multi_cell_id && cell.text_size == text.size_bytes()) {
        // Since everything else matches, we only need to update potentially the text.
        auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
        auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + cell.text_size);
        ASSERT(text_start.has_value());
        ASSERT(text_end.has_value());
        if (row.text.substr(text_start.value(), text_end.value()) != text) {
            row.text.replace(text_start.value(), text_end.value(), text);
            cell.stale = false;
        }
        cell.explicitly_sized = explicitly_sized;
        cell.complex_grapheme_cluster = complex_grapheme_cluster;
        m_active_rows.drop_multi_cell_id(multi_cell_id.value());
    } else {
        auto deletion_point = insertion_point;
        if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
            while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
                deletion_point--;
            }
        }
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
        if (m_graphics_id) {
            cell.graphics_rendition_id = m_active_rows.use_graphics_id(m_graphics_id);
        }
        if (m_hyperlink_id) {
            cell.hyperlink_id = m_active_rows.use_hyperlink_id(m_hyperlink_id);
        }

        // No need to bump reference because allocation already gave us a reference.
        cell.left_boundary_of_multicell = (multi_cell_id != 0);
        cell.multi_cell_id = multi_cell_id.value();
        cell.explicitly_sized = explicitly_sized;
        cell.complex_grapheme_cluster = complex_grapheme_cluster;
        cell.stale = false;

        // Insert the text. We can safely assert the iterator is valid because we compute the text offset in all cases
        // and it should be correct. Additionally, because each cell contains valid UTF-8 text, the cell boundaries
        // will always be valid insertion points.
        auto text_start = row.text.iterator_at_offset(text_start_position);
        auto text_end = row.text.iterator_at_offset(text_end_position);
        ASSERT(text_start.has_value());
        ASSERT(text_end.has_value());
        row.text.replace(text_start.value(), text_end.value(), text);
        cell.text_size = text.size_bytes();
    }

    // Advance the cursor 1 cell.
    auto new_cursor = m_cursor;
    if (new_cursor.col + 1 == max_width()) {
        new_cursor.overflow_pending = true;
        new_cursor.text_offset = text_start_position;
    } else {
        new_cursor.col++;
        new_cursor.text_offset = text_start_position + text.size_bytes();
    }
    m_cursor = new_cursor;
}

void Screen::put_wide_cell(di::StringView text, MultiCellInfo const& multi_cell_info, AutoWrapMode auto_wrap_mode,
                           bool explicitly_sized, bool complex_grapheme_cluster) {
    auto width = multi_cell_info.compute_width();
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
        put_wide_cell(text, multi_cell_info, auto_wrap_mode, explicitly_sized, complex_grapheme_cluster);
        return;
    }

    // Try to the allocate the multi cell info. This is required, and so we bail if
    // this fails.
    auto multi_cell_id = m_active_rows.maybe_allocate_multi_cell_id(multi_cell_info);
    if (!multi_cell_id.has_value()) {
        return;
    }

    // Determine the insertion point. This may not be the cursor column when we're at the
    // end of the line.
    auto insertion_point = di::min(m_cursor.col, max_width() - width);
    auto text_start_position = m_cursor.text_offset;

    // Fast path: check for redundant updates. This means we're putting the same text into a cell.
    auto& primary_cell = row.cells[insertion_point];
    if (primary_cell.is_primary_in_multi_cell() && primary_cell.multi_cell_id == multi_cell_id.value() &&
        primary_cell.graphics_rendition_id == m_graphics_id && primary_cell.hyperlink_id == m_hyperlink_id &&
        primary_cell.text_size == text.size_bytes()) {
        // Since everything else matches, we only need to update potentially the text.
        auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
        auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + primary_cell.text_size);
        ASSERT(text_start.has_value());
        ASSERT(text_end.has_value());
        if (row.text.substr(text_start.value(), text_end.value()) != text) {
            row.text.replace(text_start.value(), text_end.value(), text);
            primary_cell.stale = false;
        }
        primary_cell.explicitly_sized = explicitly_sized;
        primary_cell.complex_grapheme_cluster = complex_grapheme_cluster;
        m_active_rows.drop_multi_cell_id(multi_cell_id.value());
    } else {
        // We have to clear text starting from the insertion point. However, we have to extend the
        // deletion of cells to account for a potential multi cell already partially occuppying the
        // new multi cell.
        auto deletion_point = insertion_point;
        if (row.cells[deletion_point].is_nonprimary_in_multi_cell()) {
            while (!row.cells[deletion_point].is_primary_in_multi_cell()) {
                deletion_point--;
            }
        }
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

        // Now that we've cleared any old text, set the attributes appropriately on the primary cells.
        primary_cell.left_boundary_of_multicell = true;
        primary_cell.multi_cell_id = multi_cell_id.value();
        primary_cell.explicitly_sized = explicitly_sized;
        primary_cell.complex_grapheme_cluster = complex_grapheme_cluster;
        primary_cell.stale = false;
        if (m_graphics_id) {
            primary_cell.graphics_rendition_id = m_active_rows.use_graphics_id(m_graphics_id);
        }
        if (m_hyperlink_id) {
            primary_cell.hyperlink_id = m_active_rows.use_hyperlink_id(m_hyperlink_id);
        }

        // Apply the multi cell id to all components of the multi cell.
        for (auto& cell : auto(*row.cells.subspan(insertion_point + 1, width - 1))) {
            cell.multi_cell_id = m_active_rows.use_multi_cell_id(multi_cell_id.value());
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
    }

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

void Screen::clear_damage_tracking() {
    for (auto const& row : m_active_rows.rows()) {
        for (auto const& cell : row.cells) {
            cell.stale = true;
        }
        row.stale = true;
    }
}

void Screen::clear_selection() {
    if (m_selection) {
        invalidate_region(m_selection.value());
    }
    m_selection = {};
}

auto Screen::clamp_selection_point(SelectionPoint const& point) const
    -> di::Tuple<SelectionPoint, RowGroup const&, u32> {
    // Clamp the selection point to ensure its in bounds. Also lookup the backing
    // row, which also is needed to clamping to the top left of the multi cell.
    auto adjusted_point = point;
    adjusted_point.row = di::clamp(point.row, absolute_row_start(), absolute_row_end() - 1);
    auto [row_index, row_group] = find_row(adjusted_point.row);
    auto const& row = row_group.rows()[row_index];
    adjusted_point.col = di::clamp(point.col, 0_u32, u32(row.cells.size() - 1));

    // Adjust the selection point to be at the start of any potential multicell.
    while (adjusted_point.col > 0 && row.cells[adjusted_point.col].is_nonprimary_in_multi_cell()) {
        adjusted_point.col--;
    }
    return di::make_tuple(adjusted_point, di::cref(row_group), row_index);
}

void Screen::begin_selection(SelectionPoint const& point, BeginSelectionMode mode) {
    clear_selection();

    auto [adjusted_point, row_group, row_index] = clamp_selection_point(point);
    auto const& row = row_group.rows()[row_index];
    switch (mode) {
        case BeginSelectionMode::Single: {
            m_selection = { adjusted_point, adjusted_point };
            row.cells[adjusted_point.col].stale = false;
            return;
        }
        case BeginSelectionMode::Word: {
            // For word selection, consider all non whitespace characters
            // as part of a word. This algorithm should work well enough
            // while still being simple to implement.
            auto text_per_cell = di::Vector<di::Tuple<di::StringView, bool>> {};
            for (auto row : iterate_row(adjusted_point.row)) {
                auto [_, cell, text, _, _, _] = row;
                text_per_cell.push_back({ text, cell.is_nonprimary_in_multi_cell() });
            }
            auto start = adjusted_point.col;
            auto end = adjusted_point.col;
            while (start > 0) {
                auto [text, is_nonprimary_in_multi_cell] = text_per_cell[start - 1];
                if (!is_nonprimary_in_multi_cell &&
                    (text.empty() || dius::unicode::general_category(*text.front()) ==
                                         dius::unicode::GeneralCategory::SpaceSeparator)) {
                    break;
                }
                start--;
            }
            while (end < max_col_inclusive()) {
                auto [text, is_nonprimary_in_multi_cell] = text_per_cell[end + 1];
                if (!is_nonprimary_in_multi_cell &&
                    (text.empty() || dius::unicode::general_category(*text.front()) ==
                                         dius::unicode::GeneralCategory::SpaceSeparator)) {
                    break;
                }
                end++;
            }
            m_selection = { SelectionPoint { adjusted_point.row, start }, SelectionPoint { adjusted_point.row, end } };
            for (auto const& cell : auto(*row.cells.subspan(start, end - start + 1))) {
                cell.stale = false;
            }
            return;
        }
        case BeginSelectionMode::Line:
            m_selection = { SelectionPoint { adjusted_point.row, 0 },
                            SelectionPoint { adjusted_point.row, u32(row.cells.size() - 1) } };
            row.stale = false;
            return;
    }
    di::unreachable();
}

void Screen::update_selection(SelectionPoint const& point) {
    auto [adjusted_point, row_group, row_index] = clamp_selection_point(point);
    // auto const& row = row_group.rows()[row_index];
    if (!m_selection) {
        begin_selection(adjusted_point, BeginSelectionMode::Single);
    }
    ASSERT(m_selection);
    if (adjusted_point != m_selection.value().end) {
        auto modified_region = Selection { m_selection.value().end, adjusted_point };
        m_selection.value().end = adjusted_point;
        invalidate_region(modified_region);
    }
}

auto Screen::in_selection(SelectionPoint const& point) const -> bool {
    if (!m_selection) {
        return false;
    }
    auto [start, end] = m_selection.value().normalize();
    return point >= start && point <= end;
}

void Screen::invalidate_region(Selection const& region) {
    auto [start, end] = region.normalize();
    ASSERT_GT_EQ(start.row, absolute_row_start());
    ASSERT_LT_EQ(start.row, absolute_row_end());
    ASSERT_GT_EQ(end.row, absolute_row_start());
    ASSERT_LT_EQ(end.row, absolute_row_end());

    // If we're invalidating a region larger than the screen, there's
    // point if having detailed damage tracking.
    if (end.row - start.row >= max_height()) {
        invalidate_all();
        return;
    }

    for (auto r = start.row; r <= end.row; r++) {
        // Fast path: the entire row is contained the selection.
        auto [row, group] = find_row(r);
        auto const& row_object = group.rows()[row];
        if (r > start.row && r < end.row) {
            row_object.stale = false;
            continue;
        }

        // Slow path: iterate over the whole row and invalidate the relevant cells.
        auto iter_start_col = r == start.row ? start.col : 0_usize;
        auto iter_end_col = r == end.row ? end.col : row_object.cells.size() - 1;
        for (auto const& cell : auto(*row_object.cells.subspan(iter_start_col, iter_end_col - iter_start_col + 1))) {
            cell.stale = false;
        }
    }
}

auto Screen::selected_text() const -> di::String {
    if (!m_selection) {
        return {};
    }

    auto [start, end] = m_selection.value().normalize();
    ASSERT_GT_EQ(start.row, absolute_row_start());
    ASSERT_LT_EQ(start.row, absolute_row_end());
    ASSERT_GT_EQ(end.row, absolute_row_start());
    ASSERT_LT_EQ(end.row, absolute_row_end());

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
        auto iter_end_col = r == end.row ? end.col : row_object.cells.size() - 1;
        for (auto values : group.iterate_row(row)) {
            auto [c, _, cell_text, _, _, _] = values;
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
        auto [start, _, _] = clamp_selection_point(selection.start);
        auto [end, _, _] = clamp_selection_point(selection.end);
        selection.start = start;
        selection.end = end;
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
            auto [c, cell, text, gfx, hyperlink, multi_cell_info] = row;

            // If we're at the cursor, and overflow isn't pending, then save the cursor now.
            auto at_cursor = r >= absolute_row_screen_start() && r - absolute_row_screen_start() == m_cursor.row &&
                             c == m_cursor.col;
            if (at_cursor && (text.empty() || !m_cursor.overflow_pending)) {
                di::writer_print<di::String::Encoding>(writer, "\0337"_sv);
            }

            // Ignore rendering non-primary multi cells.
            if (cell.is_nonprimary_in_multi_cell()) {
                continue;
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

            // Write out the text according the explicit size flag and account for non-standard multi-cell info.
            if (cell.explicitly_sized) {
                auto osc66 = OSC66(multi_cell_info, text);
                di::writer_print<di::String::Encoding>(writer, "{}"_sv, osc66.serialize());
            } else if (multi_cell_info != narrow_multi_cell_info && multi_cell_info != wide_multi_cell_info) {
                auto osc66 = OSC66(multi_cell_info, text);
                osc66.info.width = 0;
                di::writer_print<di::String::Encoding>(writer, "{}"_sv, osc66.serialize());
            } else {
                di::writer_print<di::String::Encoding>(writer, "{}"_sv, text);
            }

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
