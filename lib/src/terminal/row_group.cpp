#include "ttx/terminal/row_group.h"

#include "di/container/algorithm/find_last_if_not.h"
#include "ttx/terminal/cell.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/reflow_result.h"

namespace ttx::terminal {
auto RowGroup::reflow(u64 absolute_row_start, u32 target_width) -> ReflowResult {
    DI_ASSERT_GT(target_width, 0);

    auto result = ReflowResult {};
    auto new_rows = di::Ring<Row> {};

    auto original_row_index = 0_u64;

    auto compute_dr = [&] {
        // The dr component is computed by determining the offset between the row after reflow
        // and the row before reflow. Since we use a count for the new rows and an index for the
        // overflow, we must adjust the delta by 1.
        return i64(new_rows.size()) - i64(original_row_index) - 1;
    };

    // First, we chunk each physical row into logical rows. Logical rows are a continuous
    // sequence of rows where all but the last has Row::overflow set to true.
    for (auto chunk : m_rows | di::chunk_by([](Row const& a, Row const&) {
                          return a.overflow;
                      })) {
        // Now we just take text from each row in the group greedily to fill new rows satisfying the
        // new target width. We get to keep the same metadata IDs as the original cells, but the text
        // offset does change. Each chunk will have at least 1 row.
        new_rows.emplace_back();
        result.add_offset({ absolute_row_start + original_row_index, 0 }, compute_dr(), 0);
        auto current_width = 0_u32;
        for (auto& row : chunk) {
            auto text_offset = 0_usize;
            auto [end, _] = di::find_last_if_not(row.cells, &Cell::is_empty);
            auto effective_width = row.cells.end() == end ? 0_usize : usize(end - row.cells.begin() + 1);
            auto need_mapping = false;

            for (auto col = 0_u32; col < effective_width;) {
                auto& cell = row.cells[col];
                auto width = multi_cell_info(cell.multi_cell_id).compute_width();

                auto _ = di::ScopeExit([&] {
                    col += width;
                    text_offset += cell.text_size;
                });

                // If the target width is smaller than the cell width we actually have to drop this cell. For this case
                // we need a special mapping to signify the cells have been deleted.
                if (width > target_width) {
                    auto current_output_col = new_rows.back()
                                                  .transform([](Row const& row) {
                                                      return i32(row.cells.size());
                                                  })
                                                  .value_or(0);
                    result.add_offset({ absolute_row_start + original_row_index, col }, compute_dr(),
                                      current_output_col, true);
                    for (auto i = 0_u8; i < width; i++) {
                        drop_cell(row.cells[col + i]);
                    }
                    need_mapping = true;
                    continue;
                }

                // We must make a new row if we overflow the width of the current row we're building.
                if (current_width + width > target_width) {
                    current_width = 0;
                    new_rows.back().value().overflow = true;
                    new_rows.emplace_back();

                    result.add_offset({ absolute_row_start + original_row_index, col }, compute_dr(), -i32(col));
                } else if (col == 0 && !new_rows.back().value().cells.empty()) {
                    // Since we're adding to an existing row but we're the start of an original row,
                    // we need an updated displacement.
                    result.add_offset({ absolute_row_start + original_row_index, 0 }, compute_dr(),
                                      i32(new_rows.back().value().cells.size()));
                } else if (need_mapping) {
                    result.add_offset({ absolute_row_start + original_row_index, col }, compute_dr(),
                                      i32(new_rows.back().value().cells.size()) - i32(col));
                }
                need_mapping = false;

                // Now we just need to append the current group of cells to the row we're building.
                auto& new_row = new_rows.back().value();
                new_row.cells.append_container(row.cells.subspan(col, width).value());
                current_width += width;

                // Append the relevant text.
                auto text_start = row.text.iterator_at_offset(text_offset);
                auto text_end = row.text.iterator_at_offset(text_offset + cell.text_size);
                ASSERT(text_start);
                ASSERT(text_end);
                auto text = row.text.substr(text_start.value(), text_end.value());
                new_row.text.append(text);
            }

            original_row_index++;
        }
    }

    // Add a final mapping for everything that comes after this row group.
    result.add_offset({ absolute_row_start + original_row_index, 0 }, i64(new_rows.size()) - i64(total_rows()), 0);

    // Preserve the pending flag on the last row of the group.
    auto original_last_row_pending = m_rows.back().transform(&Row::overflow).value_or(false);
    new_rows.back().transform([&](Row& row) {
        row.overflow = original_last_row_pending;
    });

    m_rows = di::move(new_rows);
    return result;
}

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

auto RowGroup::maybe_allocate_multi_cell_id(MultiCellInfo const& multi_cell_info) -> di::Optional<u16> {
    auto existing_id = multi_cell_id(multi_cell_info);
    if (!existing_id) {
        return allocate_multi_cell_id(multi_cell_info);
    }
    return use_multi_cell_id(existing_id.value());
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
        if (id > 1) {
            m_multi_cell_info.drop_id(id);
        }
        id = 0;
    }
}

