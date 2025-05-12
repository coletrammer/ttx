#include "ttx/renderer.h"

#include "di/container/tree/tree_set.h"
#include "di/io/vector_writer.h"
#include "di/io/writer_print.h"
#include "di/meta/constexpr.h"
#include "dius/print.h"
#include "dius/sync_file.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/escapes/osc_8.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/screen.h"

namespace ttx {
auto Renderer::setup(dius::SyncFile& output) -> di::Result<> {
    m_cleanup = {};

    auto buffer = di::VectorWriter<> {};

    // Setup - alternate screen buffer.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1049h"_sv);
    m_cleanup.push_back("\033[?1049l\033[?25h"_s);

    // Setup - disable autowrap.
    di::writer_print<di::String::Encoding>(buffer, "\033[?7l"_sv);
    m_cleanup.push_back("\033[?7h"_s);

    // Setup - kitty key mode.
    di::writer_print<di::String::Encoding>(buffer, "\033[>31u"_sv);
    m_cleanup.push_back("\033[<u"_s);

    // Setup - capture mouse events with shift held
    // Unfortunately, capturing shift mouse events prevents us from being able
    // to click on hyperlinks on Linux + Ghostty. On MacOS, command clicks can
    // be used. Other terminals ignore this escape anyway.
    // di::writer_print<di::String::Encoding>(buffer, "\033[>1s"_sv);
    // m_cleanup.push_back("\033[>0s"_s);

    // Setup - capture all mouse events and use SGR mosue reporting.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1003h\033[?1006h"_sv);
    m_cleanup.push_back("\033[?1006l\033[?1003l"_s);

    // Setup - enable focus events.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1004h"_sv);
    m_cleanup.push_back("\033[?1004l"_s);

    // Setup - bracketed paste.
    di::writer_print<di::String::Encoding>(buffer, "\033[?2004h"_sv);
    m_cleanup.push_back("\033[?2004l"_s);

    // Setup - grapheme cluster mode
    di::writer_print<di::String::Encoding>(buffer, "\033[?2027h"_sv);
    m_cleanup.push_back("\033[?2027l"_s);

    // Setup - ensure the current and desired screens are fully cleared.
    m_current_screen.clear();
    m_desired_screen.clear();
    m_desired_screen.clear_damage_tracking();
    m_current_cursor = {};
    m_size_changed = true;

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

auto Renderer::cleanup(dius::SyncFile& output) -> di::Result<> {
    auto buffer = di::VectorWriter<> {};
    for (auto const& string : m_cleanup | di::reverse) {
        di::writer_print<di::String::Encoding>(buffer, "{}"_sv, string);
    }
    m_cleanup.clear();

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

void Renderer::start(Size const& size) {
    // If the size has changed, we need to flush all our state.
    if (m_size_changed || this->size() != size) {
        m_size_changed = true;
        m_current_screen.resize(size);
        m_current_screen.clear();
        m_desired_screen.resize(size);
        m_desired_screen.clear();
        m_desired_screen.clear_damage_tracking();
        m_current_cursor = {};
    }
    // Otherwise, update the current screen state to the desired state.
    // We are assuming that the caller previously called end which would
    // actually write out the state. We do this here because it saves computation
    // when the size changes (as opposed to doing it in the end() function).
    else {
        ASSERT_EQ(m_desired_screen.absolute_row_start(), 0);
        ASSERT_EQ(m_desired_screen.size(), m_current_screen.size());
        auto [_, desired_row_group] = m_desired_screen.find_row(0);
        for (auto row_index : di::range(this->size().rows)) {
            for (auto desired : desired_row_group.iterate_row(row_index)) {
                auto [col, cell, text, gfx, hyperlink, multi_cell_info] = desired;
                if (cell.is_nonprimary_in_multi_cell()) {
                    continue;
                }
                m_current_screen.set_cursor(row_index, col);
                m_current_screen.set_current_graphics_rendition(gfx);
                m_current_screen.set_current_hyperlink(hyperlink);
                if (multi_cell_info == terminal::narrow_multi_cell_info) {
                    m_current_screen.put_single_cell(text, terminal::AutoWrapMode::Disabled);
                } else {
                    m_current_screen.put_wide_cell(text, multi_cell_info, terminal::AutoWrapMode::Disabled);
                }
            }
        }
    }

    // Reset bounding box.
    m_row_offset = 0;
    m_col_offset = 0;
    m_bound_width = size.cols;
    m_bound_height = size.rows;
}

// The pending changes are stored in the difference between the current and desired screens. We must translate only
// the relevant changes into terminal escape sequences and write ot the output. We first need to collect all
// modified cells and store them in grouped list. The grouping is chosen to minimize the amount of bytes written (so
// the order is hyperlink > graphics > cursor position).
struct Change {
    di::Optional<terminal::Hyperlink const&> hyperlink;
    GraphicsRendition const& graphics_rendition;
    u32 row { 0 };
    u32 col { 0 };
    di::StringView text;
    terminal::MultiCellInfo const& multi_cell_info;

    auto operator==(Change const& o) const -> bool {
        return di::tie(hyperlink, graphics_rendition, row, col) ==
               di::tie(o.hyperlink, o.graphics_rendition, o.row, o.col);
    };
    auto operator<=>(Change const& o) const {
        return di::tie(hyperlink, graphics_rendition, row, col) <=>
               di::tie(o.hyperlink, o.graphics_rendition, o.row, o.col);
    }
};

static void move_cursor(di::VectorWriter<>& buffer, terminal::Cursor const& current, terminal::Cursor const& desired) {
    // Optmizations: we want to move the cursor using the fewest number of bytes.
    // We have the following escapes available to us:
    //   \n  (C0) : row += 1
    //   \r  (C0) : col = 0
    //   CS  (C0) : col -= 1
    //   RI  (C1) : row -= 1
    //   CUU (CSI): row -= N
    //   CUD (CSI): row += N
    //   CUF (CSI): col += N
    //   CUB (CSI): col -= N
    //   CPL (CSI): row -= N, col = 0
    //   CNL (CSI): row += N, col = 0
    //   CUP (CSI): row = N+1, col = M+1
    //   CHA (CSI): col = N+1
    //   VPA (CSI): row = N+1

    // Instead of brute forcing the best sequence, which is possible but overly complex, we can some basic rules.
    // In particular, use \n and RI only when the row if off by 1. And use relative whenever possible, but falling
    // back on CUP.

    if (current.row == desired.row && current.col == desired.col) {
        return;
    }

    if (current.row == desired.row) {
        // Column only: Use \r if possible, otherwise relative positioning.
        if (desired.col == 0) {
            di::writer_print<di::String::Encoding>(buffer, "\r"_sv);
        } else if (desired.col + 1 == current.col) {
            // BS
            di::writer_print<di::String::Encoding>(buffer, "\x08"_sv);
        } else if (desired.col < current.col) {
            // CUB
            di::writer_print<di::String::Encoding>(buffer, "\033[{}D"_sv, current.col - desired.col);
        } else {
            // CUF
            di::writer_print<di::String::Encoding>(buffer, "\033[{}C"_sv, desired.col - current.col);
        }
        return;
    }

    if (current.col == desired.col) {
        // Row only: Use \n or RI if possible, otherwise relative movement.
        if (desired.row == current.row + 1) {
            di::writer_print<di::String::Encoding>(buffer, "\n"_sv);
        } else if (desired.row + 1 == current.row) {
            di::writer_print<di::String::Encoding>(buffer, "\033M"_sv);
        } else if (desired.row < current.row) {
            // CUU
            di::writer_print<di::String::Encoding>(buffer, "\033[{}A"_sv, current.row - desired.row);
        } else {
            // CUD
            di::writer_print<di::String::Encoding>(buffer, "\033[{}B"_sv, desired.row - current.row);
        }
        return;
    }

    if (desired.col == 0) {
        // Desired col = 0: Use CPL or CNL for relative movement.
        if (desired.row == current.row + 1) {
            di::writer_print<di::String::Encoding>(buffer, "\r\n"_sv);
        } else if (desired.row + 1 == current.row) {
            di::writer_print<di::String::Encoding>(buffer, "\r\033M"_sv);
        } else if (desired.row < current.row) {
            // CPL
            di::writer_print<di::String::Encoding>(buffer, "\033[{}F"_sv, current.row - desired.row);
        } else {
            // CNL
            di::writer_print<di::String::Encoding>(buffer, "\033[{}E"_sv, desired.row - current.row);
        }
        return;
    }

    // Row movement by 1: adjust the row and then adjust the column
    if (desired.row == current.row + 1) {
        move_cursor(buffer, { desired.row, current.col }, desired);
        di::writer_print<di::String::Encoding>(buffer, "\n"_sv);
        return;
    }
    if (desired.row + 1 == current.row) {
        move_cursor(buffer, { desired.row, current.col }, desired);
        di::writer_print<di::String::Encoding>(buffer, "\033M"_sv);
        return;
    }

    // Fallback:
    // CPA
    di::writer_print<di::String::Encoding>(buffer, "\033[{};{}H"_sv, desired.row + 1, desired.col + 1);
}

auto Renderer::finish(dius::SyncFile& output, RenderedCursor const& cursor) -> di::Result<> {
    auto changes = di::TreeSet<Change> {};

    ASSERT_EQ(m_current_screen.absolute_row_start(), 0);
    ASSERT_EQ(m_desired_screen.absolute_row_start(), 0);
    ASSERT_EQ(m_desired_screen.size(), m_current_screen.size());
    auto [_, current_row_group] = m_current_screen.find_row(0);
    auto [_, desired_row_group] = m_desired_screen.find_row(0);
    for (auto row_index : di::range(size().rows)) {
        for (auto [current, desired] :
             di::zip(current_row_group.iterate_row(row_index), desired_row_group.iterate_row(row_index))) {
            auto [col, current_cell, current_text, current_gfx, current_hyperlink, current_multi_cell_info] = current;
            auto [_, desired_cell, desired_text, desired_gfx, desired_hyperlink, desired_multi_cell_info] = desired;
            if (desired_cell.is_nonprimary_in_multi_cell()) {
                continue;
            }

            // Now detect a change. Order comparisons in order of likeliness.
            if (desired_text != current_text || desired_gfx != current_gfx || desired_hyperlink != current_hyperlink ||
                desired_multi_cell_info != current_multi_cell_info) {
                changes.emplace(desired_hyperlink, desired_gfx, row_index, col, desired_text, desired_multi_cell_info);
            }
        }
    }

    // Start sequence: hide the cursor, begin synchronized updaes, and reset graphics/hyperlink state.
    auto buffer = di::VectorWriter<> {};
    if (!changes.empty()) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?2026h"_sv);
        di::writer_print<di::String::Encoding>(buffer, "\033[?25l"_sv);
        di::writer_print<di::String::Encoding>(buffer, "\033[H"_sv);
        di::writer_print<di::String::Encoding>(buffer, "\033[m"_sv);
        di::writer_print<di::String::Encoding>(buffer, terminal::OSC8().serialize());
        if (di::exchange(m_size_changed, false)) {
            di::writer_print<di::String::Encoding>(buffer, "\033[2J"_sv);
        }
    } else if (cursor == m_current_cursor) {
        // No updates, so do nothing.
        return {};
    }

    // Now apply the changes.
    auto current_hyperlink = di::Optional<terminal::Hyperlink const&> {};
    auto current_gfx = GraphicsRendition {};
    auto current_cursor = terminal::Cursor {};
    for (auto [hyperlink, gfx, row, col, text, multi_cell_info] : changes) {
        if (current_hyperlink != hyperlink) {
            di::writer_print<di::String::Encoding>(buffer, terminal::OSC8::from_hyperlink(hyperlink).serialize());
            current_hyperlink = hyperlink;
        }
        if (current_gfx != gfx) {
            for (auto& params : gfx.as_csi_params()) {
                di::writer_print<di::String::Encoding>(buffer, "\033[{}m"_sv, params);
            }
            current_gfx = gfx;
        }
        if (current_cursor.row != row || current_cursor.col != col) {
            move_cursor(buffer, current_cursor, { row, col });
            current_cursor.row = row;
            current_cursor.col = col;
        }

        // TODO: use erase character if there is no background color.
        if (text == ""_sv) {
            text = " "_sv;
        }

        // TODO: multi cell info (this needs to handle different terminals).
        di::writer_print<di::String::Encoding>(buffer, "{}"_sv, text);

        current_cursor.col += multi_cell_info.compute_width();
        current_cursor.col = di::min(current_cursor.col, size().cols - 1);
    }

    // End sequence: potentially show the cursor, as well as end the synchronized output.
    if (!cursor.hidden) {
        di::writer_print<di::String::Encoding>(buffer, "\033[{};{}H"_sv, cursor.cursor_row + 1, cursor.cursor_col + 1);
        di::writer_print<di::String::Encoding>(buffer, "\033[{} q"_sv, i32(cursor.style));
        di::writer_print<di::String::Encoding>(buffer, "\033[?25h"_sv);
    }
    m_current_cursor = cursor;
    if (!changes.empty()) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?2026l"_sv);
    }

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

void Renderer::put_text(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    // TODO: have sane truncation behavior.
    row += m_row_offset;
    col += m_col_offset;

    m_desired_screen.set_cursor(row, col);
    m_desired_screen.set_current_graphics_rendition(rendition);
    m_desired_screen.set_current_hyperlink(hyperlink);
    for (auto ch : text) {
        m_desired_screen.put_code_point(ch, terminal::AutoWrapMode::Disabled);
        if (m_desired_screen.cursor().col >= m_col_offset + m_bound_width) {
            break;
        }
    }
}

void Renderer::put_text(c32 text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    auto string = di::container::string::StringImpl<di::String::Encoding, di::StaticVector<c8, di::Constexpr<4zu>>> {};
    (void) string.push_back(text);
    put_text(string.view(), row, col, rendition, hyperlink);
}

void Renderer::put_cell(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink,
                        terminal::MultiCellInfo const& multi_cell_info) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    // If the entire multi-cell doesn't fit, replace it with blanks.
    if (col + multi_cell_info.compute_width() > m_bound_width) {
        m_desired_screen.set_cursor(row + m_row_offset, col + m_col_offset);
        m_desired_screen.erase_characters(m_bound_width - col);
        return;
    }

    row += m_row_offset;
    col += m_col_offset;

    m_desired_screen.set_cursor(row, col);
    m_desired_screen.set_current_graphics_rendition(rendition);
    m_desired_screen.set_current_hyperlink(hyperlink);
    if (multi_cell_info == terminal::narrow_multi_cell_info) {
        m_desired_screen.put_single_cell(text, terminal::AutoWrapMode::Disabled);
    } else {
        m_desired_screen.put_wide_cell(text, multi_cell_info, terminal::AutoWrapMode::Disabled);
    }
}

void Renderer::clear_row(u32 row, GraphicsRendition const& rendition,
                         di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (row >= m_bound_height) {
        return;
    }

    for (auto c : di::range(m_bound_width)) {
        put_text(U' ', row, c, rendition, hyperlink);
    }
}

void Renderer::set_bound(u32 row, u32 col, u32 width, u32 height) {
    m_row_offset = row;
    m_col_offset = col;
    m_bound_width = width;
    m_bound_height = height;
}
}
