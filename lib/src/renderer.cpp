#include "ttx/renderer.h"

#include "di/io/writer_print.h"
#include "di/meta/constexpr.h"
#include "dius/print.h"
#include "dius/sync_file.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/escapes/osc_8.h"

namespace ttx {
void Renderer::start(Size const& size) {
    m_buffer = {};
    m_size = size;
    m_row_offset = 0;
    m_col_offset = 0;
    m_bound_width = size.cols;
    m_bound_height = size.rows;

    // Reset the cursor here since that way we don't have to worry about how window size
    // changes should affect things.
    m_last_cursor_row = 0;
    m_last_cursor_col = 0;
    m_last_graphics_rendition = {};
    m_last_hyperlink = {};

    di::writer_print<di::String::Encoding>(m_buffer, "\033[?2026h"_sv);
    di::writer_print<di::String::Encoding>(m_buffer, "\033[?25l"_sv);
    di::writer_print<di::String::Encoding>(m_buffer, "\033[H"_sv);
    di::writer_print<di::String::Encoding>(m_buffer, "\033[m"_sv);
    di::writer_print<di::String::Encoding>(m_buffer, terminal::OSC8().serialize());
}

auto Renderer::finish(dius::SyncFile& output, RenderedCursor const& cursor) -> di::Result<> {
    if (!cursor.hidden) {
        di::writer_print<di::String::Encoding>(m_buffer, "\033[{};{}H"_sv, cursor.cursor_row + 1,
                                               cursor.cursor_col + 1);
        di::writer_print<di::String::Encoding>(m_buffer, "\033[{} q"_sv, i32(cursor.style));
        di::writer_print<di::String::Encoding>(m_buffer, "\033[?25h"_sv);
    }

    di::writer_print<di::String::Encoding>(m_buffer, "\033[?2026l"_sv);

    auto text = di::move(m_buffer).vector();
    m_buffer = {};
    return output.write_exactly(di::as_bytes(text.span()));
}

void Renderer::put_text(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    // TODO: use an actual width measurement, using either grapheme glusters or east asian width.
    auto text_width = u32(di::distance(text));
    text_width = di::min(text_width, m_bound_width - col);

    auto truncated_text = di::take(text, text_width);

    row += m_row_offset;
    col += m_col_offset;
    move_cursor_to(row, col);
    set_graphics_rendition(rendition);
    set_hyperlink(hyperlink);

    for (auto code_point : truncated_text) {
        di::writer_print<di::String::Encoding>(m_buffer, "{}"_sv, code_point);
        m_last_cursor_col++;
    }
}

void Renderer::put_text(c32 text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    auto string = di::container::string::StringImpl<di::String::Encoding, di::StaticVector<c8, di::Constexpr<4zu>>> {};
    (void) string.push_back(text);
    put_text(string.view(), row, col, rendition, hyperlink);
}

void Renderer::put_cell(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    row += m_row_offset;
    col += m_col_offset;
    move_cursor_to(row, col);
    set_graphics_rendition(rendition);
    set_hyperlink(hyperlink);

    // NOTE: this assumes the text fits in a single cell., which may not be accurate...
    di::writer_print<di::String::Encoding>(m_buffer, "{}"_sv, text);
    m_last_cursor_col++;
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

void Renderer::move_cursor_to(u32 row, u32 col) {
    if (m_last_cursor_row != row || m_last_cursor_col != col) {
        m_last_cursor_row = row;
        m_last_cursor_col = col;
        di::writer_print<di::String::Encoding>(m_buffer, "\033[{};{}H"_sv, row + 1, col + 1);
    }
}

void Renderer::set_graphics_rendition(GraphicsRendition const& rendition) {
    if (m_last_graphics_rendition != rendition) {
        m_last_graphics_rendition = rendition;

        for (auto& params : rendition.as_csi_params()) {
            di::writer_print<di::String::Encoding>(m_buffer, "\033[{}m"_sv, params);
        }
    }
}

void Renderer::set_hyperlink(di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (m_last_hyperlink != hyperlink) {
        m_last_hyperlink = hyperlink.transform(di::clone);

        di::writer_print<di::String::Encoding>(m_buffer, terminal::OSC8::from_hyperlink(hyperlink).serialize());
    }
}

void Renderer::set_bound(u32 row, u32 col, u32 width, u32 height) {
    m_row_offset = row;
    m_col_offset = col;
    m_bound_width = width;
    m_bound_height = height;
}
}