void RowGroup::drop_cell(Cell& cell) {
    auto was_empty = cell.is_empty();
    if (cell.has_ids()) {
        drop_graphics_id(cell.ids.graphics_rendition_id);
        drop_hyperlink_id(cell.ids.hyperlink_id);
    } else {
        cell.ids = {};
    }
    drop_multi_cell_id(cell.multi_cell_id);

    cell.background_only = false;
    cell.left_boundary_of_multicell = false;
    cell.explicitly_sized = false;
    cell.complex_grapheme_cluster = false;
    cell.stale = cell.stale && was_empty;
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

auto RowGroup::multi_cell_info(u16 id) const -> MultiCellInfo const& {
    if (id == 0) {
        return narrow_multi_cell_info;
    }
    if (id == 1) {
        return wide_multi_cell_info;
    }
    return m_multi_cell_info.lookup_id(id);
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
        total_cells += cols_to_take > 0 ? cols_to_take : 1; // Consider empty rows to have 1 cell.
        to_row.cells.resize(cols_to_take);

        // Truncate any wide cells if they won't fully fit into the requested column size.
        auto from_cells_to_take = di::min(cols_to_take, u32(from_row.cells.size()));
        if (from_cells_to_take > 0 && from_cells_to_take < from_row.cells.size()) {
            // Check if this multi cell would be truncated.
            if (from_row.cells[from_cells_to_take - 1].is_multi_cell() &&
                from_row.cells[from_cells_to_take].is_nonprimary_in_multi_cell()) {
                while (from_cells_to_take > 0 && from_row.cells[from_cells_to_take - 1].is_nonprimary_in_multi_cell()) {
                    from_cells_to_take--;
                }
                if (from_cells_to_take > 0) {
                    from_cells_to_take--;
                }
            }
        }

        auto text_size = 0_usize;
        for (auto [i, cells] : di::zip(from_row.cells | di::take(from_cells_to_take), to_row.cells) | di::enumerate) {
            // Insert the new cell as desired.
            auto& [from_cell, to_cell] = cells;
            if (i < cols_to_take) {
                to_cell = from_cell;
                if (from_cell.has_ids()) {
                    if (from_cell.ids.graphics_rendition_id) {
                        to_cell.ids.graphics_rendition_id =
                            to.maybe_allocate_graphics_id(from.graphics_rendition(from_cell.ids.graphics_rendition_id))
                                .value_or(0);
                    }
                    if (from_cell.ids.hyperlink_id) {
                        to_cell.ids.hyperlink_id =
                            to.maybe_allocate_hyperlink_id(from.hyperlink(from_cell.ids.hyperlink_id)).value_or(0);
                    }
                }
                if (from_cell.multi_cell_id) {
                    to_cell.multi_cell_id =
                        to.maybe_allocate_multi_cell_id(from.multi_cell_info(from_cell.multi_cell_id)).value_or(0);
                }
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

    while (row.cells.size() > 1) {
        if (row.cells.back().value().is_empty()) {
            row.cells.pop_back();
            continue;
        }
        break;
    }

    // Ensure all rows have are considered at least size 1, as otherwise our cell based scroll back
    // limit breaks down (you could have an unbounded number of blank lines).
    return row.cells.empty() ? 1 : row.cells.size();
}
}
