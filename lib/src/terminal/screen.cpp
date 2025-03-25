#include "ttx/terminal/screen.h"

#include "ttx/graphics_rendition.h"
#include "ttx/terminal/cursor.h"

namespace ttx::terminal {
void Screen::resize(u32 width, u32 height) {
    // TODO: rewrap - for now we're truncating

    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    // We can ignore the height argument because we lazily add new rows if needed, and if the height shrinked
    // the caller will need to handle scroll back.
    m_max_height = height;

    // However, the width is used to trunacte or expand new cells.
    if (width == m_max_width) {
        return;
    }

    if (width > m_max_width) {
        // When expanding, just add blank cells.
        for (auto& row : m_rows) {
            row.cells.resize(width);
        }
    } else {
        // When contracting, we need to free resources used by each cell we're deleting.
        for (auto& row : m_rows) {
            while (row.cells.size() > width) {
                auto cell = *row.cells.pop_back();
                if (cell.graphics_rendition_id) {
                    m_multi_cell_info.drop_id(cell.graphics_rendition_id);
                    cell.graphics_rendition_id = 0;
                }

                if (cell.hyperlink_id) {
                    m_multi_cell_info.drop_id(cell.hyperlink_id);
                    cell.hyperlink_id = 0;
                }

                // TODO: clear the whole multi cell here.
                if (cell.multicell_id) {
                    m_multi_cell_info.drop_id(cell.multicell_id);
                    cell.multicell_id = 0;
                }
            }
        }
    }
    m_max_width = width;
}

auto Screen::current_graphics_rendition() const -> GraphicsRendition {
    if (!m_graphics_id) {
        return {};
    }
    return m_graphics_renditions.lookup_id(m_graphics_id);
}

auto Screen::current_hyperlink() const -> di::Optional<Hyperlink const&> {
    if (!m_hyperlink_id) {
        return {};
    }
    return m_hyperlinks.lookup_id(m_hyperlink_id);
}

void Screen::set_current_graphics_rendition(GraphicsRendition const& rendition) {
    if (rendition == GraphicsRendition {}) {
        drop_graphics_id(m_graphics_id);
        m_graphics_id = 0;
        return;
    }

    auto existing_id = m_graphics_renditions.lookup_key(rendition);
    if (existing_id) {
        if (*existing_id == m_graphics_id) {
            return;
        }
        drop_graphics_id(m_graphics_id);
        m_graphics_id = m_graphics_renditions.use_id(*existing_id);
        return;
    }

    drop_graphics_id(m_graphics_id);

    auto new_id = m_graphics_renditions.allocate(rendition);
    if (!new_id) {
        m_graphics_id = 0;
        return;
    }

    m_graphics_id = *new_id;
}

auto Screen::graphics_rendition(u16 id) const -> GraphicsRendition const& {
    if (id == 0) {
        return m_empty_graphics;
    }
    return m_graphics_renditions.lookup_id(id);
}

auto Screen::hyperlink(u16 id) const -> Hyperlink const& {
    ASSERT_NOT_EQ(id, 0);
    return m_hyperlinks.lookup_id(id);
}

auto Screen::maybe_hyperlink(u16 id) const -> di::Optional<Hyperlink const&> {
    if (id == 0) {
        return {};
    }
    return hyperlink(id);
}

auto Screen::text_at_cursor() -> di::StringView {
    if (m_cursor.row >= m_rows.size()) {
        return {};
    }
    auto& row = get_row(m_cursor.row);
    auto& cell = row.cells[m_cursor.col];

    auto text_start = row.text.iterator_at_offset(m_cursor.text_offset);
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset + cell.text_size);
    ASSERT(text_start);
    ASSERT(text_end);

    return row.text.substr(text_start.value(), text_end.value());
}

void Screen::set_cursor(u32 row, u32 col) {
    // Setting the cursor always clears the overflow pending flag.
    m_cursor.overflow_pending = false;

    row = di::min(row, m_max_height - 1);
    col = di::min(col, m_max_width - 1);

    // Special case when moving the cursor on the same row.
    if (m_cursor.row == row) {
        set_cursor_col(col);
        return;
    }

    m_cursor.row = row;
    m_cursor.col = col;
    m_cursor.text_offset = 0;
    if (row >= m_rows.size()) {
        return;
    }

    // Compute the text offset from the beginning of the row.
    auto& row_object = get_row(row);
    for (auto const& cell : row_object.cells | di::take(col)) {
        m_cursor.text_offset += cell.text_size;
    }
}

