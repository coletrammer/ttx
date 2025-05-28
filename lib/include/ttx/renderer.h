#pragma once

#include "di/container/string/string_view.h"
#include "dius/sync_file.h"
#include "ttx/cursor_style.h"
#include "ttx/features.h"
#include "ttx/graphics_rendition.h"
#include "ttx/size.h"
#include "ttx/terminal/hyperlink.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/screen.h"

namespace ttx {
struct RenderedCursor {
    u32 cursor_row { 0 };
    u32 cursor_col { 0 };
    CursorStyle style { CursorStyle::SteadyBlock };
    bool hidden { false };

    auto operator==(RenderedCursor const&) const -> bool = default;
};

class Renderer {
public:
    Renderer()
        : m_current_screen({ 24, 80 }, terminal::Screen::ScrollBackEnabled::No)
        , m_desired_screen({ 24, 80 }, terminal::Screen::ScrollBackEnabled::No) {}

    auto setup(dius::SyncFile& output, Feature features) -> di::Result<>;
    auto cleanup(dius::SyncFile& output) -> di::Result<>;

    void start(Size const& size);
    auto finish(dius::SyncFile& output, RenderedCursor const& cursor_in) -> di::Result<>;

    void put_text(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});
    void put_text(c32 text, u32 row, u32 col, GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void put_cell(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                  di::Optional<terminal::Hyperlink const&> hyperlink, terminal::MultiCellInfo const& multi_cell_info,
                  bool explicitly_sized, bool complex_grapheme_cluster);

    void clear_row(u32 row, GraphicsRendition const& graphics_rendition = {},
                   di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void set_bound(u32 row, u32 col, u32 width, u32 height);

private:
    auto size() const -> Size { return m_current_screen.size(); }

    terminal::Screen m_current_screen;
    terminal::Screen m_desired_screen;
    di::Optional<RenderedCursor> m_current_cursor;
    di::Vector<di::String> m_cleanup;
    Feature m_features { Feature::None };

    u32 m_row_offset { 0 };
    u32 m_col_offset { 0 };
    u32 m_bound_width { 0 };
    u32 m_bound_height { 0 };
    bool m_size_changed { true };
};
}
