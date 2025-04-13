#pragma once

#include "di/bit/bitset/prelude.h"
#include "di/container/string/string_view.h"
#include "di/container/view/cache_last.h"
#include "di/reflect/prelude.h"
#include "scroll_region.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/hyperlink.h"
#include "ttx/terminal/row.h"
#include "ttx/terminal/row_group.h"
#include "ttx/terminal/scroll_back.h"
#include "ttx/terminal/selection.h"

namespace ttx::terminal {
/// @brief Whether or not auto-wrap (DEC mode 7) is enabled.
enum class AutoWrapMode {
    Disabled,
    Enabled,
};

/// @brief Whether or not origin mode (DEC mode 6) is enabled.
///
/// When origin mode is enabled, the cursor is constrained to be
/// within the scroll region of the screen. Additionally, all row
/// and column indicies are relative to the top-left of the scroll
/// region when origin is enabled.
enum class OriginMode {
    Disabled,
    Enabled,
};

/// @brief Represents the saved cursor state, which is used for save/restore cursor operations.
///
/// The attributes which are saved and restored are defined in the [manual](https://vt100.net/docs/vt510-rm/DECSC.html)
/// for the DECSC escape sequence.
struct SavedCursor {
    u32 row { 0 };                           ///< Row (y coordinate)
    u32 col { 0 };                           ///< Column (x coordinate)
    bool overflow_pending { false };         ///< Signals that the previous text outputted reached the end of a row.
    GraphicsRendition graphics_rendition {}; ///< Active graphics rendition
    OriginMode origin_mode { OriginMode::Disabled }; ///< Origin mode

    auto operator==(SavedCursor const&) const -> bool = default;
    auto operator<=>(SavedCursor const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<SavedCursor>) {
        return di::make_fields<"SavedCursor">(di::field<"row", &SavedCursor::row>, di::field<"col", &SavedCursor::col>,
                                              di::field<"overflow_pending", &SavedCursor::overflow_pending>,
                                              di::field<"graphics_rendition", &SavedCursor::graphics_rendition>,
                                              di::field<"origin_mode", &SavedCursor::origin_mode>);
    }
};

/// @brief Represents the visible contents of the terminal (with no scroll back)
///
/// The screen class internally maintains a vector of terminal rows, as well as
/// the graphics rendition, hyperlink, and multi cell info indirectly referenced by the
/// Cell class.
///
/// When the screen scrolls due to autowrapping, when scroll back is enabled scrolled
/// rows will be moved to a separate vector of rows. The caller is expected to
/// evenually migrate these rows into a different storage location.
class Screen {
public:
    enum class ScrollBackEnabled { No, Yes };

    explicit Screen(Size const& size, ScrollBackEnabled scroll_back_enabled);

    void resize(Size const& size);
    void set_scroll_region(ScrollRegion const& region);

    auto max_height() const -> u32 { return m_size.rows; }
    auto max_width() const -> u32 { return m_size.cols; }
    auto size() const -> Size const& { return m_size; }
    auto scroll_region() const -> ScrollRegion const& { return m_scroll_region; }

    auto current_graphics_rendition() const -> GraphicsRendition const&;
    auto current_hyperlink() const -> di::Optional<Hyperlink const&>;

    void set_current_graphics_rendition(GraphicsRendition const& rendition);
    void set_current_hyperlink(di::Optional<Hyperlink const&> hyperlink);

    auto cursor() const -> Cursor { return m_cursor; }
    auto origin_mode() const -> OriginMode { return m_origin_mode; }

    auto save_cursor() const -> SavedCursor;
    void restore_cursor(SavedCursor const& cursor);

    void set_origin_mode(OriginMode mode);
    void set_cursor_relative(u32 row, u32 col);
    void set_cursor(u32 row, u32 col);
    void set_cursor(u32 row, u32 col, bool overflow_pending);
    void set_cursor_row_relative(u32 row);
    void set_cursor_row(u32 row);
    void set_cursor_col_relative(u32 col);
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
    void erase_characters(u32 n);

