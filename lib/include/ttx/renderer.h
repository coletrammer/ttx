#pragma once

#include "di/container/string/string_view.h"
#include "di/util/scope_exit.h"
#include "dius/sync_file.h"
#include "ttx/clipboard.h"
#include "ttx/cursor_style.h"
#include "ttx/features.h"
#include "ttx/size.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/graphics_rendition.h"
#include "ttx/terminal/hyperlink.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/palette.h"
#include "ttx/terminal/screen.h"

namespace ttx {
struct RenderedCursor {
    u32 cursor_row { 0 };
    u32 cursor_col { 0 };
    CursorStyle style { CursorStyle::SteadyBlock };
    terminal::Color color {};
    terminal::Color text_color {};
    bool hidden { false };

    auto operator==(RenderedCursor const&) const -> bool = default;
};

class Renderer {
public:
    Renderer()
        : m_current_screen({ 24, 80 }, terminal::Screen::ScrollBackEnabled::No)
        , m_desired_screen({ 24, 80 }, terminal::Screen::ScrollBackEnabled::No) {}

    auto setup(dius::SyncFile& output, Feature features, ClipboardMode clipboard_mode) -> di::Result<>;
    auto cleanup(dius::SyncFile& output) -> di::Result<>;

    void start(Size const& size);
    auto finish(dius::SyncFile& output, RenderedCursor const& cursor_in) -> di::Result<>;

    void put_text(di::StringView text, u32 row, u32 col, terminal::GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});
    void put_text(c32 text, u32 row, u32 col, terminal::GraphicsRendition const& rendition = {},
                  di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void put_cell(di::StringView text, u32 row, u32 col, terminal::GraphicsRendition const& rendition,
                  di::Optional<terminal::Hyperlink const&> hyperlink, terminal::MultiCellInfo const& multi_cell_info,
                  bool explicitly_sized, bool complex_grapheme_cluster);

    void clear_row(u32 row, terminal::GraphicsRendition const& graphics_rendition = {},
                   di::Optional<terminal::Hyperlink const&> hyperlink = {});

    void set_bound(u32 row, u32 col, u32 width, u32 height);

    [[nodiscard]] auto set_local_palette(terminal::Palette const& palette) -> di::ScopeExit<di::Function<void()>> {
        m_local_palette = palette;
        return di::ScopeExit(di::make_function<void()>([this] {
            reset_local_palette();
        }));
    }
    void reset_local_palette() { m_local_palette = {}; }

    [[nodiscard]] auto set_global_palette(terminal::Palette const& palette) -> di::ScopeExit<di::Function<void()>> {
        m_global_palette = palette;
        return di::ScopeExit(di::make_function<void()>([this] {
            reset_global_palette();
        }));
    }
    void reset_global_palette() { m_global_palette = {}; }

    auto resolve_color(terminal::PaletteIndex index) const -> terminal::Color;
    auto resolve_color(terminal::Color color) const -> terminal::Color;
    auto resolve_foreground(terminal::Color color) const -> terminal::Color;
    auto resolve_background(terminal::Color color) const -> terminal::Color;
    auto resolve_cursor_color() const -> terminal::Color;
    auto resolve_cursor_text_color() const -> terminal::Color;

private:
    auto size() const -> Size { return m_current_screen.size(); }

    auto resolve_rendition(terminal::GraphicsRendition const& rendition) const -> terminal::GraphicsRendition;

    terminal::Screen m_current_screen;
    terminal::Screen m_desired_screen;
    di::Optional<RenderedCursor> m_current_cursor;
    di::Vector<di::String> m_cleanup;
    Feature m_features { Feature::None };
    di::Optional<terminal::Palette const&> m_global_palette;
    di::Optional<terminal::Palette const&> m_local_palette;

    u32 m_row_offset { 0 };
    u32 m_col_offset { 0 };
    u32 m_bound_width { 0 };
    u32 m_bound_height { 0 };
    bool m_size_changed { true };
};
}
