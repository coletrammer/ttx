#include "ttx/terminal/screen.h"

#include "di/util/scope_exit.h"
#include "dius/print.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/cursor.h"

namespace ttx::terminal {
static u32 code_point_width(c32 code_point);

void Screen::resize(Size const& size) {
    // TODO: rewrap - for now we're truncating

    ASSERT_GT(size.rows, 0);
    ASSERT_GT(size.cols, 0);

    // We can ignore the height argument because we lazily add new rows if needed, and if the height shrinked
    // the caller will need to handle scroll back.
    // However, the width is used to trunacte or expand new cells.
    auto _ = di::ScopeExit([&] {
        m_size = size;
    });
    if (size.cols == max_width()) {
        return;
    }

    if (size.cols > max_width()) {
        // When expanding, just add blank cells.
        for (auto& row : m_rows) {
            row.cells.resize(size.cols);
        }
    } else {
        // When contracting, we need to free resources used by each cell we're deleting.
        for (auto& row : m_rows) {
            auto text_bytes_to_delete = 0zu;
            while (row.cells.size() > size.cols) {
                auto cell = *row.cells.pop_back();
                drop_cell(cell);

                text_bytes_to_delete += cell.text_size;
            }

            auto text_end = row.text.iterator_at_offset(row.text.size_bytes() - text_bytes_to_delete);
            ASSERT(text_end);
            row.text.erase(text_end.value(), row.text.end());
        }
    }
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

    row = di::min(row, max_height() - 1);
    col = di::min(col, max_width() - 1);

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

    col = di::min(col, max_width() - 1);
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

void Screen::insert_blank_characters(u32 count) {}

void Screen::insert_blank_lines(u32 count) {}

void Screen::delete_characters(u32 count) {}

void Screen::delete_lines(u32 count) {}

void Screen::clear() {
    // NOTE: to properly implement "bce", we'd have to actually
    // apply the current background color to all cells. For now, just
    // delete everything. We may choose to never support bce properly
    // like kitty, since it allows us to optimize clearing by fully
    // deleting all cells.

    for (auto& row : m_rows) {
        for (auto& cell : row.cells) {
            drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }
    m_rows.clear();

    // After clearing, there is no text.
    m_cursor.text_offset = 0;
}

void Screen::clear_after_cursor() {
    // First, clear the current cursor row.
    clear_row_after_cursor();

    // Now we just need to delete all lines below the cursor. As above, implementing
    // "bce" would require more work.
    if (m_cursor.row + 1 == max_height()) {
        return;
    }
    for (auto r = row_index(m_cursor.row + 1); r < m_rows.size(); r++) {
        auto& row = m_rows[r];
        for (auto& cell : row.cells) {
            drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }
    m_rows.erase(row_iterator(m_cursor.row + 1), m_rows.end());
}

void Screen::clear_before_cursor() {
    // First, clear the current cursor row.
    clear_row_before_cursor();

    // Now delete all lines before the cursor. As above, implementing
    // "bce" would require more work. However, we don't actually delete
    // the rows here because rows are always assumed to start at the top
    // of the screen.
    if (m_cursor.row == 0) {
        return;
    }
    for (auto r = 0zu; r < row_index(m_cursor.row); r++) {
        auto& row = m_rows[r];
        for (auto& cell : row.cells) {
            drop_cell(cell);
            cell.text_size = 0;
        }
        row.text.clear();
        row.overflow = false;
    }
}

void Screen::clear_row() {
    auto it = row_iterator(m_cursor.row);
    if (it == m_rows.end()) {
        return;
    }

    auto& row = *it;
    for (auto& cell : row.cells) {
        drop_cell(cell);
        cell.text_size = 0;
    }
    row.text.clear();
    row.overflow = false;

    // We deleted all the text on the cursor's row.
    m_cursor.text_offset = 0;
}

void Screen::clear_row_after_cursor() {
    auto it = row_iterator(m_cursor.row);
    if (it == m_rows.end()) {
        return;
    }

    auto& row = *it;
    for (auto& cell : row.cells | di::drop(m_cursor.col)) {
        drop_cell(cell);
        cell.text_size = 0;
    }
    row.overflow = false;
}

void Screen::clear_row_before_cursor() {
    auto it = row_iterator(m_cursor.row);
    if (it == m_rows.end()) {
        return;
    }

    auto& row = *it;
    for (auto& cell : row.cells | di::take(m_cursor.col)) {
        drop_cell(cell);
        cell.text_size = 0;
    }

    // Delete all text before the cursor.
    auto text_end = row.text.iterator_at_offset(m_cursor.text_offset);
    ASSERT(text_end);
    row.text.erase(row.text.begin(), text_end.value());

    // We deleted all the text before the cursor.
    m_cursor.text_offset = 0;
}

void Screen::scroll_down() {
    ASSERT_EQ(m_cursor.row + 1, max_height());
    m_rows.emplace_back();
    m_cursor.text_offset = 0;
    m_cursor.overflow_pending = false;
    invalidate_all();
}

void Screen::put_code_point(c32 code_point) {
    // 1. Measure the width of the code point.
    auto width = code_point_width(code_point);

    // 2. Happy path - width is 1, so add a single cell.
    if (width == 1) {
        auto code_units = di::encoding::convert_to_code_units(di::String::Encoding(), code_point);
        auto view = di::StringView(di::encoding::assume_valid, code_units.begin(), code_units.end());
        put_single_cell(view);
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
        auto& row = get_row(m_cursor.row);
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

void Screen::put_single_cell(di::StringView text) {
    // Sanity check - if the text size is larger than 2^15, ignore.
    if (text.size_bytes() > di::NumericLimits<u16>::max / 2) {
        return;
    }

    // Fetch and validate the row from the cursor position.
    auto& row = get_row(m_cursor.row);
    ASSERT_LT(m_cursor.col, max_width());
    ASSERT_EQ(row.cells.size(), max_width());

    // Check for the overflow condition, which will evenually induce scrolling.
    if (m_cursor.overflow_pending) {
        // Mark the current row as having overflowed, and then advance the cursor.
        row.overflow = true;

        auto new_cursor = Cursor { m_cursor.row + 1, 0, 0, false };
        if (m_cursor.row + 1 == max_height()) {
            // This was the last line - so induce scrolling by adding a new row.
            scroll_down();
        } else {
            m_cursor = new_cursor;
        }
        put_single_cell(text);
        return;
    }

    // Modify the cell with the new attributes, starting by clearing the old attributes.
    auto& cell = row.cells[m_cursor.col];
    drop_cell(cell);

    if (m_graphics_id) {
        cell.graphics_rendition_id = m_graphics_renditions.use_id(m_graphics_id);
    }
    if (m_hyperlink_id) {
        cell.hyperlink_id = m_hyperlinks.use_id(m_hyperlink_id);
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

void Screen::invalidate_all() {
    for (auto& row : m_rows) {
        for (auto& cell : row.cells) {
            cell.stale = false;
        }
    }
}

auto Screen::row_index(u32 row) const -> usize {
    ASSERT_LT(row, max_height());

    if (m_rows.size() > max_height()) {
        return row + (m_rows.size() - max_height());
    }
    return row;
}

auto Screen::get_row(u32 row) -> Row& {
    ASSERT_LT(row, max_height());

    // Add empty rows lazily if necessary.
    while (m_rows.size() <= row) {
        m_rows.emplace_back();
    }

    auto& result = [&] -> Row& {
        // Normal case: the number of rows is smaller than the maximum height.
        if (m_rows.size() <= max_height()) {
            return m_rows[row];
        }

        // Scrolling case: we need to access the row assuming the first N rows will soon be deleted.
        return m_rows[row + (m_rows.size() - max_height())];
    }();

    // Ensure the row has the proper number of cells.
    if (result.cells.empty()) {
        result.cells.resize(max_width());
    }
    ASSERT_EQ(result.cells.size(), max_width());
    return result;
}

void Screen::drop_graphics_id(u16& id) {
    if (id) {
        m_graphics_renditions.drop_id(id);
        id = 0;
    }
}

void Screen::drop_hyperlink_id(u16& id) {
    if (id) {
        m_hyperlinks.drop_id(id);
        id = 0;
    }
}

void Screen::drop_multi_cell_id(u16& id) {
    if (id) {
        m_multi_cell_info.drop_id(id);
        id = 0;
    }
}

void Screen::drop_cell(Cell& cell) {
    drop_graphics_id(cell.graphics_rendition_id);
    drop_hyperlink_id(cell.hyperlink_id);

    // TODO: clear the whole multi cell here.
    drop_multi_cell_id(cell.multicell_id);

    cell.stale = false;
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
