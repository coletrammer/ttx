#pragma once

#include "di/container/string/string_view.h"
#include "di/io/vector_writer.h"
#include "dius/sync_file.h"
#include "ttx/cursor_style.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/hyperlink.h"

namespace ttx {
struct RenderedCursor {
    u32 cursor_row { 0 };
    u32 cursor_col { 0 };
    CursorStyle style { CursorStyle::SteadyBlock };
    bool hidden { false };
};

class Renderer {
public:
    auto setup(dius::SyncFile& output) -> di::Result<>;
    auto cleanup(dius::SyncFile& output) -> di::Result<>;

    void start(Size const& size);
    auto finish(dius::SyncFile& output, RenderedCursor const& cursor) -> di::Result<>;

    void put_text(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});
    void put_text(c32 text, u32 row, u32 col, GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void put_cell(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void clear_row(u32 row, GraphicsRendition const& graphics_rendition = {},
                   di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void set_bound(u32 row, u32 col, u32 width, u32 height);

private:
    void move_cursor_to(u32 row, u32 col);
    void set_graphics_rendition(GraphicsRendition const& rendition);
    void set_hyperlink(di::Optional<terminal::Hyperlink const&> hyperlink);

    di::VectorWriter<> m_buffer;
    Size m_size;

    di::Vector<di::String> m_cleanup;

    GraphicsRendition m_last_graphics_rendition;
    di::Optional<terminal::Hyperlink> m_last_hyperlink;
    u32 m_last_cursor_row { 0 };
    u32 m_last_cursor_col { 0 };

    u32 m_row_offset { 0 };
    u32 m_col_offset { 0 };
    u32 m_bound_width { 0 };
    u32 m_bound_height { 0 };
};
}
