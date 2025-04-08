#include "ttx/terminal/screen.h"

#include "di/container/algorithm/rotate.h"
#include "di/util/clamp.h"
#include "di/util/scope_exit.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/cell.h"
#include "ttx/terminal/cursor.h"

namespace ttx::terminal {
static u32 code_point_width(c32 code_point);

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
            m_scroll_back.take_rows(m_active_rows, max_width(), 0, rows_to_take);
            m_cursor.row += rows_to_take;

            // In this case, we may need to adjust the visual scroll offset.
            if (m_visual_scroll_offset > absolute_row_screen_start()) {
                m_visual_scroll_offset = absolute_row_screen_start();
            }
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

auto Screen::text_at_cursor() -> di::StringView {
    ASSERT_LT(m_cursor.row, max_height());
    auto& row = rows()[m_cursor.row];
    auto& cell = row.cells[m_cursor.col];

    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + cell.text_size);
    ASSERT(text_start);
    ASSERT(text_end);

    return row.text.substr(text_start.value(), text_end.value());
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
    set_cursor(cursor.row, cursor.col);
    set_current_graphics_rendition(cursor.graphics_rendition);

    // We don't call set_origin_mode() because that also resets
    // the cursor position.
    m_origin_mode = cursor.origin_mode;

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

void Screen::set_cursor(u32 row, u32 col, bool overflow_pending) {
    set_cursor(row, col);
    m_cursor.overflow_pending = overflow_pending;
}

void Screen::set_cursor(u32 row, u32 col) {
    // Setting the cursor always clears the overflow pending flag.
    m_cursor.overflow_pending = false;

    row = di::clamp(translate_row(row), min_row(), max_row_inclusive());
    col = di::clamp(translate_col(col), min_col(), max_col_inclusive());

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

void Screen::set_cursor_row(u32 row) {
    set_cursor(row, m_cursor.col);
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

    // Start by dropping the correct number of cells, based on how many new ones we need
    // to insert.
    auto& row = rows()[m_cursor.row];
    auto max_to_insert = di::min(count, max_width() - m_cursor.col);
    auto text_size_to_remove = 0zu;
    for (auto& cell : row.cells | di::drop(max_width() - max_to_insert)) {
        m_active_rows.drop_cell(cell);
        text_size_to_remove += cell.text_size;
        cell.text_size = 0;
    }
    row.cells.erase(row.cells.end() - max_to_insert, row.cells.end());

    // Erase the text of the deleted cells.
    auto text_start = row.text.iterator_at_offset(row.text.size_bytes() - text_size_to_remove);
    ASSERT(text_start);
    row.text.erase(text_start.value(), row.text.end());

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
    auto text_size_to_remove = 0zu;
    for (auto& cell : row.cells | di::drop(m_cursor.col) | di::take(max_to_delete)) {
        m_active_rows.drop_cell(cell);
        text_size_to_remove += cell.text_size;
        cell.text_size = 0;
    }
    row.cells.erase(row.cells.begin() + m_cursor.col, row.cells.begin() + m_cursor.col + max_to_delete);

    // Erase the text of the deleted cells.
    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + text_size_to_remove);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());

    // Insert blank cells at the end of the row. Note that to implement bce this would need to
    // preserve the background color. The cursor position is unchanged, as is the cursor byte offset.
    row.cells.resize(max_width());
    row.overflow = false;
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
    for (auto& cell : row.cells | di::drop(m_cursor.col)) {
        m_active_rows.drop_cell(cell);
        text_size_to_delete += cell.text_size;
        cell.text_size = 0;
    }
    row.overflow = false;

    // Delete text after the cursor.
    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + text_size_to_delete);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());
}

void Screen::clear_row_before_cursor() {
    m_cursor.overflow_pending = false;

    auto& row = rows()[m_cursor.row];
    auto text_size_to_delete = 0zu;
    for (auto& cell : row.cells | di::take(m_cursor.col + 1)) {
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
    auto text_size_to_delete = 0zu;
    for (auto& cell : row.cells | di::drop(m_cursor.col) | di::take(n)) {
        m_active_rows.drop_cell(cell);
        text_size_to_delete += cell.text_size;
        cell.text_size = 0;
    }

    // Delete text after the cursor.
    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + text_size_to_delete);
    ASSERT(text_start);
    ASSERT(text_end);
    row.text.erase(text_start.value(), text_end.value());

    // Clear row overflow flag if text after the cursor is fully deleted.
    if (text_end.value() == row.text.end()) {
        row.overflow = false;
    }
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
    auto width = code_point_width(code_point);

    // 2. Happy path - width is 1, so add a single cell.
    if (width == 1) {
        auto code_units = di::encoding::convert_to_code_units(di::String::Encoding(), code_point);
        auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
        put_single_cell(view, auto_wrap_mode);
        return;
    }

    // 3. Combining path - width is 0, so potentially add the
    //    code point to the previous cell. In certain cases,
    //    the combining character will be ignored.
    if (width == 0) {
        // Ignore code point if there is no previous cell or the
        // previous cell has no text.
        if (m_cursor.col == 0) {
            return;
        }
        auto& row = rows()[m_cursor.row];
        auto& prev_cell = row.cells[m_cursor.col - 1];
        if (prev_cell.text_size == 0) {
            return;
        }

        // Now, we're safe to add the 0 width character to the cell's text.
        auto it = row.text.iterator_at_offset(m_cursor.text_offset);
        ASSERT(it);
        auto [s, e] = row.text.insert(it.value(), code_point);
        auto byte_size = e.data() - s.data();
        m_cursor.text_offset += byte_size;
        prev_cell.text_size += byte_size;
        prev_cell.stale = false;
    }

    // TODO: 4. Multi-cell case - width is 2.
}

