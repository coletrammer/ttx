#include "ttx/terminal/row_group.h"

#include "ttx/terminal/cell.h"

namespace ttx::terminal {
auto RowGroup::maybe_allocate_graphics_id(GraphicsRendition const& rendition) -> di::Optional<u16> {
    auto existing_id = graphics_id(rendition);
    if (!existing_id) {
        return allocate_graphics_id(rendition);
    }
    return use_graphics_id(existing_id.value());
}

auto RowGroup::maybe_allocate_hyperlink_id(Hyperlink const& hyperlink) -> di::Optional<u16> {
    auto existing_id = hyperlink_id(hyperlink.id);
    if (!existing_id) {
        return allocate_hyperlink_id(hyperlink.clone());
    }
    return use_hyperlink_id(existing_id.value());
}

void RowGroup::drop_graphics_id(u16& id) {
    if (id) {
        m_graphics_renditions.drop_id(id);
        id = 0;
    }
}

void RowGroup::drop_hyperlink_id(u16& id) {
    if (id) {
        m_hyperlinks.drop_id(id);
        id = 0;
    }
}

void RowGroup::drop_multi_cell_id(u16& id) {
    if (id) {
        m_multi_cell_info.drop_id(id);
        id = 0;
    }
}

void RowGroup::drop_cell(Cell& cell) {
    drop_graphics_id(cell.graphics_rendition_id);
    drop_hyperlink_id(cell.hyperlink_id);

    // TODO: clear the whole multi cell here.
    drop_multi_cell_id(cell.multicell_id);

    cell.stale = false;
}

auto RowGroup::graphics_rendition(u16 id) const -> GraphicsRendition const& {
    if (id == 0) {
        return m_empty_graphics;
    }
    return m_graphics_renditions.lookup_id(id);
}

auto RowGroup::hyperlink(u16 id) const -> Hyperlink const& {
    ASSERT_NOT_EQ(id, 0);
    return m_hyperlinks.lookup_id(id);
}

auto RowGroup::maybe_hyperlink(u16 id) const -> di::Optional<Hyperlink const&> {
    if (id == 0) {
        return {};
    }
    return hyperlink(id);
}

auto RowGroup::transfer_from(RowGroup& from, usize from_index, usize to_index, usize row_count,
                             di::Optional<u32> desired_cols) -> usize {
    auto& to = *this;
    ASSERT_LT_EQ(from_index + row_count, from.total_rows());
    ASSERT_LT_EQ(to_index, to.total_rows());

    to.rows().insert_container(to.rows().iterator(to_index), di::range(row_count) | di::transform([](auto) {
                                                                 return Row();
                                                             }));

    auto from_begin = from.rows().iterator(from_index);
    auto from_end = from_begin + isize(row_count);

    auto to_begin = to.rows().iterator(to_index);
    auto to_end = to_begin + isize(row_count);

    auto total_cells = 0_usize;
    for (auto [from_row, to_row] : di::zip(di::View(from_begin, from_end), di::View(to_begin, to_end))) {
        auto cols_to_take = desired_cols.value_or(from_row.cells.size());
        total_cells += cols_to_take;
        to_row.cells.resize(cols_to_take);

        auto text_size = 0_usize;
        for (auto [i, from_cell] : from_row.cells | di::enumerate) {
            // Insert the new cell as desired.
            auto& to_cell = to_row.cells[i];
            if (i < cols_to_take) {
                if (from_cell.graphics_rendition_id) {
                    to_cell.graphics_rendition_id =
                        to.maybe_allocate_graphics_id(from.graphics_rendition(from_cell.graphics_rendition_id))
                            .value_or(0);
                }
                if (from_cell.hyperlink_id) {
                    to_cell.hyperlink_id =
                        to.maybe_allocate_hyperlink_id(from.hyperlink(from_cell.hyperlink_id)).value_or(0);
                }
                to_cell.text_size = from_cell.text_size;

                // TODO: multi cells
            }

            // Always drop the cell `from` cell. We don't have to worry about clearing the associated text since the
            // entire row is deleted later.
            text_size += from_cell.text_size;
            from.drop_cell(from_cell);
        }

        // Copy over row attributes (overflow flag and text). Text is moved over to prevent
        // copying, but has to be truncated if desired cols is smaller than the actual row size.
        to_row.overflow = from_row.overflow;
        to_row.text = di::move(from_row.text);
        auto text_end = to_row.text.iterator_at_offset(text_size);
        ASSERT(text_end);
        to_row.text.erase(text_end.value(), to_row.text.end());
    }

    // Erase all desired rows in `from`.
    from.rows().erase(from_begin, from_end);

    return total_cells;
}

auto RowGroup::strip_trailing_empty_cells(usize row_index) -> usize {
    ASSERT_LT(row_index, total_rows());

    auto& row = m_rows[row_index];
    if (row.overflow) {
        return row.cells.size();
    }

    // Ensure all rows have at least size 1, as otherwise our cell based scroll back
    // limit breaks down (you could have an unbounded number of blank lines).
    while (row.cells.size() > 1) {
        if (row.cells.back().value().is_empty()) {
            row.cells.pop_back();
            continue;
        }
        break;
    }
    return row.cells.size();
}
}
