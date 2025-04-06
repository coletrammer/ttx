#pragma once

#include "di/container/ring/prelude.h"
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

    auto transfer_from(RowGroup& from, usize from_index, usize to_index, usize row_count,
                       di::Optional<u32> desired_cols = {}) -> usize;
    auto strip_trailing_empty_cells(usize row_index) -> usize;

    /// This function does not remove the text associated with the cell, as the caller typically has enough
    /// context to do this more efficiently (because they are erasing multiple cells).
    void drop_cell(Cell& cell);

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
