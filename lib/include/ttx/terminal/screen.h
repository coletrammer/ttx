#pragma once

#include "di/bit/bitset/prelude.h"
#include "di/container/ring/prelude.h"
#include "di/container/string/string_view.h"
#include "di/container/tree/tree_map.h"
#include "di/container/view/cache_last.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/hyperlink.h"
#include "ttx/terminal/id_map.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/row.h"

namespace ttx::terminal {
/// @brief Represents the visible contents of the terminal (with no scroll back)
///
/// The screen class internally maintains a ring buffer of terminal rows, as well as
/// the graphics rendition, hyperlink, and multi cell info indirectly referenced by the
/// Cell class.
///
/// The ring buffer of rows can exceed the maximum height in cases where overflow occurs.
/// In this case, the caller should commit the extra rows to the scroll back buffer, or
/// drop the rows entirely. Additionally, the actual number of rows can be smaller than
/// the maximum height, when rows at the bottom of the screen are empty. If there is rows
/// in the scroll back buffer, these can be used to fill the empty lines.
class Screen {
public:
    void resize(Size const& size);

    auto row_count() const -> u32 { return m_rows.size(); }
    auto max_height() const { return m_size.rows; }
    auto max_width() const { return m_size.cols; }
    auto size() const -> Size { return m_size; }

    auto current_graphics_rendition() const -> GraphicsRendition;
    auto current_hyperlink() const -> di::Optional<Hyperlink const&>;

    void set_current_graphics_rendition(GraphicsRendition const& rendition);
    void set_current_hyperlink(di::Optional<Hyperlink const&> hyperlink);

    auto graphics_rendition(u16 id) const -> GraphicsRendition const&;
    auto hyperlink(u16 id) const -> Hyperlink const&;
    auto maybe_hyperlink(u16 id) const -> di::Optional<Hyperlink const&>;

    auto cursor() const -> Cursor { return m_cursor; }
    auto text_at_cursor() -> di::StringView;

    auto save_cursor() const -> SavedCursor;
    void restore_cursor(SavedCursor const& cursor);

    void set_cursor(u32 row, u32 col);
    void set_cursor_row(u32 row);
    void set_cursor_col(u32 col);

    void insert_blank_characters(u32 count);
    void insert_blank_lines(u32 count);

    void delete_characters(u32 count);
    void delete_lines(u32 count);

    void clear();
    void clear_after_cursor();
    void clear_before_cursor();

    void clear_row();
    void clear_row_after_cursor();
    void clear_row_before_cursor();

    void scroll_down();
    void put_code_point(c32 code_point);

    void invalidate_all();

    auto iterate_row(u32 row) {
        ASSERT_LT(row, max_height());

        auto& row_object = get_row(row);

        // Fetch the indirect data fields for every cell in the row (text, graphics, and hyperlink). This uses
        // cache_last() so that the transform() can can maintain a mutable text offset counter safely, which
        // ensures fetching the text associated with a cell is O(1). cache_last() also ensures we do the map lookups
        // only once for each cell.
        return row_object.cells |
               di::transform([this, &row_object, text_offset = 0zu, col = 0u](Cell const& cell) mutable {
                   auto text = [&] {
                       if (cell.text_size == 0) {
                           return di::StringView {};
                       }

                       auto text_start = row_object.text.iterator_at_offset(text_offset);
                       text_offset += cell.text_size;
                       auto text_end = row_object.text.iterator_at_offset(text_offset);
                       ASSERT(text_start);
                       ASSERT(text_end);
                       return row_object.text.substr(text_start.value(), text_end.value());
                   }();

                   return di::make_tuple(col++, di::ref(cell), text,
                                         di::ref(graphics_rendition(cell.graphics_rendition_id)),
                                         maybe_hyperlink(cell.hyperlink_id));
               }) |
               di::cache_last;
    }

private:
    void put_single_cell(di::StringView text);

    auto row_iterator(u32 row) {
        ASSERT_LT_EQ(row, max_height());

        if (row >= m_rows.size()) {
            return m_rows.end();
        }
        if (m_rows.size() > max_height()) {
            return m_rows.begin() + row + (m_rows.size() - max_height());
        }
        return m_rows.begin() + row;
    }
    auto row_index(u32 row) const -> usize;
    auto get_row(u32 row) -> Row&;

    void drop_graphics_id(u16& id);
    void drop_hyperlink_id(u16& id);
    void drop_multi_cell_id(u16& id);

    /// This function does not remove the text associated with the cell, as the caller typically has enough
    /// context to do this more efficiently (because they are erasing multiple cells).
    void drop_cell(Cell& cell);

    // Screen state.
    di::Ring<Row> m_rows;
    IdMap<GraphicsRendition> m_graphics_renditions;
    IdMap<Hyperlink> m_hyperlinks;
    IdMap<MultiCellInfo> m_multi_cell_info;

    // Mutable state for writing cells.
    Cursor m_cursor;
    u16 m_graphics_id { 0 };
    u16 m_hyperlink_id { 0 };
    GraphicsRendition m_empty_graphics;

    // Terminal size information.
    Size m_size { 25, 80 };
};
}