void Screen::put_single_cell(di::StringView text, AutoWrapMode auto_wrap_mode) {
    // Sanity check - if the text size is larger than 2^15, ignore.
    if (text.size_bytes() > di::NumericLimits<u16>::max / 2) {
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

    // Modify the cell with the new attributes, starting by clearing the old attributes.
    auto& cell = row.cells[m_cursor.col];
    m_active_rows.drop_cell(cell);

    if (m_graphics_id) {
        cell.graphics_rendition_id = m_active_rows.use_graphics_id(m_graphics_id);
    }
    if (m_hyperlink_id) {
        cell.hyperlink_id = m_active_rows.use_hyperlink_id(m_hyperlink_id);
    }

    // Insert the text. We can safely assert the iterator is valid because we compute the text offset in all cases
    // and it should be correct. Additionally, because each cell contains valid UTF-8 text, the cell boundaries
    // will always be valid insertion points.
    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + cell.text_size);
    ASSERT(text_start.has_value());
    ASSERT(text_end.has_value());
    row.text.replace(text_start.value(), text_end.value(), text);
    cell.text_size = text.size_bytes();

    // Mark cell as dirty.
    cell.stale = false;

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

void Screen::begin_selection(SelectionPoint const& point) {
    m_selection = { point, point };
}

void Screen::update_selection(SelectionPoint const& point) {
    if (!m_selection) {
        begin_selection(point);
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
    return point >= start && point < end;
}

auto Screen::selected_text() const -> di::String {
    if (m_selection.value_or({}).empty()) {
        return {};
    }

    auto [start, end] = m_selection.value().normalize();
    ASSERT_GT_EQ(start.row, absolute_row_start());
    ASSERT_LT_EQ(start.col, absolute_row_end());

    auto text = ""_s;
    for (auto r = start.row; r <= end.row; r++) {
        // Fast path: the entire row is contained the selection.
        auto [row, group] = find_row(r);
        auto& row_object = group.rows()[row];
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
            if (c < iter_start_col || c >= iter_end_col) {
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
    } else {
        return m_scroll_back.find_row(row);
    }
}

// This list is only for the diacritics used by the
// kitty image protocol.
auto zero_width_characters = di::Array {
    0x0305u,  0x030Du,  0x030Eu,  0x0310u,  0x0312u,  0x033Du,  0x033Eu,  0x033Fu,  0x0346u,  0x034Au,  0x034Bu,
    0x034Cu,  0x0350u,  0x0351u,  0x0352u,  0x0357u,  0x035Bu,  0x0363u,  0x0364u,  0x0365u,  0x0366u,  0x0367u,
    0x0368u,  0x0369u,  0x036Au,  0x036Bu,  0x036Cu,  0x036Du,  0x036Eu,  0x036Fu,  0x0483u,  0x0484u,  0x0485u,
    0x0486u,  0x0487u,  0x0592u,  0x0593u,  0x0594u,  0x0595u,  0x0597u,  0x0598u,  0x0599u,  0x059Cu,  0x059Du,
    0x059Eu,  0x059Fu,  0x05A0u,  0x05A1u,  0x05A8u,  0x05A9u,  0x05ABu,  0x05ACu,  0x05AFu,  0x05C4u,  0x0610u,
    0x0611u,  0x0612u,  0x0613u,  0x0614u,  0x0615u,  0x0616u,  0x0617u,  0x0657u,  0x0658u,  0x0659u,  0x065Au,
    0x065Bu,  0x065Du,  0x065Eu,  0x06D6u,  0x06D7u,  0x06D8u,  0x06D9u,  0x06DAu,  0x06DBu,  0x06DCu,  0x06DFu,
    0x06E0u,  0x06E1u,  0x06E2u,  0x06E4u,  0x06E7u,  0x06E8u,  0x06EBu,  0x06ECu,  0x0730u,  0x0732u,  0x0733u,
    0x0735u,  0x0736u,  0x073Au,  0x073Du,  0x073Fu,  0x0740u,  0x0741u,  0x0743u,  0x0745u,  0x0747u,  0x0749u,
    0x074Au,  0x07EBu,  0x07ECu,  0x07EDu,  0x07EEu,  0x07EFu,  0x07F0u,  0x07F1u,  0x07F3u,  0x0816u,  0x0817u,
    0x0818u,  0x0819u,  0x081Bu,  0x081Cu,  0x081Du,  0x081Eu,  0x081Fu,  0x0820u,  0x0821u,  0x0822u,  0x0823u,
    0x0825u,  0x0826u,  0x0827u,  0x0829u,  0x082Au,  0x082Bu,  0x082Cu,  0x082Du,  0x0951u,  0x0953u,  0x0954u,
    0x0F82u,  0x0F83u,  0x0F86u,  0x0F87u,  0x135Du,  0x135Eu,  0x135Fu,  0x17DDu,  0x193Au,  0x1A17u,  0x1A75u,
    0x1A76u,  0x1A77u,  0x1A78u,  0x1A79u,  0x1A7Au,  0x1A7Bu,  0x1A7Cu,  0x1B6Bu,  0x1B6Du,  0x1B6Eu,  0x1B6Fu,
    0x1B70u,  0x1B71u,  0x1B72u,  0x1B73u,  0x1CD0u,  0x1CD1u,  0x1CD2u,  0x1CDAu,  0x1CDBu,  0x1CE0u,  0x1DC0u,
    0x1DC1u,  0x1DC3u,  0x1DC4u,  0x1DC5u,  0x1DC6u,  0x1DC7u,  0x1DC8u,  0x1DC9u,  0x1DCBu,  0x1DCCu,  0x1DD1u,
    0x1DD2u,  0x1DD3u,  0x1DD4u,  0x1DD5u,  0x1DD6u,  0x1DD7u,  0x1DD8u,  0x1DD9u,  0x1DDAu,  0x1DDBu,  0x1DDCu,
    0x1DDDu,  0x1DDEu,  0x1DDFu,  0x1DE0u,  0x1DE1u,  0x1DE2u,  0x1DE3u,  0x1DE4u,  0x1DE5u,  0x1DE6u,  0x1DFEu,
    0x20D0u,  0x20D1u,  0x20D4u,  0x20D5u,  0x20D6u,  0x20D7u,  0x20DBu,  0x20DCu,  0x20E1u,  0x20E7u,  0x20E9u,
    0x20F0u,  0x2CEFu,  0x2CF0u,  0x2CF1u,  0x2DE0u,  0x2DE1u,  0x2DE2u,  0x2DE3u,  0x2DE4u,  0x2DE5u,  0x2DE6u,
    0x2DE7u,  0x2DE8u,  0x2DE9u,  0x2DEAu,  0x2DEBu,  0x2DECu,  0x2DEDu,  0x2DEEu,  0x2DEFu,  0x2DF0u,  0x2DF1u,
    0x2DF2u,  0x2DF3u,  0x2DF4u,  0x2DF5u,  0x2DF6u,  0x2DF7u,  0x2DF8u,  0x2DF9u,  0x2DFAu,  0x2DFBu,  0x2DFCu,
    0x2DFDu,  0x2DFEu,  0x2DFFu,  0xA66Fu,  0xA67Cu,  0xA67Du,  0xA6F0u,  0xA6F1u,  0xA8E0u,  0xA8E1u,  0xA8E2u,
    0xA8E3u,  0xA8E4u,  0xA8E5u,  0xA8E6u,  0xA8E7u,  0xA8E8u,  0xA8E9u,  0xA8EAu,  0xA8EBu,  0xA8ECu,  0xA8EDu,
    0xA8EEu,  0xA8EFu,  0xA8F0u,  0xA8F1u,  0xAAB0u,  0xAAB2u,  0xAAB3u,  0xAAB7u,  0xAAB8u,  0xAABEu,  0xAABFu,
    0xAAC1u,  0xFE20u,  0xFE21u,  0xFE22u,  0xFE23u,  0xFE24u,  0xFE25u,  0xFE26u,  0x10A0Fu, 0x10A38u, 0x1D185u,
    0x1D186u, 0x1D187u, 0x1D188u, 0x1D189u, 0x1D1AAu, 0x1D1ABu, 0x1D1ACu, 0x1D1ADu, 0x1D242u, 0x1D243u, 0x1D244u,
};

// TODO: use the unicode data base.
static auto code_point_width(c32 code_point) -> u32 {
    if (code_point <= 255) {
        return 1;
    }

    // TODO: optimize!
    if (di::contains(zero_width_characters, code_point)) {
        return 0;
    }
    return 1;
}
}
