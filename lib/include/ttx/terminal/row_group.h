#pragma once

#include "di/container/ring/prelude.h"
#include "di/container/view/cache_last.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/hyperlink.h"
#include "ttx/terminal/id_map.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/row.h"

namespace ttx::terminal {
/// @brief Represents a group of terminal rows
///
/// An invidual screen will have 1 active row group
/// and potentially several additional row groups which
/// store the scroll back history. Within a row group,
/// attributes like the graphics rendition and hyperlink
/// information are deduplicated using an IdMap.
///
/// Internally, rows are stored in a ring buffer to speed
/// up certain operations during resizing (specifically
/// inserting rows at the start of the group).
class RowGroup {
public:
    explicit RowGroup() {
        // Ensure that multi cell ID 1 is the standard wide cell. This allows for
        // fast path optimizations.
        ASSERT_EQ(1, m_multi_cell_info.allocate({ wide_multi_cell_info }));
    }

    auto rows() -> di::Ring<Row>& { return m_rows; }
    auto rows() const -> di::Ring<Row> const& { return m_rows; }

    auto empty() const -> bool { return m_rows.empty(); }
    auto total_rows() const { return m_rows.size(); }

    void drop_graphics_id(u16& id);
    void drop_hyperlink_id(u16& id);
    void drop_multi_cell_id(u16& id);

    auto graphics_rendition(u16 id) const -> GraphicsRendition const&;
    auto hyperlink(u16 id) const -> Hyperlink const&;
    auto maybe_hyperlink(u16 id) const -> di::Optional<Hyperlink const&>;
    auto multi_cell_info(u16 id) const -> MultiCellInfo const&;

    auto graphics_id(GraphicsRendition const& rendition) -> di::Optional<u16> {
        if (rendition == GraphicsRendition {}) {
            return 0;
        }
        return m_graphics_renditions.lookup_key(rendition);
    }
    auto use_graphics_id(u16 id) -> u16 {
        if (!id) {
            return 0;
        }
        return m_graphics_renditions.use_id(id);
    }
    auto allocate_graphics_id(GraphicsRendition const& rendition) -> di::Optional<u16> {
        return m_graphics_renditions.allocate(rendition);
    }
    auto maybe_allocate_graphics_id(GraphicsRendition const& rendition) -> di::Optional<u16>;

    auto hyperlink_id(di::String const& hyperlink_id) -> di::Optional<u16> {
        return m_hyperlinks.lookup_key(hyperlink_id);
    }
    auto use_hyperlink_id(u16 id) -> u16 {
        if (!id) {
            return 0;
        }
        return m_hyperlinks.use_id(id);
    }
    auto allocate_hyperlink_id(Hyperlink&& hyperlink) -> di::Optional<u16> {
        return m_hyperlinks.allocate(di::move(hyperlink));
    }
    auto maybe_allocate_hyperlink_id(Hyperlink const& hyperlink) -> di::Optional<u16>;

    auto multi_cell_id(MultiCellInfo const& multi_cell_info) -> di::Optional<u16> {
        if (multi_cell_info == narrow_multi_cell_info) {
            return 0;
        }
        if (multi_cell_info == wide_multi_cell_info) {
            return 1;
        }
        return m_multi_cell_info.lookup_key(multi_cell_info);
    }
    auto use_multi_cell_id(u16 id) -> u16 {
        if (id <= 1) {
            return id;
        }
        return m_multi_cell_info.use_id(id);
    }
    auto allocate_multi_cell_id(MultiCellInfo const& multi_cell_info) -> di::Optional<u16> {
        return m_multi_cell_info.allocate(multi_cell_info);
    }
    auto maybe_allocate_multi_cell_id(MultiCellInfo const& multi_cell_info) -> di::Optional<u16>;

    auto transfer_from(RowGroup& from, usize from_index, usize to_index, usize row_count,
                       di::Optional<u32> desired_cols = {}) -> usize;
    auto strip_trailing_empty_cells(usize row_index) -> usize;

    /// This function does not remove the text associated with the cell, as the caller typically has enough
    /// context to do this more efficiently (because they are erasing multiple cells).
    void drop_cell(Cell& cell);

    auto iterate_row(u32 row) const {
        ASSERT_LT(row, total_rows());

        auto const& row_object = rows()[row];

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

                   return di::make_tuple(
                       col++, di::ref(cell), text, di::ref(graphics_rendition(cell.graphics_rendition_id)),
                       maybe_hyperlink(cell.hyperlink_id), di::ref(multi_cell_info(cell.multi_cell_id)));
               }) |
               di::cache_last;
    }

private:
    di::Ring<Row> m_rows;
    IdMap<GraphicsRendition> m_graphics_renditions;
    IdMap<Hyperlink> m_hyperlinks;
    IdMap<MultiCellInfo> m_multi_cell_info;

    // This empty graphics rendition is used as an optimization so that we can
    // return by reference in graphics_rendition() when the id == 0.
    GraphicsRendition m_empty_graphics;
};
}