    void scroll_down();
    void put_code_point(c32 code_point, AutoWrapMode auto_wrap_mode);

    void invalidate_all() { m_whole_screen_dirty = true; }
    auto whole_screen_dirty() const -> bool { return m_whole_screen_dirty; }
    void clear_whole_screen_dirty_flag() { m_whole_screen_dirty = false; }

    auto absolute_row_start() const -> u64 { return m_scroll_back.absolute_row_start(); }
    auto absolute_row_screen_start() const -> u64 { return m_scroll_back.absolute_row_end(); }
    auto absolute_row_end() const -> u64 { return absolute_row_screen_start() + max_height(); }
    auto total_rows() const -> usize { return m_scroll_back.total_rows() + m_active_rows.total_rows(); }

    void clear_scroll_back();

    auto visual_scroll_offset() const -> u64 {
        ASSERT_GT_EQ(m_visual_scroll_offset, absolute_row_start());
        ASSERT_LT_EQ(m_visual_scroll_offset, absolute_row_screen_start());
        return m_visual_scroll_offset;
    }
    auto visual_scroll_at_bottom() const -> bool { return visual_scroll_offset() == absolute_row_screen_start(); }
    void visual_scroll_up();
    void visual_scroll_down();
    void visual_scroll_to_bottom();

    auto selection() const -> di::Optional<Selection> { return m_selection; }
    void clear_selection();
    void begin_selection(SelectionPoint const& point);
    void update_selection(SelectionPoint const& point);
    auto in_selection(SelectionPoint const& point) const -> bool;
    auto selected_text() const -> di::String;

    auto find_row(u64 row) const -> di::Tuple<u32, RowGroup const&>;
    auto iterate_row(u64 row) const {
        auto [r, group] = find_row(row);
        return group.iterate_row(r);
    }

    /// @brief Serialize screen contents to terminal escape sequences
    ///
    /// This function does not preserve attributes like the visual scroll
    /// offset and current selection. Additionally, the terminal size is
    /// not embedded in the returned escape sequences. The
    /// Terminal::state_as_escape_sequences() includes a superset of
    /// information, including the size and dec modes.
    auto state_as_escape_sequences() const -> di::String;

private:
    void put_single_cell(di::StringView text, AutoWrapMode auto_wrap_mode);

    // Row/column helper functions for dealing with origin mode.
    auto translate_row(u32 row) const -> u32;
    auto translate_col(u32 col) const -> u32;
    auto min_row() const -> u32;
    auto max_row_inclusive() const -> u32;
    auto min_col() const -> u32;
    auto max_col_inclusive() const -> u32;

    auto cursor_in_scroll_region() const -> bool;

    // Clamp the selection to be within bounds, after adding/removing rows from scrollback.
    void clamp_selection();

    auto begin_row_iterator() { return rows().begin() + m_scroll_region.start_row; }
    auto end_row_iterator() { return rows().begin() + m_scroll_region.end_row; }

    auto rows() -> di::Ring<Row>& { return m_active_rows.rows(); }
    auto rows() const -> di::Ring<Row> const& { return m_active_rows.rows(); }

    // Screen state.
    RowGroup m_active_rows;
    bool m_whole_screen_dirty { true };

    // Scroll back
    ScrollBack m_scroll_back;
    ScrollBackEnabled m_scroll_back_enabled { ScrollBackEnabled::No };
    u64 m_visual_scroll_offset { 0 };
    bool m_never_got_input { true };

    // Visual selection
    di::Optional<Selection> m_selection;

    // Mutable state for writing cells.
    Cursor m_cursor;
    OriginMode m_origin_mode { OriginMode::Disabled };
    u16 m_graphics_id { 0 };
    u16 m_hyperlink_id { 0 };

    // Terminal size information.
    Size m_size {};
    ScrollRegion m_scroll_region;
};
}