void Screen::set_cursor_row(u32 row) {
    set_cursor(row, m_cursor.col);
}

void Screen::set_cursor_col(u32 col) {
    // Setting the cursor always clears the overflow pending flag.
    m_cursor.overflow_pending = false;

    col = di::min(col, m_max_width - 1);
    if (m_cursor.col == col) {
        return;
    }

    // Special case when moving to column 0.
    if (col == 0) {
        m_cursor.text_offset = 0;
        m_cursor.col = 0;
        return;
    }

    if (m_cursor.row >= m_rows.size()) {
        return;
    }

    // Compute the text offset relative the current cursor.
    auto& row = get_row(m_cursor.row);
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

void Screen::put_single_cell(di::StringView text) {
    // Sanity check - if the text size is larger than 2^15, ignore.
    if (text.size_bytes() > di::NumericLimits<u16>::max / 2) {
        return;
    }

    // Fetch and validate the row from the cursor position.
    auto& row = get_row(m_cursor.row);
    ASSERT_LT(m_cursor.col, m_max_width);
    ASSERT_EQ(row.cells.size(), m_max_width);

    // Check for the overflow condition, which will evenually induce scrolling.
    if (m_cursor.overflow_pending) {
        // Mark the current row as having overflowed, and then advance the cursor.
        row.overflow = true;

        auto new_cursor = Cursor { m_cursor.row + 1, 0, 0, false };
        if (m_cursor.row + 1 == m_max_height) {
            // This was the last line - so induce scrolling by adding a new row.
            new_cursor.row--;
            m_rows.emplace_back();
        }
        m_cursor = new_cursor;
        put_single_cell(text);
        return;
    }

    // Modify the cell with the new attributes.
    auto& cell = row.cells[m_cursor.col];
    if (cell.graphics_rendition_id) {
        m_graphics_renditions.drop_id(cell.graphics_rendition_id);
        cell.graphics_rendition_id = 0;
    }
    if (m_graphics_id) {
        cell.graphics_rendition_id = m_graphics_renditions.use_id(m_graphics_id);
    }

    if (cell.hyperlink_id) {
        m_hyperlinks.drop_id(cell.hyperlink_id);
        cell.hyperlink_id = 0;
    }
    if (m_hyperlink_id) {
        cell.hyperlink_id = m_hyperlinks.use_id(m_hyperlink_id);
    }

    // Insert the text. We can safely assert the iterator is valid because we compute the text offset in all cases
    // and it should be correct. Additionally, because each cell contains valid UTF-8 text, the cell boundaries
    // will always be valid insertion points.
    auto text_it = row.text.iterator_at_offset(m_cursor.text_offset);
    ASSERT(text_it.has_value());
    row.text.insert(text_it.value(), text);
    cell.text_size = text.size_bytes();

    // TODO: clear the entire multi cell if there was one.
    if (cell.multicell_id) {
        m_multi_cell_info.drop_id(cell.multicell_id);
        cell.multicell_id = 0;
    }

    // Advance the cursor 1 cell.
    auto new_cursor = m_cursor;
    if (new_cursor.col + 1 == m_max_width) {
        new_cursor.overflow_pending = true;
    } else {
        new_cursor.col++;
        new_cursor.text_offset += text.size_bytes();
    }
    m_cursor = new_cursor;
}

auto Screen::get_row(u32 row) -> Row& {
    ASSERT_LT(row, m_max_height);

    // Add empty rows lazily if necessary.
    while (m_rows.size() <= row) {
        m_rows.emplace_back();
    }

    auto& result = [&] -> Row& {
        // Normal case: the number of rows is smaller than the maximum height.
        if (m_rows.size() <= m_max_height) {
            return m_rows[row];
        }

        // Scrolling case: we need to access the row assuming the first N rows will soon be deleted.
        return m_rows[row + (m_rows.size() - m_max_height)];
    }();

    // Ensure the row has the proper number of cells.
    if (result.cells.empty()) {
        result.cells.resize(m_max_width);
    }
    ASSERT_EQ(result.cells.size(), m_max_width);
    return result;
}

void Screen::drop_graphics_id(u16& id) {
    if (id) {
        m_graphics_renditions.drop_id(id);
        id = 0;
    }
}
}
